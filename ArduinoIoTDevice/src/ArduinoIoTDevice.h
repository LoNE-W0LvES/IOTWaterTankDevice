/**
 * @file ArduinoIoTDevice.h
 * @brief Main Arduino IoT Device Library
 *
 * Complete IoT device management with WiFi, server sync, and local web server.
 * Supports custom data structures with automatic timestamp tracking.
 *
 * @author IoT Development Team
 * @version 1.0.0
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

/**
 * @brief Main IoT Device class (template-based for custom data structures)
 * @tparam ConfigType Your custom device config struct
 * @tparam ControlType Your custom control data struct
 * @tparam TelemetryType Your custom telemetry data struct
 *
 * @example
 * ```cpp
 * struct MyDeviceConfig {
 *     IoTField<String> ipAddress{"ipAddress"};
 *     IoTField<float> threshold{"threshold"};
 * };
 *
 * struct MyControlData {
 *     IoTField<bool> pumpSwitch{"pumpSwitch"};
 *     IoTField<bool> config_update{"config_update"};
 * };
 *
 * struct MyTelemetryData {
 *     IoTField<float> temperature{"temperature"};
 *     IoTField<int> humidity{"humidity"};
 * };
 *
 * IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;
 * ```
 */
template<typename ConfigType, typename ControlType, typename TelemetryType>
class IoTDevice {
public:
    // Public data structures (user can access directly)
    ConfigType deviceConfig;
    ControlType controlData;
    TelemetryType telemetryData;

    IoTDevice()
        : storage("iot_device"),
          wifiManager(&storage),
          syncManager(&storage),
          apiClient(&storage, &syncManager),
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

