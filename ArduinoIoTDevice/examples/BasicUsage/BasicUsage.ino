/**
 * @file BasicUsage.ino
 * @brief Basic example of using ArduinoIoTDevice library
 *
 * This example demonstrates:
 * - Defining custom data structures
 * - WiFi connection and provisioning
 * - Server authentication
 * - Automatic data synchronization
 * - Field access with timestamps
 */

#include <ArduinoIoTDevice.h>

// ============================================================================
// DEFINE YOUR CUSTOM DATA STRUCTURES
// ============================================================================

/**
 * Device Configuration Structure
 * Define all config fields your device needs
 */
struct MyDeviceConfig {
    // Use IoTField<Type> for automatic timestamp tracking
    // Format: IoTField<Type> fieldName{"keyName", defaultValue, "Label", "type"}

    IoTField<String> ipAddress{"ipAddress", "", "IP Address", "string"};
    IoTField<float> upperThreshold{"upperThreshold", 85.0f, "Upper Threshold", "number"};
    IoTField<float> lowerThreshold{"lowerThreshold", 20.0f, "Lower Threshold", "number"};
    IoTField<float> tankHeight{"tankHeight", 100.0f, "Tank Height", "number"};
    IoTField<String> tankShape{"tankShape", "Cylindrical", "Tank Shape", "string"};
    IoTField<bool> auto_update{"auto_update", true, "Auto Update", "boolean"};
    IoTField<bool> sensorFilter{"sensorFilter", true, "Sensor Filter", "boolean"};
};

/**
 * Control Data Structure
 * Define control commands from server/app
 */
struct MyControlData {
    IoTField<bool> pumpSwitch{"pumpSwitch", false, "Pump Switch", "boolean"};
    IoTField<bool> config_update{"config_update", false, "Config Update", "boolean"};
};

/**
 * Telemetry Data Structure
 * Define sensor data to send to server
 */
struct MyTelemetryData {
    IoTField<float> waterLevel{"waterLevel", 0.0f, "Water Level", "number"};
    IoTField<float> currInflow{"currInflow", 0.0f, "Current Inflow", "number"};
    IoTField<int> pumpStatus{"pumpStatus", 0, "Pump Status", "number"};
    IoTField<int> Status{"Status", 1, "Device Status", "number"};
};

// ============================================================================
// CREATE IOT DEVICE INSTANCE
// ============================================================================

IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;

// ============================================================================
// CONFIGURATION
// ============================================================================

// Server configuration
const char* SERVER_URL = "http://103.136.236.16";
const char* PROJECT_ID = "wt001";
const char* DEVICE_NAME = "DEV-02";
const char* MONGODB_DEVICE_ID = "690e6a9d092433c0acfb9178";
const char* FIRMWARE_VERSION = "1.0.0";

// WiFi credentials (optional - can also use provisioning)
const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";

// Dashboard credentials
const char* DASHBOARD_USER = "your_username";
const char* DASHBOARD_PASS = "your_password";

