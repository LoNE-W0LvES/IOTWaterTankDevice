/**
 * @file IoTWebServer.h
 * @brief Local web server for offline app communication
 *
 * Provides REST API endpoints for local Flutter app to access device
 * Conditional endpoints based on mode:
 * - AP Mode: Only provisioning endpoints (status, scanWifi, save)
 * - Client Mode: Only app endpoints (telemetry, control, config, timestamp)
 */

#ifndef IOT_WEB_SERVER_H
#define IOT_WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

/**
 * @brief Callback function types for web server
 */
typedef std::function<String()> GetStatusCallback;
typedef std::function<String()> GetConfigCallback;
typedef std::function<String()> GetTelemetryCallback;
typedef std::function<String()> GetControlCallback;
typedef std::function<String()> GetTimestampCallback;
typedef std::function<bool(const String&)> SetControlCallback;
typedef std::function<bool(const String&)> SetConfigCallback;
typedef std::function<bool(uint64_t)> SetTimestampCallback;
typedef std::function<void(const String&, const String&, const String&, const String&)> SaveWiFiCallback;
typedef std::function<String()> ScanWiFiCallback;

enum WebServerMode {
    WS_MODE_PROVISIONING,  // AP mode - only provisioning endpoints
    WS_MODE_CLIENT         // Client mode - only app endpoints
};

class IoTWebServer {
public:
    IoTWebServer(uint16_t port = 80);
    ~IoTWebServer();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize and start web server
     * @param mode WebServerMode (PROVISIONING or CLIENT)
     */
    void begin(WebServerMode mode = WS_MODE_CLIENT);

    /**
     * @brief Stop web server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning();

    /**
     * @brief Set device ID for endpoints
     */
    void setDeviceId(const String& deviceId);

    // ========================================================================
    // PROVISIONING MODE CALLBACKS (AP Mode)
    // ========================================================================

    /**
     * @brief Set callback for POST /{deviceId}/save (provisioning)
     * Receives SSID, password, dashboardUsername, dashboardPassword
     */
    void onSaveWiFi(SaveWiFiCallback callback);

    /**
     * @brief Set WiFi scan callback for provisioning
     */
    void onScanWiFi(ScanWiFiCallback callback);

    // ========================================================================
    // CLIENT MODE CALLBACKS (Connected to WiFi)
    // ========================================================================

    /**
     * @brief Set callback for GET /{deviceId}/telemetry
     * Should return JSON with sensor data
     */
    void onGetTelemetry(GetTelemetryCallback callback);

    /**
     * @brief Set callback for GET /{deviceId}/control
     * Should return JSON with control data and timestamps
     */
    void onGetControl(GetControlCallback callback);

    /**
     * @brief Set callback for POST /{deviceId}/control
     * Receives JSON with control commands, returns true if handled
     */
    void onSetControl(SetControlCallback callback);

    /**
     * @brief Set callback for GET /{deviceId}/config
     * Should return JSON with device configuration
     */
    void onGetConfig(GetConfigCallback callback);

    /**
     * @brief Set callback for POST /{deviceId}/config
     * Receives JSON with config updates, returns true if handled
     */
    void onSetConfig(SetConfigCallback callback);

    /**
     * @brief Set callback for GET /{deviceId}/timestamp
     * Should return JSON with timestamp and sync status
     */
    void onGetTimestamp(GetTimestampCallback callback);

    /**
     * @brief Set callback for POST /{deviceId}/timestamp
     * Receives timestamp (auto-detects seconds/millis), returns true if handled
     */
    void onSetTimestamp(SetTimestampCallback callback);

private:
    AsyncWebServer* server;
    uint16_t port;
    String deviceId;
    bool running;
    WebServerMode currentMode;

    // Provisioning callbacks (AP mode only)
    SaveWiFiCallback saveWiFiCallback;
    ScanWiFiCallback scanWiFiCallback;

    // Client mode callbacks
    GetTelemetryCallback telemetryCallback;
    GetControlCallback getControlCallback;
    SetControlCallback setControlCallback;
    GetConfigCallback getConfigCallback;
    SetConfigCallback setConfigCallback;
    GetTimestampCallback getTimestampCallback;
    SetTimestampCallback setTimestampCallback;

    /**
     * @brief Setup provisioning routes (AP mode)
     */
    void setupProvisioningRoutes();

    /**
     * @brief Setup client routes (Connected mode)
     */
    void setupClientRoutes();

    /**
     * @brief Add CORS headers
     */
    void addCORSHeaders(AsyncWebServerResponse* response);

    /**
     * @brief Handle OPTIONS preflight requests
     */
    void handleOptions(AsyncWebServerRequest* request);

    /**
     * @brief Print available endpoints
     */
    void printEndpoints();
};

#endif // IOT_WEB_SERVER_H