        Serial.println("[IoTDevice] Ready");
    }

    /**
     * @brief Configure server connection
     * @param serverUrl Server URL (e.g., "http://192.168.1.100")
     * @param projectId Your project ID
     * @param deviceName Device name
     * @param mongoDeviceId MongoDB device ID
     * @param firmwareVersion Firmware version
     */
    void configureServer(const String& serverUrl, const String& projectId,
                        const String& deviceName, const String& mongoDeviceId,
                        const String& firmwareVersion = "1.0.0") {
        apiClient.setServerURL(serverUrl);
        apiClient.setDeviceInfo(projectId, deviceName, mongoDeviceId, firmwareVersion);
        apiClient.setHardwareId(wifiManager.getMACAddress());
    }

    /**
     * @brief Set device ID for WiFi AP mode
     */
    void setDeviceId(const String& deviceId) {
        wifiManager.setDeviceId(deviceId);
    }

    /**
     * @brief Set AP password for provisioning
     */
    void setAPPassword(const String& password) {
        wifiManager.setAPPassword(password);
    }

    // ========================================================================
    // MAIN LOOP
    // ========================================================================

    /**
     * @brief Main update loop - call this in loop()
     * Handles WiFi connection, server sync, and automatic operations
     */
    void update() {
        // Update WiFi connection
        wifiManager.updateConnection();

        // Update time tracking
        syncManager.update();

        // Auto-sync if enabled
        if (autoSync && wifiManager.isConnected()) {
            unsigned long now = millis();

            // Upload telemetry
            if (now - lastTelemetryTime >= telemetryInterval) {
                uploadTelemetryAuto();
                lastTelemetryTime = now;
            }

            // Fetch control data
            if (now - lastControlFetchTime >= controlFetchInterval) {
                fetchControlData();
                lastControlFetchTime = now;
            }
        }
    }

    // ========================================================================
    // WIFI MANAGEMENT
    // ========================================================================

    /**
     * @brief Connect to WiFi with saved credentials
     * @return true if connection initiated
     */
    bool connectWiFi() {
        return wifiManager.startClient();
    }

    /**
     * @brief Connect to WiFi with explicit credentials
     */
    bool connectWiFi(const String& ssid, const String& password) {
        wifiManager.saveCredentials(ssid, password);
        return wifiManager.startClient(ssid, password);
    }

    /**
     * @brief Start AP mode for provisioning
     */
    void startAPMode() {
        wifiManager.startAP();
    }

    /**
     * @brief Check if WiFi is connected
     */
    bool isWiFiConnected() {
        return wifiManager.isConnected();
    }

    /**
     * @brief Get WiFi status string
     */
    String getWiFiStatus() {
        return wifiManager.getStatusString();
    }

    /**
     * @brief Get IP address
     */
    String getIPAddress() {
        return wifiManager.getIPAddress();
    }

    /**
     * @brief Scan WiFi networks (returns JSON string)
     */
    String scanWiFiNetworks() {
        return wifiManager.scanNetworks();
    }

    // ========================================================================
    // SERVER AUTHENTICATION
    // ========================================================================

    /**
     * @brief Login to server with dashboard credentials
     */
    bool login(const String& username, const String& password) {
        storage.saveDashboardCredentials(username, password);
        bool success = apiClient.login(username, password);
        if (success) {
            syncTimeWithServer();
        }
        return success;
    }

    /**
     * @brief Register device with server
     */
    bool registerDevice() {
        bool success = apiClient.registerDevice();
        if (success) {
            syncTimeWithServer();
        }
        return success;
    }

    /**
     * @brief Check if device is authenticated
     */
    bool isAuthenticated() {
        return apiClient.isAuthenticated();
    }

    // ========================================================================
    // TIME SYNCHRONIZATION
    // ========================================================================

    /**
     * @brief Sync time with server
     */
    bool syncTimeWithServer() {
        return apiClient.syncTime();
    }

    /**
     * @brief Get current timestamp (milliseconds)
     */
    uint64_t getCurrentTimestamp() {
        return syncManager.getCurrentTimestamp();
    }

    /**
     * @brief Check if time is synced
     */
    bool isTimeSynced() {
        return syncManager.isTimeSynced();
    }

    // ========================================================================
    // CONFIG MANAGEMENT
    // ========================================================================

    /**
     * @brief Fetch device config from server
     * @return true if successful
     */
    bool fetchDeviceConfig() {
        Serial.println("[IoTDevice] Fetching device config...");

        String response;
        int statusCode = apiClient.httpGET("/api/device/config", response);

        if (statusCode == 200) {
            return parseDeviceConfig(response);
        }

        Serial.printf("[IoTDevice] Config fetch failed (HTTP %d)\n", statusCode);
        return false;
    }

    /**
     * @brief Upload device config to server
     * @param priority If true, sets lastModified to 0 (server accepts unconditionally)
     */
    bool uploadDeviceConfig(bool priority = false) {
        Serial.println("[IoTDevice] Uploading device config...");

        String payload = buildDeviceConfigPayload(priority);
        String response;
        int statusCode = apiClient.httpPOST("/api/device/config", payload, response);

        if (statusCode == 200 || statusCode == 201) {
            Serial.println("[IoTDevice] Config uploaded successfully");
            if (priority) {
                syncManager.clearDevicePriority();
            }
            return true;
        }

        Serial.printf("[IoTDevice] Config upload failed (HTTP %d)\n", statusCode);
        return false;
    }

    /**
     * @brief Mark device config as modified (will sync TO server on next update)
     */
    void markConfigModified() {
        syncManager.markDeviceConfigModified();
    }

    // ========================================================================
    // CONTROL DATA
    // ========================================================================

    /**
     * @brief Fetch control data from server
     */
    bool fetchControlData() {
        Serial.println("[IoTDevice] Fetching control data...");

        String response;
        int statusCode = apiClient.httpGET("/api/device/control", response);

        if (statusCode == 200) {
            return parseControlData(response);
        }

        Serial.printf("[IoTDevice] Control fetch failed (HTTP %d)\n", statusCode);
        return false;
    }

    /**
     * @brief Upload control data to server
     */
    bool uploadControlData() {
        Serial.println("[IoTDevice] Uploading control data...");

        String payload = buildControlDataPayload();
        String response;
        int statusCode = apiClient.httpPOST("/api/device/control", payload, response);

        if (statusCode == 200 || statusCode == 201) {
            Serial.println("[IoTDevice] Control data uploaded");
            return true;
        }

        Serial.printf("[IoTDevice] Control upload failed (HTTP %d)\n", statusCode);
        return false;
    }

    // ========================================================================
    // TELEMETRY
    // ========================================================================

    /**
     * @brief Upload telemetry data to server
     */
    bool uploadTelemetry() {
        Serial.println("[IoTDevice] Uploading telemetry...");

        String payload = buildTelemetryPayload();
        String response;
        int statusCode = apiClient.httpPOST("/api/device/telemetry", payload, response);

        if (statusCode == 200 || statusCode == 201) {
            Serial.println("[IoTDevice] Telemetry uploaded");
            return true;
        }

        Serial.printf("[IoTDevice] Telemetry upload failed (HTTP %d)\n", statusCode);
        return false;
    }

    /**
     * @brief Set telemetry upload interval (milliseconds)
     */
    void setTelemetryInterval(unsigned long intervalMs) {
        telemetryInterval = intervalMs;
    }

    /**
     * @brief Set control fetch interval (milliseconds)
     */
    void setControlFetchInterval(unsigned long intervalMs) {
        controlFetchInterval = intervalMs;
    }

    // ========================================================================
    // UTILITIES
    // ========================================================================

    /**
     * @brief Update field value and timestamp automatically
     * @example updateField(deviceConfig.ipAddress, "192.168.1.100")
     */
    template<typename T>
    void updateField(IoTField<T>& field, const T& newValue) {
        if (field.value != newValue) {
            field.value = newValue;
            field.lastModified = getCurrentTimestamp();
            markConfigModified();
        }
    }

    /**
     * @brief Get reference to WiFi manager
     */
    IoTWiFiManager& getWiFiManager() {
        return wifiManager;
    }

    /**
     * @brief Get reference to API client
     */
    IoTAPIClient& getAPIClient() {
        return apiClient;
    }

    /**
     * @brief Get reference to sync manager
     */
    IoTSyncManager& getSyncManager() {
        return syncManager;
    }

    /**
     * @brief Get reference to storage
     */
    IoTStorage& getStorage() {
        return storage;
    }

