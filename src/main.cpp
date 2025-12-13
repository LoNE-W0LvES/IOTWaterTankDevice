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
#include <nvs_flash.h>
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
bool wifiConnectionHandled = false;  // Track if we've handled initial WiFi connection

// ============================================================================
// WIFI CONNECTION HANDLER
// ============================================================================

void handleWiFiConnection() {
    // Only run once when WiFi first connects
    if (wifiConnectionHandled || !iotDevice.isWiFiConnected()) {
        return;
    }

    wifiConnectionHandled = true;
    DEBUG_PRINTF("[Setup] WiFi connected! IP: %s\n", iotDevice.getIPAddress().c_str());

    // Only show STA (client) info if not in AP mode
    // If in AP+STA mode, keep showing AP info for provisioning
    if (!iotDevice.getWiFiManager().isAPMode()) {
        // Pure STA mode - show client network info
        displayManager.setNetworkInfo(iotDevice.getIPAddress(), iotDevice.getWiFiManager().getSSID());
    }

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
            displayManager.showMessage("Connected!", "System Ready", 3000);
        } else {
            DEBUG_PRINTLN("[Setup] Login failed - device must be registered by admin");
            displayManager.showMessage("Login Failed", "Contact Admin", 5000);
        }
    } else {
        DEBUG_PRINTLN("[Setup] No dashboard credentials - need provisioning");
        displayManager.showMessage("No Credentials", "Setup Required", 5000);
    }
}

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

    // Initialize NVS flash for persistent storage
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or needs to be erased
        Serial.println("[NVS] Erasing flash and reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    Serial.println("[NVS] Flash initialized successfully");

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
    iotDevice.setMDNSHostname(MDNS_HOSTNAME);

    // Setup webserver callbacks for custom data serialization
    setupWebServerCallbacks();

    // Try to connect to WiFi (non-blocking)
    DEBUG_PRINTLN("[Setup] Connecting to WiFi...");
    if (!iotDevice.connectWiFi()) {
        DEBUG_PRINTLN("[Setup] No saved WiFi - starting AP mode");

        // Construct AP SSID from PROJECT_ID and DEVICE_NAME
        // Format: IoTDevice-PROJECT_ID-DEVICE_NAME
        String apSSID = "IoTDevice-" + String(PROJECT_ID) + "-" + String(DEVICE_NAME);

        // Set custom SSID before starting AP mode
        iotDevice.setCustomAPSSID(apSSID);
        iotDevice.startAPMode();

        displayManager.setAPInfo(
            apSSID,
            AP_PASSWORD,  // Use actual password from config.h
            WiFi.softAPIP().toString()
        );

        displayManager.showMessage("Setup Mode", "Connect via App");
    } else {
        DEBUG_PRINTLN("[Setup] WiFi connection initiated (will complete in background)");
        displayManager.showMessage("Connecting", "Please wait...");
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

    // Handle WiFi connection (runs once when connected)
    handleWiFiConnection();

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
