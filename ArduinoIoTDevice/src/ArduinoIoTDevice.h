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
          lastControlFetchTime(0),
          isConnectedToServer(false),
          consecutiveHeartbeatFailures(0),
          lastHeartbeatCheck(0),
          heartbeatInterval(HEARTBEAT_CHECK_INTERVAL) {
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
     * Automatically selects correct mode based on WiFi state
     * @param port Web server port (default: 80)
     */
    void startWebServer(uint16_t port = 80) {
        if (webServerEnabled) {
            Serial.println("[IoTDevice] Web server already running");
            return;
        }

        // Set up web server callbacks
        setupWebServerCallbacks();

        // Select mode based on WiFi state
        WebServerMode mode = (wifiManager.getMode() == IOT_WIFI_AP_MODE)
                            ? WS_MODE_PROVISIONING
                            : WS_MODE_CLIENT;

        webServer.begin(mode);
        webServerEnabled = true;

        Serial.printf("[IoTDevice] Web server started in %s mode\n",
                     mode == WS_MODE_PROVISIONING ? "PROVISIONING" : "CLIENT");
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

    /**
     * @brief Set custom telemetry serialization callback
     * Override the default telemetry callback to include your custom fields
     */
    void setTelemetryCallback(std::function<String()> callback) {
        customTelemetryCallback = callback;
    }

    /**
     * @brief Set custom control GET callback
     * Override the default control callback to include your custom fields
     */
    void setControlGetCallback(std::function<String()> callback) {
        customControlGetCallback = callback;
    }

    /**
     * @brief Set custom control SET callback
     * Override the default control callback to handle your custom fields
     */
    void setControlSetCallback(std::function<bool(const String&)> callback) {
        customControlSetCallback = callback;
    }

    /**
     * @brief Set custom config GET callback
     * Override the default config callback to include your custom fields
     */
    void setConfigGetCallback(std::function<String()> callback) {
        customConfigGetCallback = callback;
    }

    /**
     * @brief Set custom config SET callback
     * Override the default config callback to handle your custom fields
     */
    void setConfigSetCallback(std::function<bool(const String&)> callback) {
        customConfigSetCallback = callback;
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

        // If just connected, update IP address and restart webserver in client mode
        if (!wasConnected && isConnected) {
            systemConfig.ip_address.value = wifiManager.getIPAddress();
            systemConfig.ip_address.lastModified = getCurrentTimestamp();

            // Restart webserver in client mode
            // (Mode will be CLIENT_MODE at this point due to transition in updateConnection)
            if (webServerEnabled) {
                webServer.stop();
                webServerEnabled = false;
                delay(100);
                startWebServer();  // Will start in CLIENT mode based on current mode
            }
        }

        // Update time tracking
        syncManager.update();

        // Check server connectivity with heartbeat (non-blocking)
        if (isConnected && isAuthenticated()) {
            unsigned long now = millis();

            // Determine heartbeat interval based on server status
            unsigned long checkInterval = isConnectedToServer ? HEARTBEAT_CHECK_INTERVAL : HEARTBEAT_RETRY_INTERVAL;

            // Time to check heartbeat?
            if (now - lastHeartbeatCheck >= checkInterval) {
                lastHeartbeatCheck = now;

                // Check server heartbeat
                bool heartbeatOK = apiClient.checkHeartbeat();

                if (heartbeatOK) {
                    // Heartbeat success
                    if (!isConnectedToServer) {
                        // Server just came back online
                        Serial.println("[IoTDevice] Server is now ONLINE - resuming server tasks");
                        isConnectedToServer = true;

                        // Execute queued tasks sequentially
                        Serial.println("[IoTDevice] Syncing queued data with server...");
                        uploadTelemetry();
                        fetchControlData();
                        fetchDeviceConfig();
                    }
                    // Reset failure counter
                    consecutiveHeartbeatFailures = 0;
                    isConnectedToServer = true;

                } else {
                    // Heartbeat failed
                    consecutiveHeartbeatFailures++;
                    Serial.printf("[IoTDevice] Heartbeat failure %d/%d\n",
                                consecutiveHeartbeatFailures, MAX_HEARTBEAT_FAILURES);

                    // Check if we've exceeded max failures
                    if (consecutiveHeartbeatFailures >= MAX_HEARTBEAT_FAILURES && isConnectedToServer) {
                        Serial.println("[IoTDevice] Server is now OFFLINE - pausing server tasks");
                        isConnectedToServer = false;
                    }
                }
            }
        }

        // Auto-sync if enabled, WiFi connected, and server is online
        if (autoSync && isConnected && isConnectedToServer) {
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

    void setCustomAPSSID(const String& ssid) {
        wifiManager.setCustomAPSSID(ssid);
    }

    void startAPMode() {
        wifiManager.startAP();

        // Start web server in provisioning mode
        if (!webServerEnabled) {
            setupWebServerCallbacks();
            webServer.begin(WS_MODE_PROVISIONING);
            webServerEnabled = true;
            Serial.println("[IoTDevice] Web server started in PROVISIONING mode");
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

            // After NTP sync, check server heartbeat
            Serial.println("[IoTDevice] Checking server connectivity...");
            bool heartbeatOK = apiClient.checkHeartbeat();
            if (heartbeatOK) {
                isConnectedToServer = true;
                consecutiveHeartbeatFailures = 0;
                Serial.println("[IoTDevice] Server is ONLINE - starting server tasks");
            } else {
                isConnectedToServer = false;
                consecutiveHeartbeatFailures = 1;
                Serial.println("[IoTDevice] Server heartbeat failed - will retry periodically");
            }
            lastHeartbeatCheck = millis();
        }
        return success;
    }

    bool isAuthenticated() {
        return apiClient.isAuthenticated();
    }

    bool isServerOnline() {
        return isConnectedToServer;
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

    // Server connectivity tracking
    bool isConnectedToServer;
    int consecutiveHeartbeatFailures;
    unsigned long lastHeartbeatCheck;
    unsigned long heartbeatInterval;
    const int MAX_HEARTBEAT_FAILURES = 10;
    const unsigned long HEARTBEAT_CHECK_INTERVAL = 30000;  // 30 seconds
    const unsigned long HEARTBEAT_RETRY_INTERVAL = 60000;   // 1 minute when disconnected

    // Custom serialization callbacks
    std::function<String()> customTelemetryCallback;
    std::function<String()> customControlGetCallback;
    std::function<bool(const String&)> customControlSetCallback;
    std::function<String()> customConfigGetCallback;
    std::function<bool(const String&)> customConfigSetCallback;

    /**
     * @brief Setup web server callbacks
     */
    void setupWebServerCallbacks() {
        // WiFi provisioning callbacks (AP mode)
        webServer.onSaveWiFi([this](const String& ssid, const String& password,
                                   const String& dashboardUser, const String& dashboardPass) {
            Serial.printf("[IoTDevice] Saving WiFi: %s\n", ssid.c_str());
            Serial.printf("[IoTDevice] Dashboard credentials: %s\n", dashboardUser.c_str());

            // Save WiFi credentials
            connectWiFi(ssid, password);

            // Save dashboard credentials
            if (dashboardUser.length() > 0 && dashboardPass.length() > 0) {
                storage.saveDashboardCredentials(dashboardUser, dashboardPass);
            }
        });

        webServer.onScanWiFi([this]() -> String {
            return scanWiFiNetworks();
        });

        // Telemetry callback (Client mode)
        webServer.onGetTelemetry([this]() -> String {
            // Use custom callback if provided, otherwise default to system fields only
            if (customTelemetryCallback) {
                return customTelemetryCallback();
            }

            // Default: system fields only (users should provide custom callback)
            DynamicJsonDocument doc(512);
            doc["timestamp"] = getCurrentTimestamp();
            doc["Status"] = systemTelemetry.Status.value;

            String response;
            serializeJson(doc, response);
            return response;
        });

        // Control callbacks (Client mode)
        webServer.onGetControl([this]() -> String {
            // Use custom callback if provided
            if (customControlGetCallback) {
                return customControlGetCallback();
            }

            // Default: system fields only
            DynamicJsonDocument doc(512);
            doc["config_update"] = systemControl.config_update.value;
            doc["config_update_lastModified"] = systemControl.config_update.lastModified;

            String response;
            serializeJson(doc, response);
            return response;
        });

        webServer.onSetControl([this](const String& body) -> bool {
            // Use custom callback if provided
            if (customControlSetCallback) {
                return customControlSetCallback(body);
            }

            // Default: parse and update system control fields only
            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                return false;
            }

            // Update system control fields if present
            if (doc.containsKey("config_update")) {
                systemControl.config_update.value = doc["config_update"].as<bool>();
                if (doc.containsKey("config_update_lastModified")) {
                    systemControl.config_update.lastModified = doc["config_update_lastModified"].as<uint64_t>();
                }
            }

            return true;
        });

        // Config callbacks (Client mode)
        webServer.onGetConfig([this]() -> String {
            // Use custom callback if provided
            if (customConfigGetCallback) {
                return customConfigGetCallback();
            }

            // Default: system fields only
            DynamicJsonDocument doc(1024);
            doc["force_update"] = systemConfig.force_update.value;
            doc["force_update_lastModified"] = systemConfig.force_update.lastModified;
            doc["ip_address"] = systemConfig.ip_address.value;
            doc["ip_address_lastModified"] = systemConfig.ip_address.lastModified;
            doc["auto_update"] = systemConfig.auto_update.value;
            doc["auto_update_lastModified"] = systemConfig.auto_update.lastModified;

            String response;
            serializeJson(doc, response);
            return response;
        });

        webServer.onSetConfig([this](const String& body) -> bool {
            // Use custom callback if provided
            if (customConfigSetCallback) {
                return customConfigSetCallback(body);
            }

            // Default: parse and update system config fields only
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                return false;
            }

            // Update system config fields if present
            if (doc.containsKey("force_update")) {
                systemConfig.force_update.value = doc["force_update"].as<bool>();
                if (doc.containsKey("force_update_lastModified")) {
                    systemConfig.force_update.lastModified = doc["force_update_lastModified"].as<uint64_t>();
                }
            }
            if (doc.containsKey("ip_address")) {
                systemConfig.ip_address.value = doc["ip_address"].as<String>();
                if (doc.containsKey("ip_address_lastModified")) {
                    systemConfig.ip_address.lastModified = doc["ip_address_lastModified"].as<uint64_t>();
                }
            }
            if (doc.containsKey("auto_update")) {
                systemConfig.auto_update.value = doc["auto_update"].as<bool>();
                if (doc.containsKey("auto_update_lastModified")) {
                    systemConfig.auto_update.lastModified = doc["auto_update_lastModified"].as<uint64_t>();
                }
            }

            return true;
        });

        // Timestamp callbacks (Client mode)
        webServer.onGetTimestamp([this]() -> String {
            DynamicJsonDocument doc(256);
            doc["timestamp"] = getCurrentTimestamp();
            doc["synced"] = isTimeSynced();

            String response;
            serializeJson(doc, response);
            return response;
        });

        webServer.onSetTimestamp([this](uint64_t timestamp) -> bool {
            syncManager.setServerTime(timestamp);
            Serial.printf("[IoTDevice] Time synced from app: %llu\n", timestamp);
            return true;
        });
    }
};

#endif // ARDUINO_IOT_DEVICE_H
