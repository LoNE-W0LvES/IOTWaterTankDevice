/**
 * @file TemperatureRelay.ino
 * @brief Temperature-controlled relay example using ArduinoIoTDevice
 *
 * This example demonstrates a smart relay that controls a cooling/heating device
 * based on temperature readings. Features:
 * - Auto mode: Automatic relay control based on temperature thresholds
 * - Manual mode: Remote control via server/app
 * - Real-time temperature monitoring and upload to server
 * - WiFi provisioning and local web server
 * - Remote threshold configuration
 *
 * Hardware:
 * - ESP32
 * - Temperature sensor (DHT22, DS18B20, or similar)
 * - Relay module
 *
 * Pin Configuration:
 * - Temperature sensor: GPIO 4
 * - Relay: GPIO 5
 */

#include <ArduinoIoTDevice.h>

// ============================================================================
// HARDWARE PINS
// ============================================================================

#define TEMP_SENSOR_PIN  4
#define RELAY_PIN        5

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Device Config - Temperature control settings
 * System fields (force_update, ip_address, auto_update) automatically included
 */
struct TempDeviceConfig {
    IoTField<float> tempThresholdHigh{"tempThresholdHigh", 28.0f, "High Temperature Threshold", "number"};
    IoTField<float> tempThresholdLow{"tempThresholdLow", 22.0f, "Low Temperature Threshold", "number"};
    IoTField<float> hysteresis{"hysteresis", 1.0f, "Hysteresis", "number"};
    IoTField<bool> autoMode{"autoMode", true, "Auto Mode", "boolean"};
    IoTField<String> deviceLocation{"deviceLocation", "Living Room", "Device Location", "string"};
};

/**
 * Control Data - Manual relay control
 * System field (config_update) automatically included
 */
struct TempControlData {
    IoTField<bool> relayOverride{"relayOverride", false, "Manual Relay Override", "boolean"};
    IoTField<bool> relayState{"relayState", false, "Manual Relay State", "boolean"};
};

/**
 * Telemetry Data - Temperature and relay status
 * System field (Status) automatically included
 */
struct TempTelemetryData {
    IoTField<float> temperature{"temperature", 0.0f, "Temperature", "number"};
    IoTField<float> humidity{"humidity", 0.0f, "Humidity", "number"};
    IoTField<int> relayStatus{"relayStatus", 0, "Relay Status", "number"};  // 0=OFF, 1=ON
    IoTField<String> operatingMode{"operatingMode", "auto", "Operating Mode", "string"};
};

// ============================================================================
// IOT DEVICE
// ============================================================================

IoTDevice<TempDeviceConfig, TempControlData, TempTelemetryData> iotDevice;

// ============================================================================
// CONFIGURATION
// ============================================================================

const char* SERVER_URL = "http://103.136.236.16";
const char* PROJECT_ID = "temp001";
const char* DEVICE_NAME = "relay001";
const char* MONGODB_DEVICE_ID = "mongodbid_temp_relay";
const char* FIRMWARE_VERSION = "1.0.0";
const char* DEVICE_ID = "temp001-relay001";

const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* DASHBOARD_USER = "your_username";
const char* DASHBOARD_PASS = "your_password";

// ============================================================================
// STATE VARIABLES
// ============================================================================

