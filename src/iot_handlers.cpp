/**
 * @file iot_handlers.cpp
 * @brief IoT Data Handlers Implementation
 */

#include "iot_handlers.h"
#include "iot_data_structures.h"
#include "config.h"
#include "sensor_manager.h"
#include "relay_controller.h"
#include "display_manager.h"
#include "button_handler.h"
#include <ArduinoIoTDevice.h>

// External references to global objects (defined in main.cpp)
extern IoTDevice<WaterTankDeviceConfig, WaterTankControlData, WaterTankTelemetryData> iotDevice;
extern SensorManager sensorManager;
extern RelayController relayController;
extern DisplayManager displayManager;
extern ButtonHandler buttonHandler;

// State tracking variables
static float previousWaterHeight = 0.0f;
static unsigned long previousTime = 0;
static int displayScreenMode = 0;  // 0=Status, 1=Network, 2=Settings

// ============================================================================
// SENSOR READING
// ============================================================================

void readSensors() {
    // Read ultrasonic sensor
    float distance = sensorManager.readDistance();

    // Calculate water height and level
    float tankHeight = iotDevice.deviceConfig.tankHeight.value;
    float waterHeight = tankHeight - distance;
    waterHeight = constrain(waterHeight, 0.0f, tankHeight);

    float waterLevel = (waterHeight / tankHeight) * 100.0f;

    // Calculate inflow rate
    float inflowRate = calculateInflowRate(waterHeight);

    // Update telemetry data (library will auto-upload)
    iotDevice.telemetryData.waterLevel.value = waterLevel;
    iotDevice.telemetryData.waterHeight.value = waterHeight;
    iotDevice.telemetryData.currInflow.value = inflowRate;
    iotDevice.telemetryData.pumpStatus.value = relayController.isPumpOn() ? 1 : 0;

    // System telemetry (always 1 when device is running)
    iotDevice.systemTelemetry.Status.value = 1;

    // Update previous values
    previousWaterHeight = waterHeight;

    DEBUG_PRINTF("[Sensor] Level: %.1f%% (%.1fcm), Inflow: %.1fL/min, Pump: %s\n",
                waterLevel, waterHeight, inflowRate,
                relayController.isPumpOn() ? "ON" : "OFF");
}

float calculateInflowRate(float currentHeight) {
    unsigned long currentTime = millis();
    float timeDiff = (currentTime - previousTime) / 1000.0f;  // seconds

    if (timeDiff < 0.1f || previousTime == 0) {
        return iotDevice.telemetryData.currInflow.value;  // Return previous value
    }

    // Calculate height change
    float heightChange = currentHeight - previousWaterHeight;

    // Calculate volume change based on tank shape
    float volumeChange = 0;  // in cm³
    String tankShape = iotDevice.deviceConfig.tankShape.value;
    float tankWidth = iotDevice.deviceConfig.tankWidth.value;

    if (tankShape == "Cylindrical") {
        float radius = tankWidth / 2.0f;
        volumeChange = 3.14159f * radius * radius * heightChange;
    } else if (tankShape == "Rectangular") {
        volumeChange = tankWidth * tankWidth * heightChange;
    }

    // Convert cm³ to liters
    float volumeLiters = volumeChange / 1000.0f;

    // Calculate flow rate in L/min
    float flowRate = (volumeLiters / timeDiff) * 60.0f;

    // Constrain to reasonable values
    float maxInflow = iotDevice.deviceConfig.maxInflow.value;
    flowRate = constrain(flowRate, -maxInflow, maxInflow);

    // Update time
    previousTime = currentTime;

    return flowRate;
}

// ============================================================================
// PUMP CONTROL LOGIC
// ============================================================================

void controlPump() {
    bool shouldPumpBeOn = false;

    // Check control mode
    if (iotDevice.controlData.autoMode.value) {
        // Auto mode - threshold-based control
        float waterLevel = iotDevice.telemetryData.waterLevel.value;
        float upperThreshold = iotDevice.deviceConfig.upperThreshold.value;
        float lowerThreshold = iotDevice.deviceConfig.lowerThreshold.value;

        if (waterLevel <= lowerThreshold && !relayController.isPumpOn()) {
            shouldPumpBeOn = true;
            DEBUG_PRINTF("[Control] Auto: Level %.1f%% <= %.1f%%, pump ON\n",
                        waterLevel, lowerThreshold);
        } else if (waterLevel >= upperThreshold && relayController.isPumpOn()) {
            shouldPumpBeOn = false;
            DEBUG_PRINTF("[Control] Auto: Level %.1f%% >= %.1f%%, pump OFF\n",
                        waterLevel, upperThreshold);
        } else {
            // Maintain current state (hysteresis)
            shouldPumpBeOn = relayController.isPumpOn();
        }
    } else {
        // Manual mode - use server/app command
        shouldPumpBeOn = iotDevice.controlData.pumpSwitch.value;
    }

    // Update pump state
    if (shouldPumpBeOn != relayController.isPumpOn()) {
        if (shouldPumpBeOn) {
            relayController.turnOn();
        } else {
            relayController.turnOff();
        }
    }
}

