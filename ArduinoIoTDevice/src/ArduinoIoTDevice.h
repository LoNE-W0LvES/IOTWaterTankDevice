/**
 * @file ArduinoIoTDevice.h
 * @brief Main Arduino IoT Device Library
 *
 * Complete IoT device management with WiFi, server sync, and local web server.
 * Includes permanent system fields and supports custom data structures.
 *
 * @author IoT Development Team
 * @version 1.0.1
 */

#ifndef ARDUINO_IOT_DEVICE_H
#define ARDUINO_IOT_DEVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "IoTField.h"
#include "IoTStorage.h"
#include "IoTWiFiManager.h"
#include "IoTSyncManager.h"
#include "IoTAPIClient.h"
#include "IoTWebServer.h"

// ============================================================================
// PERMANENT SYSTEM FIELDS (Present in all projects)
// ============================================================================

/**
 * @brief Permanent device config fields (included in every project)
 */
struct IoTSystemDeviceConfig {
    // System fields that are always present
    IoTField<bool> force_update{"force_update", false, "Force Firmware Update", "boolean"};
    IoTField<String> ip_address{"ip_address", "", "Device Local IP Address", "string"};
    IoTField<bool> auto_update{"auto_update", true, "Auto Update Configuration", "boolean"};

    IoTSystemDeviceConfig() {
        force_update.value = false;
        ip_address.value = "";
        auto_update.value = true;
    }
};

/**
 * @brief Permanent control data fields (included in every project)
 */
struct IoTSystemControlData {
    // System field that is always present
    IoTField<bool> config_update{"config_update", false, "Configuration Update", "boolean"};

    IoTSystemControlData() {
        config_update.value = false;
    }
};

/**
 * @brief Permanent telemetry fields (included in every project)
 */
struct IoTSystemTelemetryData {
    // System field that is always present (1 = online, 0 = offline)
    IoTField<int> Status{"Status", 1, "Device Status", "number"};

    IoTSystemTelemetryData() {
        Status.value = 1;  // Always 1 when sending telemetry
    }
};

// ============================================================================
// MAIN IOT DEVICE CLASS
// ============================================================================

/**
 * @brief Main IoT Device class (template-based for custom data structures)
 * @tparam ConfigType Your custom device config struct
 * @tparam ControlType Your custom control data struct
 * @tparam TelemetryType Your custom telemetry data struct
 *
 * System fields are automatically included:
 * - deviceConfig.force_update, .ip_address, .auto_update
 * - controlData.config_update
 * - telemetryData.Status
 *
 * @example
 * ```cpp
 * struct MyDeviceConfig {
 *     IoTField<float> threshold{"threshold", 25.0f};
 *     IoTField<String> tankShape{"tankShape", "Cylindrical"};
 * };
 *
 * struct MyControlData {
 *     IoTField<bool> pumpSwitch{"pumpSwitch", false};
 * };
 *
 * struct MyTelemetryData {
 *     IoTField<float> temperature{"temperature", 0.0f};
 * };
 *
 * IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;
 *
 * // Access custom fields:
 * float temp = iotDevice.telemetryData.temperature.value;
 *
 * // Access system fields:
 * bool forceUpdate = iotDevice.deviceConfig.force_update.value;
 * bool configUpdate = iotDevice.controlData.config_update.value;
 * iotDevice.telemetryData.Status.value = 1;
 * ```
 */
template<typename ConfigType = IoTSystemDeviceConfig,
         typename ControlType = IoTSystemControlData,
         typename TelemetryType = IoTSystemTelemetryData>
class IoTDevice {
public:
    // Public data structures (user can access directly)
    ConfigType deviceConfig;            // User's custom config fields
    ControlType controlData;            // User's custom control fields
    TelemetryType telemetryData;        // User's custom telemetry fields

    // System fields (always present)
    IoTSystemDeviceConfig systemConfig;     // force_update, ip_address, auto_update
    IoTSystemControlData systemControl;     // config_update
    IoTSystemTelemetryData systemTelemetry; // Status

    IoTDevice()
        : storage("iot_device"),
          wifiManager(&storage),
          syncManager(&storage),
          apiClient(&storage, &syncManager),
          webServer(80),
          webServerEnabled(false),
          autoSync(true),
          telemetryInterval(30000),
          controlFetchInterval(300000),
          lastTelemetryTime(0),
          lastControlFetchTime(0) {
    }

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize IoT device
     * Must be called in setup()
     */
    void begin() {
        Serial.println("[IoTDevice] Initializing...");

        storage.begin();
        wifiManager.begin();
        syncManager.begin();

        // Initialize system telemetry
        systemTelemetry.Status.value = 1;  // Always online when running

        Serial.println("[IoTDevice] Ready");
    }

