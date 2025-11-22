/**
 * @file WaterTankMonitor.ino
 * @brief Complete water tank monitoring system using ArduinoIoTDevice
 *
 * This example demonstrates a full-featured water tank monitoring and control system:
 * - Ultrasonic water level measurement
 * - Automatic pump control with upper/lower thresholds
 * - Manual pump control via server/app
 * - Current inflow rate calculation
 * - Real-time telemetry upload to server
 * - WiFi provisioning and local web server
 * - Auto/Manual/Override modes
 *
 * Hardware:
 * - ESP32
 * - Ultrasonic sensor (HC-SR04 or JSN-SR04T)
 * - Relay module (pump control)
 * - Optional: OLED display
 * - Optional: Buttons for local control
 *
 * Pin Configuration:
 * - Ultrasonic TRIG: GPIO 6
 * - Ultrasonic ECHO: GPIO 7
 * - Relay (Pump): GPIO 5
 */

#include <ArduinoIoTDevice.h>

// ============================================================================
// HARDWARE PINS
// ============================================================================

#define ULTRASONIC_TRIG_PIN  6
#define ULTRASONIC_ECHO_PIN  7
#define RELAY_PIN            5

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Device Config - Water tank settings
 * System fields (force_update, ip_address, auto_update) automatically included
 */
struct WaterTankDeviceConfig {
    IoTField<float> tankHeight{"tankHeight", 100.0f, "Tank Height (cm)", "number"};
    IoTField<float> tankWidth{"tankWidth", 50.0f, "Tank Width (cm)", "number"};
    IoTField<String> tankShape{"tankShape", "Cylindrical", "Tank Shape", "string"};
    IoTField<float> upperThreshold{"upperThreshold", 85.0f, "Upper Threshold (%)", "number"};
    IoTField<float> lowerThreshold{"lowerThreshold", 20.0f, "Lower Threshold (%)", "number"};
    IoTField<float> maxInflow{"maxInflow", 100.0f, "Max Inflow (L/min)", "number"};
    IoTField<float> usedTotal{"usedTotal", 0.0f, "Total Water Used (L)", "number"};
    IoTField<bool> sensorFilter{"sensorFilter", true, "Sensor Filter", "boolean"};
};

/**
 * Control Data - Pump control commands
 * System field (config_update) automatically included
 */
struct WaterTankControlData {
    IoTField<bool> pumpSwitch{"pumpSwitch", false, "Pump Switch", "boolean"};
    IoTField<bool> autoMode{"autoMode", true, "Auto Mode", "boolean"};
};

/**
 * Telemetry Data - Water tank measurements
 * System field (Status) automatically included
 */
struct WaterTankTelemetryData {
    IoTField<float> waterLevel{"waterLevel", 0.0f, "Water Level (%)", "number"};
    IoTField<float> currInflow{"currInflow", 0.0f, "Current Inflow (L/min)", "number"};
    IoTField<int> pumpStatus{"pumpStatus", 0, "Pump Status", "number"};  // 0=OFF, 1=ON
    IoTField<float> waterHeight{"waterHeight", 0.0f, "Water Height (cm)", "number"};
};

// ============================================================================
// IOT DEVICE
// ============================================================================

IoTDevice<WaterTankDeviceConfig, WaterTankControlData, WaterTankTelemetryData> iotDevice;

// ============================================================================
// CONFIGURATION
// ============================================================================

const char* SERVER_URL = "http://103.136.236.16";
const char* PROJECT_ID = "wt001";
const char* DEVICE_NAME = "DEV-02";
const char* MONGODB_DEVICE_ID = "690e6a9d092433c0acfb9178";
const char* FIRMWARE_VERSION = "1.0.0";
const char* DEVICE_ID = "wt001-DEV-02";

const char* WIFI_SSID = "YourWiFiSSID";
const char* WIFI_PASSWORD = "YourWiFiPassword";
const char* DASHBOARD_USER = "your_username";
const char* DASHBOARD_PASS = "your_password";

// ============================================================================
// STATE VARIABLES
// ============================================================================

