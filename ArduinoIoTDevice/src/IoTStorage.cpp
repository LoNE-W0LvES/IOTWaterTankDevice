/**
 * @file IoTStorage.cpp
 * @brief Implementation of NVS storage operations
 */

#include "IoTStorage.h"

IoTStorage::IoTStorage(const String& namespaceName)
    : namespaceName(namespaceName),
      initialized(false),
      dashboardCredentialsCached(false),
      wifiCredentialsCached(false) {
}

IoTStorage::~IoTStorage() {
    if (initialized) {
        preferences.end();
    }
}

void IoTStorage::begin() {
    if (!initialized) {
        // Try to open Preferences - this may fail if called during early boot
        // before nvs_flash_init() has been called
        if (!preferences.begin(namespaceName.c_str(), false)) {
            // Failed to initialize - likely called too early (before nvs_flash_init)
            // This is not an error - begin() will be called again later
            return;
        }

        initialized = true;

        // Load credentials into cache to avoid repeated NVS reads
        if (preferences.getBool("wifi_configured", false)) {
            cachedWiFiSSID = preferences.getString("wifi_ssid", "");
            cachedWiFiPass = preferences.getString("wifi_pass", "");
            wifiCredentialsCached = true;
        }

        cachedDashboardUser = preferences.getString("dash_user", "");
        cachedDashboardPass = preferences.getString("dash_pass", "");
        dashboardCredentialsCached = (cachedDashboardUser.length() > 0);

        if (dashboardCredentialsCached) {
            Serial.println("[Storage] Loaded dashboard credentials from NVS");
        }
        if (wifiCredentialsCached) {
            Serial.println("[Storage] Loaded WiFi credentials from NVS");
        }
    }
}

void IoTStorage::clearAll() {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.clear();

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;

    // Clear all caches
    cachedWiFiSSID = "";
    cachedWiFiPass = "";
    cachedDashboardUser = "";
    cachedDashboardPass = "";
    wifiCredentialsCached = false;
    dashboardCredentialsCached = false;
}

// ============================================================================
// WIFI CREDENTIALS
// ============================================================================

void IoTStorage::saveWiFiCredentials(const String& ssid, const String& password) {
    Serial.printf("[Storage] Saving WiFi credentials - SSID: %s\n", ssid.c_str());

    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);

    // Perform writes
    size_t written1 = preferences.putString("wifi_ssid", ssid);
    size_t written2 = preferences.putString("wifi_pass", password);
    size_t written3 = preferences.putBool("wifi_configured", true);

    Serial.printf("[Storage] NVS write results - SSID: %d bytes, Pass: %d bytes, Configured: %d bytes\n",
                  written1, written2, written3);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;

    // Update cache
    cachedWiFiSSID = ssid;
    cachedWiFiPass = password;
    wifiCredentialsCached = true;

    Serial.println("[Storage] WiFi credentials saved to NVS and cached");
}

bool IoTStorage::loadWiFiCredentials(String& ssid, String& password) {
    if (!initialized) begin();

    // Return cached values if available
    if (wifiCredentialsCached) {
        ssid = cachedWiFiSSID;
        password = cachedWiFiPass;
        Serial.printf("[Storage] Loading WiFi from cache - SSID: %s\n", ssid.c_str());
        return ssid.length() > 0;
    }

    // Cache miss - load from NVS (shouldn't happen after begin())
    Serial.println("[Storage] Cache miss - loading WiFi from NVS");
    if (!preferences.getBool("wifi_configured", false)) {
        Serial.println("[Storage] WiFi not configured in NVS");
        return false;
    }
    ssid = preferences.getString("wifi_ssid", "");
    password = preferences.getString("wifi_pass", "");

    Serial.printf("[Storage] Loaded from NVS - SSID: %s\n", ssid.c_str());

    // Update cache
    cachedWiFiSSID = ssid;
    cachedWiFiPass = password;
    wifiCredentialsCached = true;

    return ssid.length() > 0;
}

void IoTStorage::clearWiFiCredentials() {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    preferences.putBool("wifi_configured", false);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;

    // Clear cache
    cachedWiFiSSID = "";
    cachedWiFiPass = "";
    wifiCredentialsCached = false;
}

bool IoTStorage::hasWiFiCredentials() {
    if (!initialized) begin();
    return preferences.getBool("wifi_configured", false);
}

// ============================================================================
// DASHBOARD CREDENTIALS
// ============================================================================