bool relayCurrentState = false;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 2000;  // Read every 2 seconds

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  Temperature Controlled Relay");
    Serial.println("  Using ArduinoIoTDevice Library");
    Serial.println("========================================");

    // Initialize hardware
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(TEMP_SENSOR_PIN, INPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Initialize IoT device
    iotDevice.begin();
    iotDevice.configureServer(SERVER_URL, PROJECT_ID, DEVICE_NAME,
                             MONGODB_DEVICE_ID, FIRMWARE_VERSION);
    iotDevice.setDeviceId(DEVICE_ID);
    iotDevice.setAPPassword("temp-relay-setup");

    // Connect WiFi
    Serial.println("[Setup] Connecting to WiFi...");
    iotDevice.connectWiFi(WIFI_SSID, WIFI_PASSWORD);

    unsigned long startTime = millis();
    while (!iotDevice.isWiFiConnected() && (millis() - startTime < 30000)) {
        iotDevice.update();
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (iotDevice.isWiFiConnected()) {
        Serial.printf("[Setup] Connected! IP: %s\n", iotDevice.getIPAddress().c_str());

        // Start web server
        iotDevice.startWebServer();

        // Authenticate
        if (!iotDevice.isAuthenticated()) {
            if (!iotDevice.login(DASHBOARD_USER, DASHBOARD_PASS)) {
                Serial.println("[Setup] Login failed - device must be registered by admin");
            }
        }

        // Sync time and fetch config
        iotDevice.syncTimeWithServer();
        iotDevice.fetchDeviceConfig();
    } else {
        Serial.println("[Setup] Starting AP mode");
        iotDevice.startAPMode();
    }

    // Set telemetry upload interval
    iotDevice.setTelemetryInterval(10000);  // Upload every 10 seconds

    Serial.println("[Setup] Ready!");
    Serial.println("========================================");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    // Update IoT device
    iotDevice.update();

    // Read sensors periodically
    if (millis() - lastSensorRead >= SENSOR_READ_INTERVAL) {
        readSensors();
        lastSensorRead = millis();
    }

    // Control relay based on mode
    controlRelay();

    // Handle system commands
    handleSystemCommands();

    delay(50);
}

// ============================================================================
// SENSOR READING
// ============================================================================

void readSensors() {
    // Read temperature and humidity
    // Replace with actual sensor reading code
    float temp = readTemperatureSensor();
    float hum = readHumiditySensor();

    // Update telemetry
    iotDevice.telemetryData.temperature.value = temp;
    iotDevice.telemetryData.humidity.value = hum;

    // Update relay status in telemetry
    iotDevice.telemetryData.relayStatus.value = relayCurrentState ? 1 : 0;

    // Update operating mode
    if (iotDevice.controlData.relayOverride.value) {
        iotDevice.telemetryData.operatingMode.value = "manual";
    } else {
        iotDevice.telemetryData.operatingMode.value = "auto";
    }

    Serial.printf("[Sensor] Temp: %.1f°C, Humidity: %.1f%%, Relay: %s, Mode: %s\n",
                 temp, hum,
                 relayCurrentState ? "ON" : "OFF",
                 iotDevice.telemetryData.operatingMode.value.c_str());
}

float readTemperatureSensor() {
    // Replace with actual sensor reading
    // Example: DHT22, DS18B20, etc.
    return 25.0f + random(-5, 5) / 10.0f;
}

float readHumiditySensor() {
    // Replace with actual sensor reading
    return 60.0f + random(-10, 10) / 10.0f;
}

// ============================================================================
// RELAY CONTROL LOGIC
// ============================================================================

void controlRelay() {
    bool newRelayState = relayCurrentState;

    // Check for manual override
    if (iotDevice.controlData.relayOverride.value) {
        // Manual mode - use server/app command
        newRelayState = iotDevice.controlData.relayState.value;

        if (newRelayState != relayCurrentState) {
            Serial.printf("[Control] Manual override: Relay %s\n",
                         newRelayState ? "ON" : "OFF");
        }
    }
    else if (iotDevice.deviceConfig.autoMode.value) {
        // Auto mode - temperature-based control with hysteresis
        float temp = iotDevice.telemetryData.temperature.value;
        float highThreshold = iotDevice.deviceConfig.tempThresholdHigh.value;
        float lowThreshold = iotDevice.deviceConfig.tempThresholdLow.value;
        float hysteresis = iotDevice.deviceConfig.hysteresis.value;

        if (temp >= highThreshold) {
            // Temperature too high - turn relay ON (e.g., cooling)
            if (!relayCurrentState) {
                Serial.printf("[Control] Auto: Temp %.1f°C >= %.1f°C, turning relay ON\n",
                             temp, highThreshold);
            }
            newRelayState = true;
        }
        else if (temp <= (highThreshold - hysteresis)) {
            // Temperature dropped enough - turn relay OFF
            if (relayCurrentState) {
                Serial.printf("[Control] Auto: Temp %.1f°C <= %.1f°C, turning relay OFF\n",
                             temp, (highThreshold - hysteresis));
            }
            newRelayState = false;
        }
        // Else: maintain current state (hysteresis zone)
    }

    // Update relay if state changed
    if (newRelayState != relayCurrentState) {
        relayCurrentState = newRelayState;
        digitalWrite(RELAY_PIN, relayCurrentState ? HIGH : LOW);

        // Update telemetry
        iotDevice.telemetryData.relayStatus.value = relayCurrentState ? 1 : 0;

        Serial.printf("[Relay] State changed to: %s\n", relayCurrentState ? "ON" : "OFF");
    }
}

// ============================================================================
// SYSTEM COMMANDS
// ============================================================================

void handleSystemCommands() {
    // Check config update request
    if (iotDevice.systemControl.config_update.value) {
        Serial.println("[System] Config update requested");
        iotDevice.fetchDeviceConfig();
        iotDevice.systemControl.config_update.value = false;
    }

    // Check force update (OTA)
    if (iotDevice.systemConfig.force_update.value) {
        Serial.println("[System] Firmware update requested");
        // Implement OTA update logic here
        // performOTAUpdate();
        // iotDevice.systemConfig.force_update.value = false;
    }

    // Check auto_update flag
    if (iotDevice.systemConfig.auto_update.value) {
        // Device will automatically fetch config updates
        // This is handled by the library's update() function
    }
}

// ============================================================================
// DIAGNOSTIC INFO (Optional)
// ============================================================================

void printStatus() {
    Serial.println("\n========== STATUS ==========");
    Serial.printf("WiFi: %s\n", iotDevice.isWiFiConnected() ? "Connected" : "Disconnected");
    Serial.printf("IP: %s\n", iotDevice.getIPAddress().c_str());
    Serial.printf("Server Auth: %s\n", iotDevice.isAuthenticated() ? "Yes" : "No");
    Serial.printf("Time Synced: %s\n", iotDevice.isTimeSynced() ? "Yes" : "No");
    Serial.printf("Temperature: %.1f°C\n", iotDevice.telemetryData.temperature.value);
    Serial.printf("Relay: %s\n", relayCurrentState ? "ON" : "OFF");
    Serial.printf("Mode: %s\n", iotDevice.telemetryData.operatingMode.value.c_str());
    Serial.printf("High Threshold: %.1f°C\n", iotDevice.deviceConfig.tempThresholdHigh.value);
    Serial.printf("Low Threshold: %.1f°C\n", iotDevice.deviceConfig.tempThresholdLow.value);
    Serial.println("============================\n");
}
