/**
 * @file IoTStorage.h
 * @brief NVS (Non-Volatile Storage) management for IoT device
 *
 * Handles persistent storage of WiFi credentials, tokens, and sync status.
 */

#ifndef IOT_STORAGE_H
#define IOT_STORAGE_H

#include <Arduino.h>
#include <Preferences.h>

class IoTStorage {
public:
    IoTStorage(const String& namespaceName = "iot_device");
    ~IoTStorage();

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize storage (call once in setup)
     */
    void begin();

    /**
     * @brief Clear all stored data
     */
    void clearAll();

    // ========================================================================
    // WIFI CREDENTIALS
    // ========================================================================

    /**
     * @brief Save WiFi credentials
     */
    void saveWiFiCredentials(const String& ssid, const String& password);

    /**
     * @brief Load WiFi credentials
     * @return true if credentials exist
     */
    bool loadWiFiCredentials(String& ssid, String& password);

    /**
     * @brief Clear WiFi credentials
     */
    void clearWiFiCredentials();

    /**
     * @brief Check if WiFi credentials are configured
     */
    bool hasWiFiCredentials();

    // ========================================================================
    // DASHBOARD CREDENTIALS
    // ========================================================================

    /**
     * @brief Save dashboard login credentials
     */
    void saveDashboardCredentials(const String& username, const String& password);

    /**
     * @brief Load dashboard credentials
     */
    bool loadDashboardCredentials(String& username, String& password);

    // ========================================================================
    // DEVICE TOKEN
    // ========================================================================

    /**
     * @brief Save JWT device token
     */
    void saveDeviceToken(const String& token);

    /**
     * @brief Load JWT device token
     */
    String loadDeviceToken();

    /**
     * @brief Clear device token
     */
    void clearDeviceToken();

    /**
     * @brief Check if device token exists
     */
    bool hasDeviceToken();

    // ========================================================================
    // HARDWARE ID
    // ========================================================================

    /**
     * @brief Save hardware ID
     */
    void saveHardwareId(const String& hardwareId);

    /**
     * @brief Load hardware ID
     */
    String loadHardwareId();

    // ========================================================================
    // SYNC STATUS
    // ========================================================================

    /**
     * @brief Save sync status
     */
    void saveSyncStatus(bool serverSync, bool configSync, uint64_t serverTime,
                       uint64_t millisAtSync, uint32_t overflowCount);

    /**
     * @brief Load sync status
     */
    bool loadSyncStatus(bool& serverSync, bool& configSync, uint64_t& serverTime,
                       uint64_t& millisAtSync, uint32_t& overflowCount);

    // ========================================================================
    // GENERIC OPERATIONS
    // ========================================================================

    /**
     * @brief Save string value
     */
    void saveString(const String& key, const String& value);

    /**
     * @brief Load string value
     */
    String loadString(const String& key, const String& defaultValue = "");

    /**
     * @brief Save boolean value
     */
    void saveBool(const String& key, bool value);

    /**
     * @brief Load boolean value
     */
    bool loadBool(const String& key, bool defaultValue = false);

    /**
     * @brief Save unsigned 64-bit value
     */
    void saveUInt64(const String& key, uint64_t value);

    /**
     * @brief Load unsigned 64-bit value
     */
    uint64_t loadUInt64(const String& key, uint64_t defaultValue = 0);

    /**
     * @brief Save unsigned 32-bit value
     */
    void saveUInt32(const String& key, uint32_t value);

    /**
     * @brief Load unsigned 32-bit value
     */
    uint32_t loadUInt32(const String& key, uint32_t defaultValue = 0);

private:
    Preferences preferences;
    String namespaceName;
    bool initialized;

    // Cached values to avoid repeated NVS reads
    String cachedDashboardUser;
    String cachedDashboardPass;
    bool dashboardCredentialsCached;

    String cachedWiFiSSID;
    String cachedWiFiPass;
    bool wifiCredentialsCached;
};

#endif // IOT_STORAGE_H
