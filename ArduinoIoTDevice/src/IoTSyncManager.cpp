/**
 * @file IoTSyncManager.cpp
 * @brief Implementation of sync manager
 */

#include "IoTSyncManager.h"

IoTSyncManager::IoTSyncManager(IoTStorage* storage)
    : storage(storage), lastMillis(0) {
}

void IoTSyncManager::begin() {
    load();
    lastMillis = millis();
}

// ============================================================================
// TIME SYNCHRONIZATION
// ============================================================================

void IoTSyncManager::setServerTime(uint64_t timestamp) {
    status.lastServerTimestamp = timestamp;
    status.millisAtSync = millis();
    status.millisOverflowCount = 0;
    lastMillis = status.millisAtSync;

    Serial.printf("[Sync] Time synced: %llu ms\n", timestamp);

    save();
}

uint64_t IoTSyncManager::getCurrentTimestamp() {
    if (status.lastServerTimestamp == 0) {
        return 0;  // Not synced yet
    }

    checkMillisOverflow();

    uint32_t currentMillis = millis();
    uint64_t elapsedMillis;

    // Calculate elapsed time accounting for overflows
    if (currentMillis >= status.millisAtSync) {
        elapsedMillis = currentMillis - status.millisAtSync;
    } else {
        // Overflow occurred
        elapsedMillis = (0xFFFFFFFF - status.millisAtSync) + currentMillis;
    }

    // Add overflow cycles (49.7 days each)
    elapsedMillis += (uint64_t)status.millisOverflowCount * 0x100000000ULL;

    return status.lastServerTimestamp + elapsedMillis;
}

bool IoTSyncManager::isTimeSynced() {
    return status.lastServerTimestamp > 0;
}

void IoTSyncManager::update() {
    checkMillisOverflow();
}

// ============================================================================
// SERVER STATUS
// ============================================================================

void IoTSyncManager::setServerOnline() {
    if (!status.serverOnline) {
        status.serverOnline = true;
        Serial.println("[Sync] Server online");
        save();
    }
}

void IoTSyncManager::setServerOffline() {
    if (status.serverOnline) {
        status.serverOnline = false;
        Serial.println("[Sync] Server offline");
        save();
    }
}

bool IoTSyncManager::isServerOnline() {
    return status.serverOnline;
}

// ============================================================================
// CONFIG SYNC PRIORITY
// ============================================================================

void IoTSyncManager::markDeviceConfigModified() {
    if (!status.deviceConfigPriority) {
        status.deviceConfigPriority = true;
        Serial.println("[Sync] Device config modified - will sync TO server");
        save();
    }
}

void IoTSyncManager::clearDevicePriority() {
    if (status.deviceConfigPriority) {
        status.deviceConfigPriority = false;
        Serial.println("[Sync] Device priority cleared - will sync FROM server");
        save();
    }
}

bool IoTSyncManager::hasDevicePriority() {
    return status.deviceConfigPriority;
}

// ============================================================================
// STORAGE
// ============================================================================

void IoTSyncManager::save() {
    if (storage) {
        storage->saveSyncStatus(
            status.serverOnline,
            status.deviceConfigPriority,
            status.lastServerTimestamp,
            status.millisAtSync,
            status.millisOverflowCount
        );
    }
}

void IoTSyncManager::load() {
    if (storage) {
        storage->loadSyncStatus(
            status.serverOnline,
            status.deviceConfigPriority,
            status.lastServerTimestamp,
            status.millisAtSync,
            status.millisOverflowCount
        );
        Serial.printf("[Sync] Loaded sync status - Server: %s, Priority: %s\n",
                     status.serverOnline ? "online" : "offline",
                     status.deviceConfigPriority ? "device" : "server");
    }
}

IoTSyncStatus IoTSyncManager::getStatus() {
    return status;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void IoTSyncManager::checkMillisOverflow() {
    uint32_t currentMillis = millis();

    // Detect overflow (millis() wrapped from max to 0)
    if (currentMillis < lastMillis) {
        status.millisOverflowCount++;
        Serial.printf("[Sync] millis() overflow detected (count: %u)\n",
                     status.millisOverflowCount);
        save();
    }

    lastMillis = currentMillis;
}