// Device ID for AP mode
const char* DEVICE_ID = "wt001-DEV-02";

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  ArduinoIoTDevice Example");
    Serial.println("========================================");

    // 1. Initialize IoT device
    iotDevice.begin();

    // 2. Configure server connection
    iotDevice.configureServer(SERVER_URL, PROJECT_ID, DEVICE_NAME,
                             MONGODB_DEVICE_ID, FIRMWARE_VERSION);

    // 3. Set device ID for AP mode
    iotDevice.setDeviceId(DEVICE_ID);

    // 4. Set AP password (optional, has default)
    iotDevice.setAPPassword("PassAquaWT001");

    // 5. Connect to WiFi
    Serial.println("[Setup] Connecting to WiFi...");

    // Option A: Connect with saved credentials (from previous provisioning)
    if (iotDevice.connectWiFi()) {
        Serial.println("[Setup] Connecting with saved credentials...");
    }
    // Option B: Connect with explicit credentials
    // iotDevice.connectWiFi(WIFI_SSID, WIFI_PASSWORD);

    // Wait for connection (with timeout)
    unsigned long startTime = millis();
    while (!iotDevice.isWiFiConnected() && (millis() - startTime < 30000)) {
        iotDevice.update();
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (iotDevice.isWiFiConnected()) {
        Serial.printf("[Setup] WiFi connected! IP: %s\n", iotDevice.getIPAddress().c_str());

        // 6. Authenticate with server
        Serial.println("[Setup] Authenticating with server...");

        if (!iotDevice.isAuthenticated()) {
            // Try to login
            if (iotDevice.login(DASHBOARD_USER, DASHBOARD_PASS)) {
                Serial.println("[Setup] Login successful!");
            } else {
                // If login fails, try registration
                Serial.println("[Setup] Login failed, trying registration...");
                if (iotDevice.registerDevice()) {
                    Serial.println("[Setup] Device registered!");
                } else {
                    Serial.println("[Setup] Authentication failed!");
                }
            }
        } else {
            Serial.println("[Setup] Already authenticated");
        }

        // 7. Sync time with server
        if (iotDevice.syncTimeWithServer()) {
            Serial.println("[Setup] Time synchronized");
        }

        // 8. Fetch initial configuration
        if (iotDevice.fetchDeviceConfig()) {
            Serial.println("[Setup] Device config loaded");
        }

    } else {
        Serial.println("[Setup] WiFi connection failed - starting AP mode");
        iotDevice.startAPMode();
        Serial.printf("[Setup] AP Mode: Connect to '%s' to configure WiFi\n", DEVICE_ID);
    }

    // 9. Set intervals (optional, these are defaults)
    iotDevice.setTelemetryInterval(30000);      // Upload telemetry every 30 seconds
    iotDevice.setControlFetchInterval(300000);  // Fetch control every 5 minutes

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
    // EXAMPLE: Read and use device config
    // ========================================================================

    // Access config values with .value
    float upperThreshold = iotDevice.deviceConfig.upperThreshold.value;
    float lowerThreshold = iotDevice.deviceConfig.lowerThreshold.value;
    String ipAddr = iotDevice.deviceConfig.ipAddress.value;

    // Check last modified timestamp
    uint64_t ipLastModified = iotDevice.deviceConfig.ipAddress.lastModified;

    // ========================================================================
    // EXAMPLE: Update device config locally
    // ========================================================================

    // Method 1: Direct assignment
    // iotDevice.deviceConfig.tankHeight.value = 120.0f;
    // iotDevice.deviceConfig.tankHeight.lastModified = iotDevice.getCurrentTimestamp();
    // iotDevice.markConfigModified();

    // Method 2: Using helper (automatically sets timestamp and marks modified)
    // iotDevice.updateField(iotDevice.deviceConfig.tankHeight, 120.0f);

    // ========================================================================
    // EXAMPLE: Read control data from server
    // ========================================================================

    // Check if pump switch command received from server/app
    if (iotDevice.controlData.pumpSwitch.value) {
        Serial.println("[Loop] Pump ON command received");
        // Turn on pump
    } else {
        Serial.println("[Loop] Pump OFF command received");
        // Turn off pump
    }

    // Check if config update requested
    if (iotDevice.controlData.config_update.value) {
        Serial.println("[Loop] Config update requested - fetching new config");
        iotDevice.fetchDeviceConfig();
        // Reset the flag
        iotDevice.controlData.config_update.value = false;
    }

    // ========================================================================
    // EXAMPLE: Update telemetry data
    // ========================================================================

    // Simulate sensor readings
    float waterLevel = 75.5;  // Read from sensor
    float inflow = 12.3;      // Calculate inflow

    // Update telemetry fields
    iotDevice.telemetryData.waterLevel.value = waterLevel;
    iotDevice.telemetryData.currInflow.value = inflow;
    iotDevice.telemetryData.pumpStatus.value = 1;  // 1 = ON, 0 = OFF
    iotDevice.telemetryData.Status.value = 1;      // Always 1 (device online)

    // Telemetry is automatically uploaded by iotDevice.update()
    // Or manually upload:
    // iotDevice.uploadTelemetry();

    // ========================================================================
    // EXAMPLE: Manual operations
    // ========================================================================

    // Manually fetch control data
    // iotDevice.fetchControlData();

    // Manually upload config (with priority = true to override server)
    // iotDevice.uploadDeviceConfig(true);

    // Check current timestamp
    // uint64_t timestamp = iotDevice.getCurrentTimestamp();
    // Serial.printf("Current time: %llu ms\n", timestamp);

    // Small delay to prevent busy-waiting
    delay(100);
}

// ============================================================================
// HELPER FUNCTIONS (Optional)
// ============================================================================

/**
 * @brief Example: WiFi provisioning via serial commands
 */
void handleSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command == "scan") {
            Serial.println("Scanning WiFi networks...");
            String networks = iotDevice.scanWiFiNetworks();
            Serial.println(networks);
        }
        else if (command == "status") {
            Serial.printf("WiFi: %s\n", iotDevice.getWiFiStatus().c_str());
            Serial.printf("IP: %s\n", iotDevice.getIPAddress().c_str());
            Serial.printf("Authenticated: %s\n", iotDevice.isAuthenticated() ? "Yes" : "No");
            Serial.printf("Time Synced: %s\n", iotDevice.isTimeSynced() ? "Yes" : "No");
        }
        else if (command == "ap") {
            Serial.println("Starting AP mode...");
            iotDevice.startAPMode();
        }
        else if (command.startsWith("connect ")) {
            // Format: connect SSID PASSWORD
            int firstSpace = command.indexOf(' ');
            int secondSpace = command.indexOf(' ', firstSpace + 1);
            String ssid = command.substring(firstSpace + 1, secondSpace);
            String password = command.substring(secondSpace + 1);
            Serial.printf("Connecting to %s...\n", ssid.c_str());
            iotDevice.connectWiFi(ssid, password);
        }
    }
}