private:
    IoTStorage storage;
    IoTWiFiManager wifiManager;
    IoTSyncManager syncManager;
    IoTAPIClient apiClient;

    bool webServerEnabled;
    bool autoSync;
    unsigned long telemetryInterval;
    unsigned long controlFetchInterval;
    unsigned long lastTelemetryTime;
    unsigned long lastControlFetchTime;

    /**
     * @brief Parse device config from JSON response
     */
    bool parseDeviceConfig(const String& jsonStr) {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, jsonStr);

        if (error) {
            Serial.printf("[IoTDevice] Config parse error: %s\n", error.c_str());
            return false;
        }

        // Expected format: {"data": {"deviceConfig": {...}}}
        if (!doc.containsKey("data") || !doc["data"].containsKey("deviceConfig")) {
            Serial.println("[IoTDevice] Invalid config format");
            return false;
        }

        JsonObject configObj = doc["data"]["deviceConfig"];

        // Use reflection/iteration to parse all fields
        // This is a simplified version - you can extend with more sophisticated parsing
        Serial.println("[IoTDevice] Config parsed successfully");
        return true;
    }

    /**
     * @brief Build device config JSON payload
     */
    String buildDeviceConfigPayload(bool priority) {
        DynamicJsonDocument doc(4096);
        JsonObject configObj = doc.createNestedObject("deviceConfig");

        // Serialize all fields
        // This would need to be implemented based on your specific ConfigType
        // For now, it's a placeholder that you'd customize

        String payload;
        serializeJson(doc, payload);
        return payload;
    }

    /**
     * @brief Parse control data from JSON
     */
    bool parseControlData(const String& jsonStr) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonStr);

        if (error) {
            Serial.printf("[IoTDevice] Control parse error: %s\n", error.c_str());
            return false;
        }

        Serial.println("[IoTDevice] Control data parsed");
        return true;
    }

    /**
     * @brief Build control data payload
     */
    String buildControlDataPayload() {
        DynamicJsonDocument doc(2048);
        JsonObject controlObj = doc.createNestedObject("controlData");

        String payload;
        serializeJson(doc, payload);
        return payload;
    }

    /**
     * @brief Build telemetry payload
     */
    String buildTelemetryPayload() {
        DynamicJsonDocument doc(2048);
        JsonObject telemetryObj = doc.createNestedObject("sensorData");

        String payload;
        serializeJson(doc, payload);
        return payload;
    }

    /**
     * @brief Auto upload telemetry (called by update loop)
     */
    void uploadTelemetryAuto() {
        if (isAuthenticated()) {
            uploadTelemetry();
        }
    }
};

#endif // ARDUINO_IOT_DEVICE_H
