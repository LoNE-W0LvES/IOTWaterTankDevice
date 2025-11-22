/**
 * ESP32-S3 Water Tank Monitoring Device
 * Using ArduinoIoTDevice Library v1.0
 *
 * This main file handles initialization and main loop coordination.
 * All IoT functionality (WiFi, server sync, NTP, webserver) is handled by the library.
 * Hardware-specific logic is in separate manager files.
 *
 * File Structure:
 * - main.cpp (this file) - Setup and main loop
 * - iot_data_structures.h - Custom data structures using IoTField
 * - iot_handlers.cpp/h - Sensor reading, pump control, display, buttons, system commands
 * - sensor_manager.cpp/h - Ultrasonic sensor interface
 * - relay_controller.cpp/h - Pump relay control
 * - display_manager.cpp/h - OLED display management
 * - button_handler.cpp/h - Button input handling
 * - calculate_level.cpp/h - Water level calculations
 */

#include <Arduino.h>
#include <ArduinoIoTDevice.h>
#include "config.h"
#include "iot_data_structures.h"
#include "iot_handlers.h"
#include "sensor_manager.h"
#include "relay_controller.h"
#include "display_manager.h"
#include "button_handler.h"

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

// ============================================================================
// STATE TRACKING
// ============================================================================

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

    // Initialize hardware managers
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

    // Setup webserver callbacks for custom data serialization
    setupWebServerCallbacks();

    // Try to connect to WiFi
    DEBUG_PRINTLN("[Setup] Connecting to WiFi...");
    if (!iotDevice.connectWiFi()) {
        DEBUG_PRINTLN("[Setup] No saved WiFi - starting AP mode");
        iotDevice.startAPMode();
        displayManager.showMessage("Setup Mode", "Connect via App");
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

        // Update display with network info
        displayManager.setNetworkInfo(iotDevice.getIPAddress(), iotDevice.getWiFiManager().getSSID());

        // Start web server (automatically selects CLIENT mode)
        iotDevice.startWebServer();

        // Authenticate with server using credentials from provisioning
        String username, password;
        if (iotDevice.getStorage().loadDashboardCredentials(username, password)) {
            if (iotDevice.login(username, password)) {
                DEBUG_PRINTLN("[Setup] Login successful!");

                // Sync time with server
                iotDevice.syncTimeWithServer();

                // Fetch initial configuration
                iotDevice.fetchDeviceConfig();

                // Update display with tank settings
                displayManager.setTankSettings(
                    iotDevice.deviceConfig.tankHeight.value,
                    iotDevice.deviceConfig.tankWidth.value,
                    iotDevice.deviceConfig.tankShape.value,
                    iotDevice.deviceConfig.upperThreshold.value,
                    iotDevice.deviceConfig.lowerThreshold.value
                );

                systemInitialized = true;
            } else {
                DEBUG_PRINTLN("[Setup] Login failed - device must be registered by admin");
                displayManager.showMessage("Login Failed", "Contact Admin", 5000);
            }
        } else {
            DEBUG_PRINTLN("[Setup] No dashboard credentials - need provisioning");
            displayManager.showMessage("No Credentials", "Setup Required", 5000);
        }
    } else {
        DEBUG_PRINTLN("[Setup] Starting AP mode for provisioning");
        iotDevice.startAPMode();

        // Update display with AP credentials for user to connect
        displayManager.setAPInfo(
            iotDevice.getWiFiManager().getAPSSID(),
            iotDevice.getWiFiManager().getAPPassword(),
            WiFi.softAPIP().toString()
        );

        displayManager.showMessage("Setup Mode", "Check App", 5000);
    }

    // Set library intervals
    iotDevice.setTelemetryInterval(TELEMETRY_UPLOAD_INTERVAL);
    iotDevice.setControlFetchInterval(CONTROL_FETCH_INTERVAL);

    DEBUG_PRINTLN("[Setup] Initialization complete!");
    DEBUG_PRINTLN("========================================");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
    // Update IoT device (handles WiFi, sync, config/control fetch, telemetry upload)
    iotDevice.update();

    // Read sensors periodically
    if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
        readSensors();
        lastSensorRead = millis();
    }

    // Control pump based on mode and thresholds
    controlPump();

    // Update display periodically
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }

    // Handle button inputs
    buttonHandler.update();
    handleButtons();

    // Handle system commands (config_update, force_update, etc.)
    handleSystemCommands();

    delay(50);
}
