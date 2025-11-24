/**
 * @file IoTAPIClient.cpp
 * @brief Implementation of HTTP API client
 */

#include "IoTAPIClient.h"

IoTAPIClient::IoTAPIClient(IoTStorage* storage, IoTSyncManager* syncManager)
    : storage(storage),
      syncManager(syncManager),
      serverURL(""),
      projectId(""),
      deviceName(""),
      mongoDeviceId(""),
      firmwareVersion("1.0.0"),
      hardwareId(""),
      jwtToken(""),
      retryCount(3),
      retryDelay(2000),
      timeout(10000) {
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void IoTAPIClient::setServerURL(const String& url) {
    serverURL = url;
    // Remove trailing slash
    if (serverURL.endsWith("/")) {
        serverURL = serverURL.substring(0, serverURL.length() - 1);
    }
}

void IoTAPIClient::setDeviceInfo(const String& projectId, const String& deviceName,
                                const String& mongoDeviceId, const String& firmwareVersion) {
    this->projectId = projectId;
    this->deviceName = deviceName;
    this->mongoDeviceId = mongoDeviceId;
    this->firmwareVersion = firmwareVersion;
}

void IoTAPIClient::setHardwareId(const String& hardwareId) {
    this->hardwareId = hardwareId;
    if (storage) {
        storage->saveHardwareId(hardwareId);
    }
}

// ============================================================================
// AUTHENTICATION
// ============================================================================

bool IoTAPIClient::login(const String& username, const String& password) {
    Serial.println("[API] Attempting login...");

    DynamicJsonDocument doc(512);
    doc["username"] = username;
    doc["password"] = password;
    doc["hardwareId"] = hardwareId;

    String payload;
    serializeJson(doc, payload);

    String response;
    int statusCode = httpRequest("POST", "/api/device-auth/login", payload, response, false);

    if (statusCode == 200) {
        DynamicJsonDocument responseDoc(1024);
        DeserializationError error = deserializeJson(responseDoc, response);

        if (!error && responseDoc.containsKey("deviceToken")) {
            jwtToken = responseDoc["deviceToken"].as<String>();
            saveToken(jwtToken);
            Serial.println("[API] Login successful");
            return true;
        }
    }

    Serial.printf("[API] Login failed (HTTP %d)\n", statusCode);
    return false;
}


bool IoTAPIClient::isAuthenticated() {
    if (jwtToken.length() > 0) {
        return true;
    }
    return loadToken();
}

String IoTAPIClient::getToken() {
    return jwtToken;
}

// ============================================================================
// TIME SYNC
// ============================================================================

bool IoTAPIClient::syncTime() {
    Serial.println("[API] Syncing time...");

    String response;
    int statusCode = httpGET("/api/device/time", response);

    if (statusCode == 200) {
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, response);

        if (!error && doc.containsKey("timestamp")) {
            uint64_t timestamp = doc["timestamp"].as<uint64_t>();
            if (syncManager) {
                syncManager->setServerTime(timestamp);
            }
            Serial.printf("[API] Time synced: %llu\n", timestamp);
            return true;
        }
    }

    Serial.printf("[API] Time sync failed (HTTP %d)\n", statusCode);
    return false;
}

// ============================================================================
// SERVER CONNECTIVITY
// ============================================================================

bool IoTAPIClient::checkHeartbeat() {
    Serial.println("[API] Checking server heartbeat...");

    String response;
    int statusCode = httpGET("/api/device/heartbeat", response);

    if (statusCode == 200) {
        Serial.println("[API] Heartbeat OK - server is online");
        return true;
    }

    Serial.printf("[API] Heartbeat failed (HTTP %d)\n", statusCode);
    return false;
}

// ============================================================================
// GENERIC HTTP OPERATIONS
// ============================================================================

int IoTAPIClient::httpGET(const String& endpoint, String& response) {
    return httpRequest("GET", endpoint, "", response, true);
}

int IoTAPIClient::httpPOST(const String& endpoint, const String& payload, String& response) {
    return httpRequest("POST", endpoint, payload, response, true);
}

int IoTAPIClient::httpPUT(const String& endpoint, const String& payload, String& response) {
    return httpRequest("PUT", endpoint, payload, response, true);
}

int IoTAPIClient::httpPATCH(const String& endpoint, const String& payload, String& response) {
    return httpRequest("PATCH", endpoint, payload, response, true);
}

// ============================================================================
// RETRY LOGIC
// ============================================================================

void IoTAPIClient::setRetryCount(int count) {
    retryCount = count;
}

void IoTAPIClient::setRetryDelay(int delayMs) {
    retryDelay = delayMs;
}

void IoTAPIClient::setTimeout(int timeoutMs) {
    timeout = timeoutMs;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

int IoTAPIClient::httpRequest(const String& method, const String& endpoint,
                             const String& payload, String& response, bool useAuth) {
    if (serverURL.length() == 0) {
        Serial.println("[API] Error: Server URL not set");
        return 0;
    }

    String url = serverURL + endpoint;
    int attempts = 0;
    int statusCode = 0;

    while (attempts < retryCount) {
        attempts++;

        HTTPClient http;
        http.begin(url);
        http.setTimeout(timeout);
        http.addHeader("Content-Type", "application/json");

        if (useAuth) {
            addAuthHeader(http);
        }

        Serial.printf("[API] %s %s (attempt %d/%d)\n",
                     method.c_str(), endpoint.c_str(), attempts, retryCount);

        // Perform request
        if (method == "GET") {
            statusCode = http.GET();
        } else if (method == "POST") {
            statusCode = http.POST(payload);
        } else if (method == "PUT") {
            statusCode = http.PUT(payload);
        } else if (method == "PATCH") {
            statusCode = http.PATCH(payload);
        }

        if (statusCode > 0) {
            response = http.getString();
            http.end();

            if (statusCode >= 200 && statusCode < 300) {
                Serial.printf("[API] Success (HTTP %d)\n", statusCode);
                if (syncManager) {
                    syncManager->setServerOnline();
                }
                return statusCode;
            }

            Serial.printf("[API] Request failed (HTTP %d)\n", statusCode);

            // Don't retry on client errors (4xx)
            if (statusCode >= 400 && statusCode < 500) {
                break;
            }
        } else {
            Serial.printf("[API] Connection failed: %s\n", http.errorToString(statusCode).c_str());
            http.end();
        }

        // Retry delay
        if (attempts < retryCount) {
            delay(retryDelay);
        }
    }

    if (syncManager && statusCode <= 0) {
        syncManager->setServerOffline();
    }

    return statusCode;
}

void IoTAPIClient::addAuthHeader(HTTPClient& http) {
    if (jwtToken.length() > 0) {
        http.addHeader("Authorization", "Bearer " + jwtToken);
    }
}

bool IoTAPIClient::loadToken() {
    if (storage) {
        jwtToken = storage->loadDeviceToken();
        return jwtToken.length() > 0;
    }
    return false;
}

void IoTAPIClient::saveToken(const String& token) {
    jwtToken = token;
    if (storage) {
        storage->saveDeviceToken(token);
    }
}