void IoTStorage::saveDashboardCredentials(const String& username, const String& password) {
    Serial.printf("[Storage] Saving dashboard credentials - User: %s\n", username.c_str());

    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);

    // Perform writes
    size_t written1 = preferences.putString("dash_user", username);
    size_t written2 = preferences.putString("dash_pass", password);

    Serial.printf("[Storage] NVS write results - User: %d bytes, Pass: %d bytes\n",
                  written1, written2);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;

    // Update cache
    cachedDashboardUser = username;
    cachedDashboardPass = password;
    dashboardCredentialsCached = true;

    Serial.println("[Storage] Dashboard credentials saved to NVS and cached");
}

bool IoTStorage::loadDashboardCredentials(String& username, String& password) {
    if (!initialized) begin();

    // Return cached values if available
    if (dashboardCredentialsCached) {
        username = cachedDashboardUser;
        password = cachedDashboardPass;
        Serial.printf("[Storage] Loading dashboard from cache - User: %s\n", username.c_str());
        return username.length() > 0;
    }

    // Cache miss - load from NVS (shouldn't happen after begin())
    Serial.println("[Storage] Cache miss - loading dashboard from NVS");
    username = preferences.getString("dash_user", "");
    password = preferences.getString("dash_pass", "");

    if (username.length() > 0) {
        Serial.printf("[Storage] Loaded from NVS - User: %s\n", username.c_str());
    } else {
        Serial.println("[Storage] No dashboard credentials in NVS");
    }

    // Update cache
    cachedDashboardUser = username;
    cachedDashboardPass = password;
    dashboardCredentialsCached = (username.length() > 0);

    return username.length() > 0;
}

// ============================================================================
// DEVICE TOKEN
// ============================================================================

void IoTStorage::saveDeviceToken(const String& token) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putString("device_token", token);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

String IoTStorage::loadDeviceToken() {
    if (!initialized) begin();
    return preferences.getString("device_token", "");
}

void IoTStorage::clearDeviceToken() {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.remove("device_token");

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

bool IoTStorage::hasDeviceToken() {
    if (!initialized) begin();
    String token = preferences.getString("device_token", "");
    return token.length() > 0;
}

// ============================================================================
// HARDWARE ID
// ============================================================================

void IoTStorage::saveHardwareId(const String& hardwareId) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putString("hardware_id", hardwareId);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

String IoTStorage::loadHardwareId() {
    if (!initialized) begin();
    return preferences.getString("hardware_id", "");
}

// ============================================================================
// SYNC STATUS
// ============================================================================

void IoTStorage::saveSyncStatus(bool serverSync, bool configSync, uint64_t serverTime,
                                uint64_t millisAtSync, uint32_t overflowCount) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putBool("server_sync", serverSync);
    preferences.putBool("config_sync", configSync);
    preferences.putULong64("server_time", serverTime);
    preferences.putULong64("millis_sync", millisAtSync);
    preferences.putUInt("overflow_cnt", overflowCount);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

bool IoTStorage::loadSyncStatus(bool& serverSync, bool& configSync, uint64_t& serverTime,
                                uint64_t& millisAtSync, uint32_t& overflowCount) {
    if (!initialized) begin();
    serverSync = preferences.getBool("server_sync", false);
    configSync = preferences.getBool("config_sync", true);
    serverTime = preferences.getULong64("server_time", 0);
    millisAtSync = preferences.getULong64("millis_sync", 0);
    overflowCount = preferences.getUInt("overflow_cnt", 0);
    return true;
}

// ============================================================================
// GENERIC OPERATIONS
// ============================================================================

void IoTStorage::saveString(const String& key, const String& value) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putString(key.c_str(), value);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

String IoTStorage::loadString(const String& key, const String& defaultValue) {
    if (!initialized) begin();
    return preferences.getString(key.c_str(), defaultValue);
}

void IoTStorage::saveBool(const String& key, bool value) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putBool(key.c_str(), value);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

bool IoTStorage::loadBool(const String& key, bool defaultValue) {
    if (!initialized) begin();
    return preferences.getBool(key.c_str(), defaultValue);
}

void IoTStorage::saveUInt64(const String& key, uint64_t value) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putULong64(key.c_str(), value);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

uint64_t IoTStorage::loadUInt64(const String& key, uint64_t defaultValue) {
    if (!initialized) begin();
    return preferences.getULong64(key.c_str(), defaultValue);
}

void IoTStorage::saveUInt32(const String& key, uint32_t value) {
    // Close any existing Preferences connection
    if (initialized) {
        preferences.end();
        initialized = false;
    }

    // Open namespace for this specific operation
    preferences.begin(namespaceName.c_str(), false);
    preferences.putUInt(key.c_str(), value);

    // Close immediately to commit to flash
    preferences.end();
    initialized = false;
}

uint32_t IoTStorage::loadUInt32(const String& key, uint32_t defaultValue) {
    if (!initialized) begin();
    return preferences.getUInt(key.c_str(), defaultValue);
}