bool pumpCurrentState = false;
float previousWaterLevel = 0.0f;
float previousWaterHeight = 0.0f;
unsigned long previousTime = 0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 1000;  // Read every 1 second

// Smoothing filter
const int SMOOTHING_SAMPLES = 5;
float distanceReadings[SMOOTHING_SAMPLES];
int readingIndex = 0;

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("========================================");
    Serial.println("  Water Tank Monitoring System");
    Serial.println("  Using ArduinoIoTDevice Library");
    Serial.println("========================================");

    // Initialize hardware
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);
    digitalWrite(RELAY_PIN, LOW);

    // Initialize smoothing array
    for (int i = 0; i < SMOOTHING_SAMPLES; i++) {
        distanceReadings[i] = 0;
    }

    // Initialize IoT device
    iotDevice.begin();
    iotDevice.configureServer(SERVER_URL, PROJECT_ID, DEVICE_NAME,
                             MONGODB_DEVICE_ID, FIRMWARE_VERSION);
    iotDevice.setDeviceId(DEVICE_ID);
    iotDevice.setAPPassword("PassAquaWT001");

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
        Serial.printf("[Setup] Connect to 'AquaFlow-%s'\n", DEVICE_ID);
        iotDevice.startAPMode();
    }

    // Set telemetry upload interval
    iotDevice.setTelemetryInterval(30000);  // Upload every 30 seconds

    // Initialize time tracking
    previousTime = millis();

    Serial.println("[Setup] Water Tank Monitor Ready!");
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
        measureWaterLevel();
        lastSensorRead = millis();
    }

    // Control pump
    controlPump();

    // Handle system commands
    handleSystemCommands();

    delay(50);
}

// ============================================================================
// SENSOR READING
// ============================================================================

void measureWaterLevel() {
    // Read ultrasonic sensor
    float distance = readUltrasonicDistance();

    // Apply smoothing filter if enabled
    if (iotDevice.deviceConfig.sensorFilter.value) {
        distance = applySmoothingFilter(distance);
    }

    // Calculate water height (distance from sensor to water surface)
    float tankHeight = iotDevice.deviceConfig.tankHeight.value;
    float waterHeight = tankHeight - distance;

    // Constrain to valid range
    waterHeight = constrain(waterHeight, 0.0f, tankHeight);

    // Calculate water level percentage
    float waterLevel = (waterHeight / tankHeight) * 100.0f;

    // Calculate inflow rate
    float inflowRate = calculateInflowRate(waterHeight);

    // Update telemetry
    iotDevice.telemetryData.waterLevel.value = waterLevel;
    iotDevice.telemetryData.waterHeight.value = waterHeight;
    iotDevice.telemetryData.currInflow.value = inflowRate;
    iotDevice.telemetryData.pumpStatus.value = pumpCurrentState ? 1 : 0;

    // Update previous values
    previousWaterLevel = waterLevel;
    previousWaterHeight = waterHeight;

    Serial.printf("[Sensor] Level: %.1f%% (%.1fcm), Inflow: %.1fL/min, Pump: %s\n",
                 waterLevel, waterHeight, inflowRate,
                 pumpCurrentState ? "ON" : "OFF");
}

float readUltrasonicDistance() {
    // Send ultrasonic pulse
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);

    // Read echo
    long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, 30000);  // 30ms timeout

    if (duration == 0) {
        // Timeout - return previous value or max distance
        return iotDevice.deviceConfig.tankHeight.value;
    }

    // Calculate distance in cm (speed of sound = 343 m/s = 0.0343 cm/μs)
    float distance = duration * 0.0343f / 2.0f;

    return distance;
}

