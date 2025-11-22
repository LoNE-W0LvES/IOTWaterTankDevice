/**
 * @file BasicUsage.ino
 * @brief Basic example of using ArduinoIoTDevice library
 *
 * This example demonstrates:
 * - System fields are automatically included (force_update, ip_address, auto_update, config_update, Status)
 * - Defining custom data structures for your application
 * - WiFi connection and provisioning
 * - Server authentication
 * - Automatic data synchronization
 * - Field access with .value and .lastModified
 * - Local web server for offline app access
 */

#include <ArduinoIoTDevice.h>

// ============================================================================
// DEFINE YOUR CUSTOM DATA STRUCTURES
// ============================================================================

/**
 * Device Configuration Structure
 * Define only YOUR application-specific fields here.
 * System fields (force_update, ip_address, auto_update) are automatically included!
 */
struct MyDeviceConfig {
    IoTField<float> upperThreshold{"upperThreshold", 85.0f, "Upper Threshold", "number"};
    IoTField<float> lowerThreshold{"lowerThreshold", 20.0f, "Lower Threshold", "number"};
    IoTField<String> deviceName{"deviceName", "MyDevice", "Device Name", "string"};
};

/**
 * Control Data Structure
 * Define only YOUR application-specific control fields.
 * System field (config_update) is automatically included!
 */
struct MyControlData {
    IoTField<bool> relaySwitch{"relaySwitch", false, "Relay Switch", "boolean"};
};

/**
 * Telemetry Data Structure
 * Define only YOUR application-specific telemetry fields.
 * System field (Status) is automatically included!
 */
struct MyTelemetryData {
    IoTField<float> temperature{"temperature", 0.0f, "Temperature", "number"};
    IoTField<float> humidity{"humidity", 0.0f, "Humidity", "number"};
};

// ============================================================================
// CREATE IOT DEVICE INSTANCE
// ============================================================================

IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;

// ============================================================================
// CONFIGURATION
// ============================================================================

const char* SERVER_URL = "http://103.136.236.16";
const char* PROJECT_ID = "project001";
const char* DEVICE_NAME = "device001";
const char* MONGODB_DEVICE_ID = "mongodbid123";
const char* FIRMWARE_VERSION = "1.0.0";
const char* DEVICE_ID = "project001-device001";

// WiFi credentials (optional - can use provisioning)
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// Dashboard credentials
const char* DASHBOARD_USER = "your_username";
const char* DASHBOARD_PASS = "your_password";

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  ArduinoIoTDevice Basic Example");
    Serial.println("========================================");

    // 1. Initialize IoT device
    iotDevice.begin();

    // 2. Configure server connection
    iotDevice.configureServer(SERVER_URL, PROJECT_ID, DEVICE_NAME,
                             MONGODB_DEVICE_ID, FIRMWARE_VERSION);

    // 3. Set device ID for AP mode
    iotDevice.setDeviceId(DEVICE_ID);

    // 4. Set AP password (optional)
    iotDevice.setAPPassword("setup-password");

    // 5. Connect to WiFi
    Serial.println("[Setup] Connecting to WiFi...");
    if (!iotDevice.connectWiFi(WIFI_SSID, WIFI_PASSWORD)) {
        Serial.println("[Setup] No saved credentials - starting AP mode");
        iotDevice.startAPMode();
    }

    // Wait for connection
    unsigned long startTime = millis();
    while (!iotDevice.isWiFiConnected() && (millis() - startTime < 30000)) {
        iotDevice.update();
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (iotDevice.isWiFiConnected()) {
        Serial.printf("[Setup] WiFi connected! IP: %s\n", iotDevice.getIPAddress().c_str());

        // 6. Start local web server
        iotDevice.startWebServer();

        // 7. Authenticate with server
        if (!iotDevice.isAuthenticated()) {
            if (iotDevice.login(DASHBOARD_USER, DASHBOARD_PASS)) {
                Serial.println("[Setup] Login successful!");
            } else {
                Serial.println("[Setup] Trying registration...");
                iotDevice.registerDevice();
            }
        }

        // 8. Sync time
        iotDevice.syncTimeWithServer();

        // 9. Fetch initial config
        iotDevice.fetchDeviceConfig();

    } else {
        Serial.println("[Setup] Starting AP mode for provisioning");
        Serial.printf("[Setup] Connect to '%s' to configure WiFi\n", DEVICE_ID);
    }

    Serial.println("[Setup] Initialization complete!");
    Serial.println("========================================");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Update IoT device (handles WiFi, sync, auto-upload)
    iotDevice.update();

    // ========================================================================
    // ACCESS CUSTOM FIELDS
    // ========================================================================

    // Read your custom config values
    float upperThreshold = iotDevice.deviceConfig.upperThreshold.value;
    float lowerThreshold = iotDevice.deviceConfig.lowerThreshold.value;

    // Read timestamps
    uint64_t upperModified = iotDevice.deviceConfig.upperThreshold.lastModified;

    // ========================================================================
    // ACCESS SYSTEM FIELDS (Automatically Included)
    // ========================================================================

    // System config fields
    bool forceUpdate = iotDevice.systemConfig.force_update.value;
    String ipAddress = iotDevice.systemConfig.ip_address.value;
    bool autoUpdate = iotDevice.systemConfig.auto_update.value;

    // System control fields
    bool configUpdateRequested = iotDevice.systemControl.config_update.value;

    // System telemetry fields
    int deviceStatus = iotDevice.systemTelemetry.Status.value;  // Always 1

    // ========================================================================
    // UPDATE YOUR CUSTOM FIELDS
    // ========================================================================

    // Simulate sensor readings
    float temp = readTemperature();  // Your sensor reading function
    float hum = readHumidity();

    // Update telemetry
    iotDevice.telemetryData.temperature.value = temp;
    iotDevice.telemetryData.humidity.value = hum;

    // Update using helper (auto-sets timestamp)
    // iotDevice.updateField(iotDevice.deviceConfig.upperThreshold, 90.0f);

    // ========================================================================
    // HANDLE CONTROL COMMANDS
    // ========================================================================

    // Check your custom control fields
    if (iotDevice.controlData.relaySwitch.value) {
        Serial.println("[Loop] Relay ON command received");
        // Turn on relay
    }

    // Check system control fields
    if (configUpdateRequested) {
        Serial.println("[Loop] Config update requested");
        iotDevice.fetchDeviceConfig();
        iotDevice.systemControl.config_update.value = false;
    }

    // Check force update
    if (forceUpdate) {
        Serial.println("[Loop] Firmware update requested");
        // Implement your OTA update logic here
        // iotDevice.systemConfig.force_update.value = false;  // Reset after update
    }

    delay(100);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

float readTemperature() {
    // Replace with actual sensor reading
    return 25.0f + random(-5, 5);
}

float readHumidity() {
    // Replace with actual sensor reading
    return 60.0f + random(-10, 10);
}