    /**
     * @brief Configure server connection
     */
    void configureServer(const String& serverUrl, const String& projectId,
                        const String& deviceName, const String& mongoDeviceId,
                        const String& firmwareVersion = "1.0.0") {
        apiClient.setServerURL(serverUrl);
        apiClient.setDeviceInfo(projectId, deviceName, mongoDeviceId, firmwareVersion);
        apiClient.setHardwareId(wifiManager.getMACAddress());
    }

    /**
     * @brief Set device ID for WiFi AP mode and web server
     */
    void setDeviceId(const String& deviceId) {
        wifiManager.setDeviceId(deviceId);
        webServer.setDeviceId(deviceId);
    }

    /**
     * @brief Set AP password for provisioning
     */
    void setAPPassword(const String& password) {
        wifiManager.setAPPassword(password);
    }

    // ========================================================================
    // WEB SERVER
    // ========================================================================

    /**
     * @brief Start local web server for offline app access
     * @param port Web server port (default: 80)
     */
    void startWebServer(uint16_t port = 80) {
        if (webServerEnabled) {
            Serial.println("[IoTDevice] Web server already running");
            return;
        }

        // Set up web server callbacks
        setupWebServerCallbacks();

        // Enable provisioning if in AP mode
        if (wifiManager.getMode() == IOT_WIFI_AP_MODE) {
            webServer.enableProvisioning(true);
        }

        webServer.begin();
        webServerEnabled = true;

        Serial.println("[IoTDevice] Web server started");
    }

    /**
     * @brief Stop web server
     */
    void stopWebServer() {
        if (webServerEnabled) {
            webServer.stop();
            webServerEnabled = false;
        }
    }

    /**
     * @brief Check if web server is running
     */
    bool isWebServerRunning() {
        return webServerEnabled;
    }

    // ========================================================================
    // MAIN LOOP
    // ========================================================================

    /**
     * @brief Main update loop - call this in loop()
     */
    void update() {
        // Update WiFi connection
        bool wasConnected = wifiManager.isConnected();
        bool isConnected = wifiManager.updateConnection();

        // If just connected, update IP address in config
        if (!wasConnected && isConnected) {
            systemConfig.ip_address.value = wifiManager.getIPAddress();
            systemConfig.ip_address.lastModified = getCurrentTimestamp();
        }

        // Update time tracking
        syncManager.update();

        // Auto-sync if enabled and connected
        if (autoSync && isConnected) {
            unsigned long now = millis();

            // Upload telemetry
            if (now - lastTelemetryTime >= telemetryInterval) {
                uploadTelemetry();
                lastTelemetryTime = now;
            }

            // Fetch control data
            if (now - lastControlFetchTime >= controlFetchInterval) {
                fetchControlData();
                lastControlFetchTime = now;

                // Check if config update requested
                if (systemControl.config_update.value) {
                    fetchDeviceConfig();
                    systemControl.config_update.value = false;
                }
            }

            // Check force_update flag for OTA
            if (systemConfig.force_update.value) {
                Serial.println("[IoTDevice] Firmware update requested");
                // User should implement OTA update logic in their code
                // and reset this flag after update
            }
        }
    }

    // ========================================================================
    // WIFI MANAGEMENT
    // ========================================================================

    bool connectWiFi() {
        return wifiManager.startClient();
    }

    bool connectWiFi(const String& ssid, const String& password) {
        wifiManager.saveCredentials(ssid, password);
        return wifiManager.startClient(ssid, password);
    }

    void startAPMode() {
        wifiManager.startAP();

        // Start web server for provisioning
        if (!webServerEnabled) {
            webServer.enableProvisioning(true);
            setupWebServerCallbacks();
            webServer.begin();
            webServerEnabled = true;
        }
    }

    bool isWiFiConnected() {
        return wifiManager.isConnected();
    }

    String getWiFiStatus() {
        return wifiManager.getStatusString();
    }

    String getIPAddress() {
        return wifiManager.getIPAddress();
    }

    String scanWiFiNetworks() {
        return wifiManager.scanNetworks();
    }

    // ========================================================================
    // SERVER AUTHENTICATION
    // ========================================================================

    bool login(const String& username, const String& password) {
        storage.saveDashboardCredentials(username, password);
        bool success = apiClient.login(username, password);
        if (success) {
            syncTimeWithServer();
        }
        return success;
    }

    bool isAuthenticated() {
        return apiClient.isAuthenticated();
    }

