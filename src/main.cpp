/**
 * ESP32-S3 Water Tank Monitoring Device - Using ArduinoIoTDevice Library
 *
 * This simplified version uses the ArduinoIoTDevice library which handles:
 * - WiFi connectivity with AP fallback
 * - Backend API integration (JWT authentication)
 * - Time synchronization (NTP)
 * - Configuration sync with 3-way merge
 * - Control data fetch
 * - Telemetry upload
 * - Local web server for Flutter app
 *
 * This main.cpp only handles:
 * - Sensor reading (ultrasonic)
 * - Relay control logic
 * - Display updates (OLED)
 * - Button inputs
 */

#include <Arduino.h>
#include <ArduinoIoTDevice.h>
#include "config.h"
#include "sensor_manager.h"
#include "relay_controller.h"
#include "display_manager.h"
#include "button_handler.h"
#include "calculate_level.h"

// ============================================================================
// DATA STRUCTURES - Using IoTField for automatic timestamp tracking
// ============================================================================

struct WaterTankDeviceConfig {
    IoTField<float> tankHeight{"tankHeight", DEFAULT_TANK_HEIGHT, "Tank Height (cm)", "number"};
    IoTField<float> tankWidth{"tankWidth", DEFAULT_TANK_WIDTH, "Tank Width (cm)", "number"};
    IoTField<String> tankShape{"tankShape", "Cylindrical", "Tank Shape", "string"};
    IoTField<float> upperThreshold{"upperThreshold", DEFAULT_UPPER_THRESHOLD, "Upper Threshold (%)", "number"};
    IoTField<float> lowerThreshold{"lowerThreshold", DEFAULT_LOWER_THRESHOLD, "Lower Threshold (%)", "number"};
    IoTField<float> maxInflow{"maxInflow", 100.0f, "Max Inflow (L/min)", "number"};
    IoTField<float> usedTotal{"usedTotal", 0.0f, "Total Water Used (L)", "number"};
    IoTField<bool> sensorFilter{"sensorFilter", DEFAULT_SENSOR_FILTER, "Sensor Filter", "boolean"};
};

struct WaterTankControlData {
    IoTField<bool> pumpSwitch{"pumpSwitch", false, "Pump Switch", "boolean"};
    IoTField<bool> autoMode{"autoMode", true, "Auto Mode", "boolean"};
};

struct WaterTankTelemetryData {
    IoTField<float> waterLevel{"waterLevel", 0.0f, "Water Level (%)", "number"};
    IoTField<float> currInflow{"currInflow", 0.0f, "Current Inflow (L/min)", "number"};
    IoTField<int> pumpStatus{"pumpStatus", 0, "Pump Status", "number"};
    IoTField<float> waterHeight{"waterHeight", 0.0f, "Water Height (cm)", "number"};
};

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

IoTDevice<WaterTankDeviceConfig, WaterTankControlData, WaterTankTelemetryData> iotDevice;

SensorManager sensorManager;
RelayController relayController;
DisplayManager displayManager;
ButtonHandler buttonHandler;

// ============================================================================
// TIMING VARIABLES
// ============================================================================

unsigned long lastSensorRead = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long previousTime = 0;  // For inflow calculation

// ============================================================================
// STATE TRACKING
// ============================================================================

float previousWaterHeight = 0.0f;
bool systemInitialized = false;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  ESP32-S3 Water Tank Monitor");
    Serial.println("  Using ArduinoIoTDevice Library v1.0");
    Serial.println("========================================");

    // Initialize hardware
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Initialize managers (hardware-specific)
    sensorManager.begin();
    relayController.begin();
    displayManager.begin();
    buttonHandler.begin();

    // Initialize IoT device library
    iotDevice.begin();
    iotDevice.configureServer(SERVER_URL, PROJECT_ID, DEVICE_NAME,
                             MONGODB_DEVICE_ID, FIRMWARE_VERSION);
    iotDevice.setDeviceId(DEVICE_ID);
    iotDevice.setAPPassword(AP_PASSWORD);

    // Try to connect to WiFi
    DEBUG_PRINTLN("[Setup] Connecting to WiFi...");
    if (!iotDevice.connectWiFi()) {
        DEBUG_PRINTLN("[Setup] No saved WiFi - starting AP mode");
        iotDevice.startAPMode();
        displayManager.showStatus("Setup Mode", "Connect to WiFi", "");
    }

    // Wait for WiFi connection
    unsigned long startTime = millis();
    while (!iotDevice.isWiFiConnected() && (millis() - startTime < 30000)) {
        iotDevice.update();
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (iotDevice.isWiFiConnected()) {
        DEBUG_PRINTF("[Setup] WiFi connected! IP: %s\n", iotDevice.getIPAddress().c_str());

        // Update IP address in system config
        iotDevice.systemConfig.ip_address.value = iotDevice.getIPAddress();

        // Start web server in client mode
        iotDevice.startWebServer();  // Uses WS_MODE_CLIENT automatically

        // Authenticate with server (credentials from provisioning)
        String username, password;
        if (iotDevice.getStorage().loadDashboardCredentials(username, password)) {
            if (iotDevice.login(username, password)) {
                DEBUG_PRINTLN("[Setup] Login successful!");

                // Sync time with server
                iotDevice.syncTimeWithServer();

                // Fetch initial configuration
                iotDevice.fetchDeviceConfig();

                systemInitialized = true;
            } else {
                DEBUG_PRINTLN("[Setup] Login failed - device must be registered by admin");
                displayManager.showStatus("Login Failed", "Contact Admin", "");
            }
        } else {
            DEBUG_PRINTLN("[Setup] No dashboard credentials - need provisioning");
            displayManager.showStatus("No Credentials", "Setup Required", "");
        }
    } else {
        DEBUG_PRINTLN("[Setup] Starting AP mode for provisioning");
        iotDevice.startAPMode();
        displayManager.showStatus("Setup Mode", DEVICE_ID, iotDevice.getIPAddress().c_str());
    }

    // Set library intervals
    iotDevice.setTelemetryInterval(TELEMETRY_UPLOAD_INTERVAL);
    iotDevice.setControlFetchInterval(CONTROL_FETCH_INTERVAL);

    // Initialize time tracking
    previousTime = millis();

    DEBUG_PRINTLN("[Setup] Initialization complete!");
    DEBUG_PRINTLN("========================================");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Update IoT device (handles WiFi, sync, auto-upload)
    iotDevice.update();

    // Read sensors periodically
    if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
        readSensors();
        lastSensorRead = millis();
    }

    // Control pump
    controlPump();

    // Update display
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }

    // Handle button inputs
    buttonHandler.update();
    handleButtons();

    // Handle system commands from server
    handleSystemCommands();

    delay(50);
}

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

    if (timeDiff < 0.1f) {
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
    // Display different screens based on button cycles
    static int screenMode = 0;  // 0=Status, 1=Network, 2=Settings

    float waterLevel = iotDevice.telemetryData.waterLevel.value;
    float inflow = iotDevice.telemetryData.currInflow.value;
    bool pumpOn = relayController.isPumpOn();
    bool autoMode = iotDevice.controlData.autoMode.value;

    switch (screenMode) {
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
        static int screenMode = 0;
        screenMode = (screenMode + 1) % 3;
        DEBUG_PRINTF("[Button] Display screen: %d\n", screenMode);
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