float applySmoothingFilter(float newReading) {
    // Add new reading to array
    distanceReadings[readingIndex] = newReading;
    readingIndex = (readingIndex + 1) % SMOOTHING_SAMPLES;

    // Calculate average
    float sum = 0;
    for (int i = 0; i < SMOOTHING_SAMPLES; i++) {
        sum += distanceReadings[i];
    }

    return sum / SMOOTHING_SAMPLES;
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
        // Volume = π * r² * h
        float radius = tankWidth / 2.0f;
        volumeChange = 3.14159f * radius * radius * heightChange;
    }
    else if (tankShape == "Rectangular") {
        // Volume = width² * h
        volumeChange = tankWidth * tankWidth * heightChange;
    }

    // Convert cm³ to liters (1L = 1000cm³)
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
    bool newPumpState = pumpCurrentState;

    // Check control mode
    if (iotDevice.controlData.autoMode.value) {
        // Auto mode - threshold-based control
        float waterLevel = iotDevice.telemetryData.waterLevel.value;
        float upperThreshold = iotDevice.deviceConfig.upperThreshold.value;
        float lowerThreshold = iotDevice.deviceConfig.lowerThreshold.value;

        if (waterLevel <= lowerThreshold && !pumpCurrentState) {
            // Water level too low - turn pump ON
            newPumpState = true;
            Serial.printf("[Control] Auto: Level %.1f%% <= %.1f%%, pump ON\n",
                         waterLevel, lowerThreshold);
        }
        else if (waterLevel >= upperThreshold && pumpCurrentState) {
            // Water level high enough - turn pump OFF
            newPumpState = false;
            Serial.printf("[Control] Auto: Level %.1f%% >= %.1f%%, pump OFF\n",
                         waterLevel, upperThreshold);
        }
    }
    else {
        // Manual mode - use server/app command
        newPumpState = iotDevice.controlData.pumpSwitch.value;

        if (newPumpState != pumpCurrentState) {
            Serial.printf("[Control] Manual: Pump %s\n",
                         newPumpState ? "ON" : "OFF");
        }
    }

    // Update pump if state changed
    if (newPumpState != pumpCurrentState) {
        pumpCurrentState = newPumpState;
        digitalWrite(RELAY_PIN, pumpCurrentState ? HIGH : LOW);

        // Update telemetry
        iotDevice.telemetryData.pumpStatus.value = pumpCurrentState ? 1 : 0;

        Serial.printf("[Pump] State changed to: %s\n", pumpCurrentState ? "ON" : "OFF");
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
}

// ============================================================================
// DIAGNOSTIC INFO
// ============================================================================

void printDetailedStatus() {
    Serial.println("\n========== WATER TANK STATUS ==========");
    Serial.printf("WiFi: %s (%s)\n",
                 iotDevice.isWiFiConnected() ? "Connected" : "Disconnected",
                 iotDevice.getIPAddress().c_str());
    Serial.printf("Server: %s\n", iotDevice.isAuthenticated() ? "Authenticated" : "Not authenticated");
    Serial.printf("Time Synced: %s\n", iotDevice.isTimeSynced() ? "Yes" : "No");
    Serial.println("\nTank Configuration:");
    Serial.printf("  Height: %.1fcm\n", iotDevice.deviceConfig.tankHeight.value);
    Serial.printf("  Width: %.1fcm\n", iotDevice.deviceConfig.tankWidth.value);
    Serial.printf("  Shape: %s\n", iotDevice.deviceConfig.tankShape.value.c_str());
    Serial.printf("  Upper Threshold: %.1f%%\n", iotDevice.deviceConfig.upperThreshold.value);
    Serial.printf("  Lower Threshold: %.1f%%\n", iotDevice.deviceConfig.lowerThreshold.value);
    Serial.println("\nCurrent Status:");
    Serial.printf("  Water Level: %.1f%%\n", iotDevice.telemetryData.waterLevel.value);
    Serial.printf("  Water Height: %.1fcm\n", iotDevice.telemetryData.waterHeight.value);
    Serial.printf("  Inflow Rate: %.1fL/min\n", iotDevice.telemetryData.currInflow.value);
    Serial.printf("  Pump: %s\n", pumpCurrentState ? "ON" : "OFF");
    Serial.printf("  Mode: %s\n", iotDevice.controlData.autoMode.value ? "Auto" : "Manual");
    Serial.println("=======================================\n");
}
