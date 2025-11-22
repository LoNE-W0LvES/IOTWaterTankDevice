/**
 * @file IoTAPIClient.h
 * @brief HTTP API client with JWT authentication
 *
 * Handles server communication, authentication, and data exchange.
 */

#ifndef IOT_API_CLIENT_H
#define IOT_API_CLIENT_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "IoTStorage.h"
#include "IoTSyncManager.h"

class IoTAPIClient {
public:
    IoTAPIClient(IoTStorage* storage, IoTSyncManager* syncManager);

    // ========================================================================
    // CONFIGURATION
    // ========================================================================

    /**
     * @brief Set server URL
     * @param url Server base URL (e.g., "http://192.168.1.100")
     */
    void setServerURL(const String& url);

    /**
     * @brief Set device identifiers
     */
    void setDeviceInfo(const String& projectId, const String& deviceName,
                      const String& mongoDeviceId, const String& firmwareVersion);

    /**
     * @brief Set hardware ID (MAC address)
     */
    void setHardwareId(const String& hardwareId);

    // ========================================================================
    // AUTHENTICATION
    // ========================================================================

    /**
     * @brief Login with dashboard credentials
     * @return true if login successful and token obtained
     */
    bool login(const String& username, const String& password);

    /**
     * @brief Register device (first-time setup)
     * @return true if registration successful
     */
    bool registerDevice();

    /**
     * @brief Check if authenticated (has valid token)
     */
    bool isAuthenticated();

    /**
     * @brief Get JWT token
     */
    String getToken();

    // ========================================================================
    // TIME SYNC
    // ========================================================================

    /**
     * @brief Sync time with server (NTP-like)
     * @return true if sync successful
     */
    bool syncTime();

    // ========================================================================
    // GENERIC HTTP OPERATIONS
    // ========================================================================

    /**
     * @brief Perform GET request
     * @param endpoint API endpoint (e.g., "/api/config")
     * @param response Response string (output)
     * @return HTTP status code (200 = success, 0 = error)
     */
    int httpGET(const String& endpoint, String& response);

    /**
     * @brief Perform POST request
     * @param endpoint API endpoint
     * @param payload JSON payload
     * @param response Response string (output)
     * @return HTTP status code (200 = success, 0 = error)
     */
    int httpPOST(const String& endpoint, const String& payload, String& response);

    /**
     * @brief Perform PUT request
     */
    int httpPUT(const String& endpoint, const String& payload, String& response);

    /**
     * @brief Perform PATCH request
     */
    int httpPATCH(const String& endpoint, const String& payload, String& response);

    // ========================================================================
    // RETRY LOGIC
    // ========================================================================

    /**
     * @brief Set retry count for failed requests (default: 3)
     */
    void setRetryCount(int count);

    /**
     * @brief Set retry delay in milliseconds (default: 2000)
     */
    void setRetryDelay(int delayMs);

    /**
     * @brief Set HTTP timeout in milliseconds (default: 10000)
     */
    void setTimeout(int timeoutMs);

private:
    IoTStorage* storage;
    IoTSyncManager* syncManager;

    String serverURL;
    String projectId;
    String deviceName;
    String mongoDeviceId;
    String firmwareVersion;
    String hardwareId;
    String jwtToken;

    // Configuration
    int retryCount;
    int retryDelay;
    int timeout;

    /**
     * @brief Perform HTTP request with retry logic
     */
    int httpRequest(const String& method, const String& endpoint,
                   const String& payload, String& response, bool useAuth = true);

    /**
     * @brief Add authorization header
     */
    void addAuthHeader(HTTPClient& http);

    /**
     * @brief Load token from storage
     */
    bool loadToken();

    /**
     * @brief Save token to storage
     */
    void saveToken(const String& token);
};

#endif // IOT_API_CLIENT_H
