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
    float waterHeight = iotDevice.telemetryData.waterHeight.value;
    bool pumpOn = relayController.isPumpOn();
    bool autoMode = iotDevice.controlData.autoMode.value;

    // Use update() method to refresh display with current data
    displayManager.update(waterLevel, waterHeight, pumpOn, autoMode);
}

// ============================================================================
// BUTTON HANDLING
// ============================================================================

void handleButtons() {
    // BTN1 - Cycle display screens
    if (buttonHandler.isButtonPressed(1)) {
        displayManager.nextScreen();
        DEBUG_PRINTLN("[Button] Next screen");
    }

    // BTN2 - Manual pump ON
    if (buttonHandler.isButtonPressed(2)) {
        iotDevice.controlData.autoMode.value = false;
        iotDevice.controlData.pumpSwitch.value = true;
        DEBUG_PRINTLN("[Button] Manual pump ON");
    }

    // BTN3 - Toggle Auto/Manual mode
    if (buttonHandler.isButtonPressed(3)) {
        bool newMode = !iotDevice.controlData.autoMode.value;
        iotDevice.controlData.autoMode.value = newMode;
        DEBUG_PRINTF("[Button] Mode: %s\n", newMode ? "Auto" : "Manual");
    }

    // BTN4 - Manual pump OFF
    if (buttonHandler.isButtonPressed(4)) {
        iotDevice.controlData.autoMode.value = false;
        iotDevice.controlData.pumpSwitch.value = false;
        DEBUG_PRINTLN("[Button] Manual pump OFF");
    }

    // BTN5 - WiFi reset
    if (buttonHandler.isButtonPressed(5)) {
        DEBUG_PRINTLN("[Button] WiFi reset - restarting in AP mode");
        displayManager.showMessage("WiFi Reset", "Restarting...");
        delay(1000);
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
        displayManager.showMessage("Firmware Update", "Please wait...", 3000);
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

// ============================================================================
// WEBSERVER CALLBACKS SETUP
// ============================================================================

void setupWebServerCallbacks() {
    iotDevice.setTelemetryCallback(serializeTelemetry);
    iotDevice.setControlGetCallback(serializeControl);
    iotDevice.setControlSetCallback(deserializeControl);
    iotDevice.setConfigGetCallback(serializeConfig);
    iotDevice.setConfigSetCallback(deserializeConfig);
}

// ============================================================================
// SERIALIZATION CALLBACKS
// ============================================================================

String serializeTelemetry() {
    DynamicJsonDocument doc(2048);

    // Custom telemetry fields (using server format with nested objects)
    JsonObject waterLevel = doc.createNestedObject("waterLevel");
    waterLevel["key"] = "waterLevel";
    waterLevel["label"] = "Water Level";
    waterLevel["type"] = "number";
    waterLevel["value"] = iotDevice.telemetryData.waterLevel.value;
    waterLevel["lastModified"] = iotDevice.telemetryData.waterLevel.lastModified;

    JsonObject currInflow = doc.createNestedObject("currInflow");
    currInflow["key"] = "currInflow";
    currInflow["label"] = "Current Inflow";
    currInflow["type"] = "number";
    currInflow["value"] = iotDevice.telemetryData.currInflow.value;
    currInflow["lastModified"] = iotDevice.telemetryData.currInflow.lastModified;

    JsonObject pumpStatus = doc.createNestedObject("pumpStatus");
    pumpStatus["key"] = "pumpStatus";
    pumpStatus["label"] = "Pump Status";
    pumpStatus["type"] = "number";
    pumpStatus["value"] = iotDevice.telemetryData.pumpStatus.value;
    pumpStatus["lastModified"] = iotDevice.telemetryData.pumpStatus.lastModified;

    JsonObject waterHeight = doc.createNestedObject("waterHeight");
    waterHeight["key"] = "waterHeight";
    waterHeight["label"] = "Water Height";
    waterHeight["type"] = "number";
    waterHeight["value"] = iotDevice.telemetryData.waterHeight.value;
    waterHeight["lastModified"] = iotDevice.telemetryData.waterHeight.lastModified;

    // System telemetry
    JsonObject status = doc.createNestedObject("Status");
    status["key"] = "Status";
    status["label"] = "Device Status";
    status["type"] = "number";
    status["value"] = iotDevice.systemTelemetry.Status.value;
    status["lastModified"] = iotDevice.systemTelemetry.Status.lastModified;

    // Timestamp
    doc["timestamp"] = iotDevice.getCurrentTimestamp();

    String response;
    serializeJson(doc, response);
    return response;
}

String serializeControl() {
    DynamicJsonDocument doc(2048);

    // Custom control fields
    JsonObject pumpSwitch = doc.createNestedObject("pumpSwitch");
    pumpSwitch["key"] = "pumpSwitch";
    pumpSwitch["label"] = "Pump Switch";
    pumpSwitch["type"] = "boolean";
    pumpSwitch["value"] = iotDevice.controlData.pumpSwitch.value;
    pumpSwitch["lastModified"] = iotDevice.controlData.pumpSwitch.lastModified;

    JsonObject autoMode = doc.createNestedObject("autoMode");
    autoMode["key"] = "autoMode";
    autoMode["label"] = "Auto Mode";
    autoMode["type"] = "boolean";
    autoMode["value"] = iotDevice.controlData.autoMode.value;
    autoMode["lastModified"] = iotDevice.controlData.autoMode.lastModified;

    // System control fields
    JsonObject configUpdate = doc.createNestedObject("config_update");
    configUpdate["key"] = "config_update";
    configUpdate["label"] = "Configuration Update";
    configUpdate["type"] = "boolean";
    configUpdate["value"] = iotDevice.systemControl.config_update.value;
    configUpdate["lastModified"] = iotDevice.systemControl.config_update.lastModified;

    String response;
    serializeJson(doc, response);
    return response;
}

bool deserializeControl(const String& body) {
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        DEBUG_PRINTF("[Control] Parse error: %s\n", error.c_str());
        return false;
    }

    // Update custom control fields
    if (doc.containsKey("pumpSwitch") && doc["pumpSwitch"].containsKey("value")) {
        iotDevice.controlData.pumpSwitch.value = doc["pumpSwitch"]["value"].as<bool>();
        if (doc["pumpSwitch"].containsKey("lastModified")) {
            iotDevice.controlData.pumpSwitch.lastModified = doc["pumpSwitch"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("autoMode") && doc["autoMode"].containsKey("value")) {
        iotDevice.controlData.autoMode.value = doc["autoMode"]["value"].as<bool>();
        if (doc["autoMode"].containsKey("lastModified")) {
            iotDevice.controlData.autoMode.lastModified = doc["autoMode"]["lastModified"].as<uint64_t>();
        }
    }

    // Update system control fields
    if (doc.containsKey("config_update") && doc["config_update"].containsKey("value")) {
        iotDevice.systemControl.config_update.value = doc["config_update"]["value"].as<bool>();
        if (doc["config_update"].containsKey("lastModified")) {
            iotDevice.systemControl.config_update.lastModified = doc["config_update"]["lastModified"].as<uint64_t>();
        }
    }

    return true;
}

String serializeConfig() {
    DynamicJsonDocument doc(4096);

    // Custom config fields
    JsonObject tankHeight = doc.createNestedObject("tankHeight");
    tankHeight["key"] = "tankHeight";
    tankHeight["label"] = "Tank Height (cm)";
    tankHeight["type"] = "number";
    tankHeight["value"] = iotDevice.deviceConfig.tankHeight.value;
    tankHeight["lastModified"] = iotDevice.deviceConfig.tankHeight.lastModified;

    JsonObject tankWidth = doc.createNestedObject("tankWidth");
    tankWidth["key"] = "tankWidth";
    tankWidth["label"] = "Tank Width (cm)";
    tankWidth["type"] = "number";
    tankWidth["value"] = iotDevice.deviceConfig.tankWidth.value;
    tankWidth["lastModified"] = iotDevice.deviceConfig.tankWidth.lastModified;

    JsonObject tankShape = doc.createNestedObject("tankShape");
    tankShape["key"] = "tankShape";
    tankShape["label"] = "Tank Shape";
    tankShape["type"] = "string";
    tankShape["value"] = iotDevice.deviceConfig.tankShape.value;
    tankShape["lastModified"] = iotDevice.deviceConfig.tankShape.lastModified;

    JsonObject upperThreshold = doc.createNestedObject("upperThreshold");
    upperThreshold["key"] = "upperThreshold";
    upperThreshold["label"] = "Upper Threshold (%)";
    upperThreshold["type"] = "number";
    upperThreshold["value"] = iotDevice.deviceConfig.upperThreshold.value;
    upperThreshold["lastModified"] = iotDevice.deviceConfig.upperThreshold.lastModified;

    JsonObject lowerThreshold = doc.createNestedObject("lowerThreshold");
    lowerThreshold["key"] = "lowerThreshold";
    lowerThreshold["label"] = "Lower Threshold (%)";
    lowerThreshold["type"] = "number";
    lowerThreshold["value"] = iotDevice.deviceConfig.lowerThreshold.value;
    lowerThreshold["lastModified"] = iotDevice.deviceConfig.lowerThreshold.lastModified;

    JsonObject maxInflow = doc.createNestedObject("maxInflow");
    maxInflow["key"] = "maxInflow";
    maxInflow["label"] = "Max Inflow (L/min)";
    maxInflow["type"] = "number";
    maxInflow["value"] = iotDevice.deviceConfig.maxInflow.value;
    maxInflow["lastModified"] = iotDevice.deviceConfig.maxInflow.lastModified;

    JsonObject usedTotal = doc.createNestedObject("usedTotal");
    usedTotal["key"] = "usedTotal";
    usedTotal["label"] = "Total Water Used (L)";
    usedTotal["type"] = "number";
    usedTotal["value"] = iotDevice.deviceConfig.usedTotal.value;
    usedTotal["lastModified"] = iotDevice.deviceConfig.usedTotal.lastModified;

    JsonObject sensorFilter = doc.createNestedObject("sensorFilter");
    sensorFilter["key"] = "sensorFilter";
    sensorFilter["label"] = "Sensor Filter";
    sensorFilter["type"] = "boolean";
    sensorFilter["value"] = iotDevice.deviceConfig.sensorFilter.value;
    sensorFilter["lastModified"] = iotDevice.deviceConfig.sensorFilter.lastModified;

    // System config fields
    JsonObject forceUpdate = doc.createNestedObject("force_update");
    forceUpdate["key"] = "force_update";
    forceUpdate["label"] = "Force Firmware Update";
    forceUpdate["type"] = "boolean";
    forceUpdate["value"] = iotDevice.systemConfig.force_update.value;
    forceUpdate["lastModified"] = iotDevice.systemConfig.force_update.lastModified;

    JsonObject ipAddress = doc.createNestedObject("ip_address");
    ipAddress["key"] = "ip_address";
    ipAddress["label"] = "Device Local IP Address";
    ipAddress["type"] = "string";
    ipAddress["value"] = iotDevice.systemConfig.ip_address.value;
    ipAddress["lastModified"] = iotDevice.systemConfig.ip_address.lastModified;

    JsonObject autoUpdate = doc.createNestedObject("auto_update");
    autoUpdate["key"] = "auto_update";
    autoUpdate["label"] = "Auto Update Configuration";
    autoUpdate["type"] = "boolean";
    autoUpdate["value"] = iotDevice.systemConfig.auto_update.value;
    autoUpdate["lastModified"] = iotDevice.systemConfig.auto_update.lastModified;

    String response;
    serializeJson(doc, response);
    return response;
}

bool deserializeConfig(const String& body) {
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        DEBUG_PRINTF("[Config] Parse error: %s\n", error.c_str());
        return false;
    }

    // Update custom config fields
    if (doc.containsKey("tankHeight") && doc["tankHeight"].containsKey("value")) {
        iotDevice.deviceConfig.tankHeight.value = doc["tankHeight"]["value"].as<float>();
        if (doc["tankHeight"].containsKey("lastModified")) {
            iotDevice.deviceConfig.tankHeight.lastModified = doc["tankHeight"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("tankWidth") && doc["tankWidth"].containsKey("value")) {
        iotDevice.deviceConfig.tankWidth.value = doc["tankWidth"]["value"].as<float>();
        if (doc["tankWidth"].containsKey("lastModified")) {
            iotDevice.deviceConfig.tankWidth.lastModified = doc["tankWidth"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("tankShape") && doc["tankShape"].containsKey("value")) {
        iotDevice.deviceConfig.tankShape.value = doc["tankShape"]["value"].as<String>();
        if (doc["tankShape"].containsKey("lastModified")) {
            iotDevice.deviceConfig.tankShape.lastModified = doc["tankShape"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("upperThreshold") && doc["upperThreshold"].containsKey("value")) {
        iotDevice.deviceConfig.upperThreshold.value = doc["upperThreshold"]["value"].as<float>();
        if (doc["upperThreshold"].containsKey("lastModified")) {
            iotDevice.deviceConfig.upperThreshold.lastModified = doc["upperThreshold"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("lowerThreshold") && doc["lowerThreshold"].containsKey("value")) {
        iotDevice.deviceConfig.lowerThreshold.value = doc["lowerThreshold"]["value"].as<float>();
        if (doc["lowerThreshold"].containsKey("lastModified")) {
            iotDevice.deviceConfig.lowerThreshold.lastModified = doc["lowerThreshold"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("maxInflow") && doc["maxInflow"].containsKey("value")) {
        iotDevice.deviceConfig.maxInflow.value = doc["maxInflow"]["value"].as<float>();
        if (doc["maxInflow"].containsKey("lastModified")) {
            iotDevice.deviceConfig.maxInflow.lastModified = doc["maxInflow"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("usedTotal") && doc["usedTotal"].containsKey("value")) {
        iotDevice.deviceConfig.usedTotal.value = doc["usedTotal"]["value"].as<float>();
        if (doc["usedTotal"].containsKey("lastModified")) {
            iotDevice.deviceConfig.usedTotal.lastModified = doc["usedTotal"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("sensorFilter") && doc["sensorFilter"].containsKey("value")) {
        iotDevice.deviceConfig.sensorFilter.value = doc["sensorFilter"]["value"].as<bool>();
        if (doc["sensorFilter"].containsKey("lastModified")) {
            iotDevice.deviceConfig.sensorFilter.lastModified = doc["sensorFilter"]["lastModified"].as<uint64_t>();
        }
    }

    // Update system config fields
    if (doc.containsKey("force_update") && doc["force_update"].containsKey("value")) {
        iotDevice.systemConfig.force_update.value = doc["force_update"]["value"].as<bool>();
        if (doc["force_update"].containsKey("lastModified")) {
            iotDevice.systemConfig.force_update.lastModified = doc["force_update"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("ip_address") && doc["ip_address"].containsKey("value")) {
        iotDevice.systemConfig.ip_address.value = doc["ip_address"]["value"].as<String>();
        if (doc["ip_address"].containsKey("lastModified")) {
            iotDevice.systemConfig.ip_address.lastModified = doc["ip_address"]["lastModified"].as<uint64_t>();
        }
    }

    if (doc.containsKey("auto_update") && doc["auto_update"].containsKey("value")) {
        iotDevice.systemConfig.auto_update.value = doc["auto_update"]["value"].as<bool>();
        if (doc["auto_update"].containsKey("lastModified")) {
            iotDevice.systemConfig.auto_update.lastModified = doc["auto_update"]["lastModified"].as<uint64_t>();
        }
    }

    return true;
}
