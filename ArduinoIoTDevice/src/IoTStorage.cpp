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
        preferences.begin(namespaceName.c_str(), false);
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
    if (!initialized) begin();
    preferences.clear();
}

// ============================================================================
// WIFI CREDENTIALS
// ============================================================================

void IoTStorage::saveWiFiCredentials(const String& ssid, const String& password) {
    if (!initialized) begin();
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);
    preferences.putBool("wifi_configured", true);

    // Update cache
    cachedWiFiSSID = ssid;
    cachedWiFiPass = password;
    wifiCredentialsCached = true;
}

bool IoTStorage::loadWiFiCredentials(String& ssid, String& password) {
    if (!initialized) begin();

    // Return cached values if available
    if (wifiCredentialsCached) {
        ssid = cachedWiFiSSID;
        password = cachedWiFiPass;
        return ssid.length() > 0;
    }

    // Cache miss - load from NVS (shouldn't happen after begin())
    if (!preferences.getBool("wifi_configured", false)) {
        return false;
    }
    ssid = preferences.getString("wifi_ssid", "");
    password = preferences.getString("wifi_pass", "");

    // Update cache
    cachedWiFiSSID = ssid;
    cachedWiFiPass = password;
    wifiCredentialsCached = true;

    return ssid.length() > 0;
}

void IoTStorage::clearWiFiCredentials() {
    if (!initialized) begin();
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");
    preferences.putBool("wifi_configured", false);

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
    if (!initialized) begin();
    preferences.putString("dash_user", username);
    preferences.putString("dash_pass", password);

    // Update cache
    cachedDashboardUser = username;
    cachedDashboardPass = password;
    dashboardCredentialsCached = true;
}

bool IoTStorage::loadDashboardCredentials(String& username, String& password) {
    if (!initialized) begin();

    // Return cached values if available
    if (dashboardCredentialsCached) {
        username = cachedDashboardUser;
        password = cachedDashboardPass;
        return username.length() > 0;
    }

    // Cache miss - load from NVS (shouldn't happen after begin())
    username = preferences.getString("dash_user", "");
    password = preferences.getString("dash_pass", "");

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
    if (!initialized) begin();
    preferences.putString("device_token", token);
}

String IoTStorage::loadDeviceToken() {
    if (!initialized) begin();
    return preferences.getString("device_token", "");
}

void IoTStorage::clearDeviceToken() {
    if (!initialized) begin();
    preferences.remove("device_token");
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
    if (!initialized) begin();
    preferences.putString("hardware_id", hardwareId);
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
    if (!initialized) begin();
    preferences.putBool("server_sync", serverSync);
    preferences.putBool("config_sync", configSync);
    preferences.putULong64("server_time", serverTime);
    preferences.putULong64("millis_sync", millisAtSync);
    preferences.putUInt("overflow_cnt", overflowCount);
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
    if (!initialized) begin();
    preferences.putString(key.c_str(), value);
}

String IoTStorage::loadString(const String& key, const String& defaultValue) {
    if (!initialized) begin();
    return preferences.getString(key.c_str(), defaultValue);
}

void IoTStorage::saveBool(const String& key, bool value) {
    if (!initialized) begin();
    preferences.putBool(key.c_str(), value);
}

bool IoTStorage::loadBool(const String& key, bool defaultValue) {
    if (!initialized) begin();
    return preferences.getBool(key.c_str(), defaultValue);
}

void IoTStorage::saveUInt64(const String& key, uint64_t value) {
    if (!initialized) begin();
    preferences.putULong64(key.c_str(), value);
}

uint64_t IoTStorage::loadUInt64(const String& key, uint64_t defaultValue) {
    if (!initialized) begin();
    return preferences.getULong64(key.c_str(), defaultValue);
}

void IoTStorage::saveUInt32(const String& key, uint32_t value) {
    if (!initialized) begin();
    preferences.putUInt(key.c_str(), value);
}

uint32_t IoTStorage::loadUInt32(const String& key, uint32_t defaultValue) {
    if (!initialized) begin();
    return preferences.getUInt(key.c_str(), defaultValue);
}
