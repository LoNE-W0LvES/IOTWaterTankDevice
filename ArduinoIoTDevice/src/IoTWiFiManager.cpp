/**
 * @file IoTWiFiManager.cpp
 * @brief Implementation of WiFi management
 */

#include "IoTWiFiManager.h"
#include <WiFi.h>
#include <ArduinoJson.h>

IoTWiFiManager::IoTWiFiManager(IoTStorage* storage)
    : storage(storage),
      currentMode(IOT_WIFI_DISABLED),
      currentStatus(IOT_WIFI_IDLE),
      deviceId(""),
      apPassword("iot-setup-password"),
      savedSSID(""),
      savedPassword(""),
      connectStartTime(0),
      lastReconnectAttempt(0),
      connectionAttempts(0) {
}

void IoTWiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);  // We handle reconnection manually
    loadCredentials();
}

void IoTWiFiManager::setDeviceId(const String& deviceId) {
    this->deviceId = deviceId;
}

void IoTWiFiManager::setAPPassword(const String& password) {
    this->apPassword = password;
}

// ============================================================================
// CLIENT MODE
// ============================================================================

bool IoTWiFiManager::startClient() {
    if (!loadCredentials() || savedSSID.length() == 0) {
        Serial.println("[WiFi] No saved credentials");
        return false;
    }
    return startClient(savedSSID, savedPassword);
}

bool IoTWiFiManager::startClient(const String& ssid, const String& password) {
    Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());

    currentMode = IOT_WIFI_CLIENT_MODE;
    currentStatus = IOT_WIFI_CONNECTING;
    connectStartTime = millis();
    connectionAttempts++;

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    return true;
}

bool IoTWiFiManager::updateConnection() {
    if (currentMode != IOT_WIFI_CLIENT_MODE) {
        return false;
    }

    wl_status_t wifiStatus = WiFi.status();

    switch (currentStatus) {
        case IOT_WIFI_CONNECTING:
            if (wifiStatus == WL_CONNECTED) {
                currentStatus = IOT_WIFI_CONNECTED;
                connectionAttempts = 0;
                Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
                return true;
            } else if (isConnectTimeout()) {
                currentStatus = IOT_WIFI_FAILED;
                Serial.println("[WiFi] Connection timeout");

                if (connectionAttempts >= IOT_MAX_CONNECT_ATTEMPTS) {
                    Serial.println("[WiFi] Max attempts reached, starting AP mode");
                    startAP();
                }
                return false;
            }
            break;

        case IOT_WIFI_CONNECTED:
            if (wifiStatus != WL_CONNECTED) {
                currentStatus = IOT_WIFI_DISCONNECTED;
                Serial.println("[WiFi] Disconnected");
                lastReconnectAttempt = millis();
                return false;
            }
            return true;

        case IOT_WIFI_DISCONNECTED:
        case IOT_WIFI_FAILED:
            // Auto-reconnect logic
            if (millis() - lastReconnectAttempt > IOT_WIFI_RECONNECT_INTERVAL) {
                Serial.println("[WiFi] Attempting reconnect...");
                startClient();
            }
            break;

        default:
            break;
    }

    return false;
}

void IoTWiFiManager::disconnect() {
    WiFi.disconnect();
    currentStatus = IOT_WIFI_DISCONNECTED;
}

// ============================================================================
// AP MODE
// ============================================================================

void IoTWiFiManager::setCustomAPSSID(const String& ssid) {
    customAPSSID = ssid;
}

void IoTWiFiManager::startAP() {
    String apSSID;

    // Use custom SSID if set, otherwise construct from deviceId
    if (customAPSSID.length() > 0) {
        apSSID = customAPSSID;
    } else {
        apSSID = "IoTDevice-" + deviceId;
        if (deviceId.length() == 0) {
            apSSID = "IoTDevice-" + getMACAddress().substring(9);  // Last 8 chars of MAC
        }
    }

    Serial.printf("[WiFi] Starting AP: %s\n", apSSID.c_str());

    // Use AP+STA mode to allow WiFi scanning while AP is active
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(apSSID.c_str(), apPassword.c_str());

    currentMode = IOT_WIFI_AP_MODE;
    currentStatus = IOT_WIFI_CONNECTED;

    Serial.printf("[WiFi] AP Started (AP+STA mode). IP: %s\n", WiFi.softAPIP().toString().c_str());
}

void IoTWiFiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    currentMode = IOT_WIFI_DISABLED;
    currentStatus = IOT_WIFI_IDLE;
}

// ============================================================================
// STATUS & INFO
// ============================================================================

bool IoTWiFiManager::isConnected() {
    // Only return true if connected as CLIENT (not in AP mode)
    return currentMode == IOT_WIFI_CLIENT_MODE && currentStatus == IOT_WIFI_CONNECTED;
}

IoTWiFiMode IoTWiFiManager::getMode() {
    return currentMode;
}

IoTWiFiStatus IoTWiFiManager::getStatus() {
    return currentStatus;
}

String IoTWiFiManager::getStatusString() {
    switch (currentStatus) {
        case IOT_WIFI_IDLE:         return "Idle";
        case IOT_WIFI_CONNECTING:   return "Connecting";
        case IOT_WIFI_CONNECTED:    return "Connected";
        case IOT_WIFI_DISCONNECTED: return "Disconnected";
        case IOT_WIFI_FAILED:       return "Failed";
        default:                    return "Unknown";
    }
}

String IoTWiFiManager::getIPAddress() {
    if (currentMode == IOT_WIFI_CLIENT_MODE) {
        return WiFi.localIP().toString();
    } else if (currentMode == IOT_WIFI_AP_MODE) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String IoTWiFiManager::getMACAddress() {
    return WiFi.macAddress();
}

int IoTWiFiManager::getRSSI() {
    return WiFi.RSSI();
}

String IoTWiFiManager::getSSID() {
    return WiFi.SSID();
}

String IoTWiFiManager::getAPSSID() {
    // Return custom SSID if set, otherwise construct from deviceId
    if (customAPSSID.length() > 0) {
        return customAPSSID;
    }

    String apSSID = "IoTDevice-" + deviceId;
    if (deviceId.length() == 0) {
        apSSID = "IoTDevice-" + getMACAddress().substring(9);  // Last 8 chars of MAC
    }
    return apSSID;
}

String IoTWiFiManager::getAPPassword() {
    return apPassword;
}

bool IoTWiFiManager::isAPMode() {
    return currentMode == IOT_WIFI_AP_MODE;
}

// ============================================================================
// NETWORK SCANNING
// ============================================================================

String IoTWiFiManager::scanNetworks() {
    Serial.println("[WiFi] Scanning networks...");

    int n = WiFi.scanNetworks();

    DynamicJsonDocument doc(4096);
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < n; i++) {
        JsonObject network = networks.createNestedObject();
        network["ssid"] = WiFi.SSID(i);
        network["signal"] = WiFi.RSSI(i);
        network["auth"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "Open" : "Secured";
    }

    String result;
    serializeJson(doc, result);

    Serial.printf("[WiFi] Found %d networks\n", n);

    return result;
}

// ============================================================================
// CREDENTIALS
// ============================================================================

void IoTWiFiManager::saveCredentials(const String& ssid, const String& password) {
    savedSSID = ssid;
    savedPassword = password;
    if (storage) {
        storage->saveWiFiCredentials(ssid, password);
    }
    Serial.printf("[WiFi] Credentials saved: %s\n", ssid.c_str());
}

void IoTWiFiManager::clearCredentials() {
    savedSSID = "";
    savedPassword = "";
    if (storage) {
        storage->clearWiFiCredentials();
    }
    Serial.println("[WiFi] Credentials cleared");
}

bool IoTWiFiManager::hasCredentials() {
    if (storage) {
        return storage->hasWiFiCredentials();
    }
    return savedSSID.length() > 0;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

bool IoTWiFiManager::loadCredentials() {
    if (storage && storage->loadWiFiCredentials(savedSSID, savedPassword)) {
        Serial.printf("[WiFi] Loaded credentials for: %s\n", savedSSID.c_str());
        return true;
    }
    return false;
}

bool IoTWiFiManager::isConnectTimeout() {
    return (millis() - connectStartTime) > IOT_WIFI_CONNECT_TIMEOUT;
}
