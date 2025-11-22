/**
 * @file IoTSyncManager.h
 * @brief Synchronization manager for time and configuration sync
 *
 * Handles:
 * - NTP time synchronization with overflow protection
 * - Priority-based configuration sync (device vs server)
 * - Online/offline state management
 */

#ifndef IOT_SYNC_MANAGER_H
#define IOT_SYNC_MANAGER_H

#include <Arduino.h>
#include "IoTStorage.h"

/**
 * @brief Sync status structure
 */
struct IoTSyncStatus {
    bool serverOnline;              // true = server reachable, false = offline
    bool deviceConfigPriority;      // true = device has local changes (sync TO server)
                                    // false = sync FROM server normally
    uint64_t lastServerTimestamp;   // Last server time in milliseconds
    uint64_t millisAtSync;          // millis() when last synced
    uint32_t millisOverflowCount;   // Handle millis() overflow (every ~49 days)

    IoTSyncStatus()
        : serverOnline(false),
          deviceConfigPriority(false),
          lastServerTimestamp(0),
          millisAtSync(0),
          millisOverflowCount(0) {}
};

class IoTSyncManager {
public:
    IoTSyncManager(IoTStorage* storage);

    // ========================================================================
    // INITIALIZATION
    // ========================================================================

    /**
     * @brief Initialize sync manager (loads saved state)
     */
    void begin();

    // ========================================================================
    // TIME SYNCHRONIZATION
    // ========================================================================

    /**
     * @brief Set current timestamp from server
     * @param timestamp Unix timestamp in milliseconds
     */
    void setServerTime(uint64_t timestamp);

    /**
     * @brief Get current calculated timestamp
     * @return Unix timestamp in milliseconds
     */
    uint64_t getCurrentTimestamp();

    /**
     * @brief Check if time is synchronized
     */
    bool isTimeSynced();

    /**
     * @brief Update timestamp (call periodically to handle millis overflow)
     */
    void update();

    // ========================================================================
    // SERVER STATUS
    // ========================================================================

    /**
     * @brief Mark server as online
     */
    void setServerOnline();

    /**
     * @brief Mark server as offline
     */
    void setServerOffline();

    /**
     * @brief Check if server is online
     */
    bool isServerOnline();

    // ========================================================================
    // CONFIG SYNC PRIORITY
    // ========================================================================

    /**
     * @brief Mark device config as modified (sets device priority)
     * Next sync will send TO server with priority
     */
    void markDeviceConfigModified();

    /**
     * @brief Clear device priority (normal sync FROM server)
     */
    void clearDevicePriority();

    /**
     * @brief Check if device has config changes to upload
     */
    bool hasDevicePriority();

    // ========================================================================
    // STORAGE
    // ========================================================================

    /**
     * @brief Save sync status to persistent storage
     */
    void save();

    /**
     * @brief Load sync status from persistent storage
     */
    void load();

    /**
     * @brief Get current sync status
     */
    IoTSyncStatus getStatus();

private:
    IoTStorage* storage;
    IoTSyncStatus status;
    uint32_t lastMillis;  // For overflow detection

    /**
     * @brief Check for millis() overflow
     */
    void checkMillisOverflow();
};

#endif // IOT_SYNC_MANAGER_H