// ============================================================================
// DISPLAY UPDATE
// ============================================================================

void updateDisplay() {
    float waterLevel = iotDevice.telemetryData.waterLevel.value;
    float inflow = iotDevice.telemetryData.currInflow.value;
    bool pumpOn = relayController.isPumpOn();
    bool autoMode = iotDevice.controlData.autoMode.value;

    switch (displayScreenMode) {
        case 0:  // Status screen
            displayManager.showWaterLevel(waterLevel, inflow, pumpOn, autoMode);
            break;
        case 1:  // Network screen
            displayManager.showNetworkInfo(
                iotDevice.isWiFiConnected() ? "Connected" : "Disconnected",
                iotDevice.getIPAddress().c_str(),
                iotDevice.isAuthenticated() ? "Authenticated" : "Not authenticated"
            );
            break;
        case 2:  // Settings screen
            char thresholds[32];
            snprintf(thresholds, sizeof(thresholds), "L:%.0f%% H:%.0f%%",
                    iotDevice.deviceConfig.lowerThreshold.value,
                    iotDevice.deviceConfig.upperThreshold.value);
            displayManager.showSettings(
                iotDevice.deviceConfig.tankHeight.value,
                thresholds,
                autoMode ? "Auto" : "Manual"
            );
            break;
    }
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void handleButtons() {
    // BTN1 - Cycle display screens
    if (buttonHandler.isBtn1Pressed()) {
        displayScreenMode = (displayScreenMode + 1) % 3;
        DEBUG_PRINTF("[Button] Display screen: %d\n", displayScreenMode);
    }

    // BTN2 - Manual pump ON
    if (buttonHandler.isBtn2Pressed()) {
        iotDevice.controlData.autoMode.value = false;
        iotDevice.controlData.pumpSwitch.value = true;
        DEBUG_PRINTLN("[Button] Manual pump ON");
    }

    // BTN3 - Toggle Auto/Manual mode
    if (buttonHandler.isBtn3Pressed()) {
        bool newMode = !iotDevice.controlData.autoMode.value;
        iotDevice.controlData.autoMode.value = newMode;
        DEBUG_PRINTF("[Button] Mode: %s\n", newMode ? "Auto" : "Manual");
    }

    // BTN4 - Manual pump OFF
    if (buttonHandler.isBtn4Pressed()) {
        iotDevice.controlData.autoMode.value = false;
        iotDevice.controlData.pumpSwitch.value = false;
        DEBUG_PRINTLN("[Button] Manual pump OFF");
    }

    // BTN5 - WiFi reset (long press)
    if (buttonHandler.isBtn5LongPressed()) {
        DEBUG_PRINTLN("[Button] WiFi reset - restarting in AP mode");
        iotDevice.getStorage().clearWiFiCredentials();
        iotDevice.getStorage().clearDeviceToken();
        ESP.restart();
    }

    // BTN6 - Hardware override (handled by relay controller)
    // This is a physical switch that directly controls the relay
}

// ============================================================================
// SYSTEM COMMANDS
// ============================================================================

void handleSystemCommands() {
    // Check config update request from server
    if (iotDevice.systemControl.config_update.value) {
        DEBUG_PRINTLN("[System] Config update requested");
        iotDevice.fetchDeviceConfig();
        iotDevice.systemControl.config_update.value = false;
    }

    // Check force update (OTA firmware)
    if (iotDevice.systemConfig.force_update.value) {
        DEBUG_PRINTLN("[System] Firmware update requested");
        displayManager.showStatus("Updating", "Firmware", "Please wait...");
        // TODO: Implement OTA update logic
        // performOTAUpdate();
        iotDevice.systemConfig.force_update.value = false;
    }

    // Check auto_update flag
    if (!iotDevice.systemConfig.auto_update.value) {
        // Device should NOT automatically fetch config updates
        // User must manually trigger config fetch
    }
}