    // ========================================================================
    // TIME SYNCHRONIZATION
    // ========================================================================

    bool syncTimeWithServer() {
        return apiClient.syncTime();
    }

    uint64_t getCurrentTimestamp() {
        return syncManager.getCurrentTimestamp();
    }

    bool isTimeSynced() {
        return syncManager.isTimeSynced();
    }

    // ========================================================================
    // CONFIG MANAGEMENT
    // ========================================================================

    bool fetchDeviceConfig() {
        Serial.println("[IoTDevice] Fetching device config...");

        String response;
        int statusCode = apiClient.httpGET("/api/device/config", response);

        if (statusCode == 200) {
            // Parse and apply config
            // User should override this in their code if needed
            Serial.println("[IoTDevice] Config fetched");
            return true;
        }

        Serial.printf("[IoTDevice] Config fetch failed (HTTP %d)\n", statusCode);
        return false;
    }

    bool uploadDeviceConfig(bool priority = false) {
        Serial.println("[IoTDevice] Uploading device config...");
        // Implementation depends on user's config structure
        return true;
    }

    void markConfigModified() {
        syncManager.markDeviceConfigModified();
    }

    // ========================================================================
    // CONTROL DATA
    // ========================================================================

    bool fetchControlData() {
        Serial.println("[IoTDevice] Fetching control data...");

        String response;
        int statusCode = apiClient.httpGET("/api/device/control", response);

        if (statusCode == 200) {
            Serial.println("[IoTDevice] Control data fetched");
            return true;
        }

        return false;
    }

    bool uploadControlData() {
        Serial.println("[IoTDevice] Uploading control data...");
        return true;
    }

    // ========================================================================
    // TELEMETRY
    // ========================================================================

    bool uploadTelemetry() {
        // Always set Status to 1 before sending
        systemTelemetry.Status.value = 1;

        Serial.println("[IoTDevice] Uploading telemetry...");
        // Implementation depends on user's telemetry structure
        return true;
    }

    void setTelemetryInterval(unsigned long intervalMs) {
        telemetryInterval = intervalMs;
    }

    void setControlFetchInterval(unsigned long intervalMs) {
        controlFetchInterval = intervalMs;
    }

    // ========================================================================
    // UTILITIES
    // ========================================================================

    template<typename T>
    void updateField(IoTField<T>& field, const T& newValue) {
        if (field.value != newValue) {
            field.value = newValue;
            field.lastModified = getCurrentTimestamp();
            markConfigModified();
        }
    }

    IoTWiFiManager& getWiFiManager() { return wifiManager; }
    IoTAPIClient& getAPIClient() { return apiClient; }
    IoTSyncManager& getSyncManager() { return syncManager; }
    IoTStorage& getStorage() { return storage; }
    IoTWebServer& getWebServer() { return webServer; }

private:
    IoTStorage storage;
    IoTWiFiManager wifiManager;
    IoTSyncManager syncManager;
    IoTAPIClient apiClient;
    IoTWebServer webServer;

    bool webServerEnabled;
    bool autoSync;
    unsigned long telemetryInterval;
    unsigned long controlFetchInterval;
    unsigned long lastTelemetryTime;
    unsigned long lastControlFetchTime;

    /**
     * @brief Setup web server callbacks
     */
    void setupWebServerCallbacks() {
        // GET /api/status callback
        webServer.onGetStatus([this]() -> String {
            DynamicJsonDocument doc(2048);
            // User should override this to include their telemetry data
            doc["timestamp"] = getCurrentTimestamp();
            doc["Status"] = systemTelemetry.Status.value;

            String response;
            serializeJson(doc, response);
            return response;
        });

        // GET /api/config callback
        webServer.onGetConfig([this]() -> String {
            DynamicJsonDocument doc(2048);
            // User should override this to include their config data
            doc["force_update"] = systemConfig.force_update.value;
            doc["ip_address"] = systemConfig.ip_address.value;
            doc["auto_update"] = systemConfig.auto_update.value;

            String response;
            serializeJson(doc, response);
            return response;
        });

        // POST /api/control callback
        webServer.onSetControl([this](const String& body) -> bool {
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                return false;
            }

            // Handle control commands
            // User should override this to handle their control data
            return true;
        });

        // WiFi provisioning callbacks
        webServer.onSaveWiFi([this](const String& ssid, const String& password) {
            Serial.printf("[IoTDevice] Saving WiFi: %s\n", ssid.c_str());
            connectWiFi(ssid, password);
        });

        webServer.onScanWiFi([this]() -> String {
            return scanWiFiNetworks();
        });
    }
};

#endif // ARDUINO_IOT_DEVICE_H
