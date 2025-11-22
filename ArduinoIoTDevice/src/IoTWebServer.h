/**
 * @file IoTWebServer.h
 * @brief Local web server for offline app communication
 *
 * Provides REST API endpoints for local Flutter app to access device
 * when server is offline or for local network access.
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
typedef std::function<bool(const String&)> SetControlCallback;
typedef std::function<void(const String&, const String&)> SaveWiFiCallback;

class IoTWebServer {
public:
    IoTWebServer(uint16_t port = 80);
    ~IoTWebServer();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize and start web server
     */
    void begin();

    /**
     * @brief Stop web server
     */
    void stop();

    /**
     * @brief Check if server is running
     */
    bool isRunning();

    // ========================================================================
    // CALLBACK REGISTRATION
    // ========================================================================

    /**
     * @brief Set callback for GET /api/status
     * Should return JSON with telemetry data
     */
    void onGetStatus(GetStatusCallback callback);

    /**
     * @brief Set callback for GET /api/config
     * Should return JSON with device configuration
     */
    void onGetConfig(GetConfigCallback callback);

    /**
     * @brief Set callback for POST /api/control
     * Receives JSON with control commands, returns true if handled
     */
    void onSetControl(SetControlCallback callback);

    /**
     * @brief Set callback for POST /wifi/save (provisioning)
     * Receives SSID and password
     */
    void onSaveWiFi(SaveWiFiCallback callback);

    /**
     * @brief Set device ID for provisioning endpoints
     */
    void setDeviceId(const String& deviceId);

    // ========================================================================
    // WIFI PROVISIONING
    // ========================================================================

    /**
     * @brief Enable/disable WiFi provisioning endpoints
     * Endpoints: /{deviceId}/status, /{deviceId}/scanWifi, /{deviceId}/save
     */
    void enableProvisioning(bool enable);

    /**
     * @brief Set WiFi scan callback for provisioning
     */
    void onScanWiFi(GetStatusCallback callback);

private:
    AsyncWebServer* server;
    uint16_t port;
    String deviceId;
    bool running;
    bool provisioningEnabled;

    // Callbacks
    GetStatusCallback statusCallback;
    GetConfigCallback configCallback;
    SetControlCallback controlCallback;
    SaveWiFiCallback saveWiFiCallback;
    GetStatusCallback scanWiFiCallback;

    /**
     * @brief Setup API routes
     */
    void setupRoutes();

    /**
     * @brief Setup provisioning routes
     */
    void setupProvisioningRoutes();

    /**
     * @brief Add CORS headers
     */
    void addCORSHeaders(AsyncWebServerResponse* response);

    /**
     * @brief Handle OPTIONS preflight requests
     */
    void handleOptions(AsyncWebServerRequest* request);
};

#endif // IOT_WEB_SERVER_H
