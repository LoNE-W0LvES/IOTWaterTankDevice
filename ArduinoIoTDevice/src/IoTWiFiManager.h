/**
 * @file IoTWiFiManager.h
 * @brief WiFi connection management with AP provisioning support
 *
 * Handles both WiFi client mode and Access Point provisioning mode.
 */

#ifndef IOT_WIFI_MANAGER_H
#define IOT_WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include "IoTStorage.h"

// WiFi operation modes
enum IoTWiFiMode {
    IOT_WIFI_CLIENT_MODE,
    IOT_WIFI_AP_MODE,
    IOT_WIFI_DISABLED
};

// WiFi connection status
enum IoTWiFiStatus {
    IOT_WIFI_IDLE,
    IOT_WIFI_CONNECTING,
    IOT_WIFI_CONNECTED,
    IOT_WIFI_DISCONNECTED,
    IOT_WIFI_FAILED
};

class IoTWiFiManager {
public:
    IoTWiFiManager(IoTStorage* storage);

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize WiFi manager
     */
    void begin();

    /**
     * @brief Set device ID for AP SSID
     */
    void setDeviceId(const String& deviceId);

    /**
     * @brief Set AP password (default: "iot-setup-password")
     */
    void setAPPassword(const String& password);

    // ========================================================================
    // CLIENT MODE
    // ========================================================================

    /**
     * @brief Start WiFi client mode (non-blocking)
     * @return true if connection initiated
     */
    bool startClient();

    /**
     * @brief Start WiFi client with explicit credentials
     */
    bool startClient(const String& ssid, const String& password);

    /**
     * @brief Update WiFi connection status (call in loop)
     * @return true if connected
     */
    bool updateConnection();

    /**
     * @brief Disconnect from WiFi
     */
    void disconnect();

    // ========================================================================
    // AP MODE (Provisioning)
    // ========================================================================

    /**
     * @brief Start Access Point mode for provisioning
     * AP SSID format: "IoTDevice-{deviceId}"
     */
    void startAP();

    /**
     * @brief Stop Access Point mode
     */
    void stopAP();

    // ========================================================================
    // STATUS & INFO
    // ========================================================================

    /**
     * @brief Check if WiFi is connected
     */
    bool isConnected();

    /**
     * @brief Get current WiFi mode
     */
    IoTWiFiMode getMode();

    /**
     * @brief Get connection status
     */
    IoTWiFiStatus getStatus();

    /**
     * @brief Get status as string
     */
    String getStatusString();

    /**
     * @brief Get IP address
     */
    String getIPAddress();

    /**
     * @brief Get MAC address (hardware ID)
     */
    String getMACAddress();

    /**
     * @brief Get RSSI (signal strength)
     */
    int getRSSI();

    /**
     * @brief Get connected SSID
     */
    String getSSID();

    // ========================================================================
    // NETWORK SCANNING
    // ========================================================================

    /**
     * @brief Scan for WiFi networks
     * @return JSON string with network list
     */
    String scanNetworks();

    // ========================================================================
    // CREDENTIALS
    // ========================================================================

    /**
     * @brief Save WiFi credentials
     */
    void saveCredentials(const String& ssid, const String& password);

    /**
     * @brief Clear saved credentials
     */
    void clearCredentials();

    /**
     * @brief Check if credentials are saved
     */
    bool hasCredentials();

private:
    IoTStorage* storage;
    IoTWiFiMode currentMode;
    IoTWiFiStatus currentStatus;
    String deviceId;
    String apPassword;
    String savedSSID;
    String savedPassword;
    unsigned long connectStartTime;
    unsigned long lastReconnectAttempt;
    int connectionAttempts;

    // Configuration
    static const int IOT_WIFI_CONNECT_TIMEOUT = 20000;      // 20 seconds
    static const int IOT_WIFI_RECONNECT_INTERVAL = 30000;   // 30 seconds
    static const int IOT_MAX_CONNECT_ATTEMPTS = 3;

    /**
     * @brief Load saved credentials from storage
     */
    bool loadCredentials();

    /**
     * @brief Check connection timeout
     */
    bool isConnectTimeout();
};

#endif // IOT_WIFI_MANAGER_H
