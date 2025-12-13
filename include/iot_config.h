#ifndef IOT_CONFIG_H
#define IOT_CONFIG_H

// ============================================================================
// IOT LIBRARY CONFIGURATION
// Configuration for ArduinoIoTDevice library
// ============================================================================

// ============================================================================
// BACKEND API CONFIGURATION
// ============================================================================

#define SERVER_URL "http://103.136.236.16"
#define PROJECT_ID "wt001"
#define DEVICE_NAME "DEV-02"
#define MONGODB_DEVICE_ID "690e6a9d092433c0acfb9178"
#define DEVICE_ID "wt001-DEV-02-690e6a9d092433c0acfb9178"
#define FIRMWARE_VERSION "1.0.0"

// Device Credentials
// Note: Device uses dashboard credentials from web portal (user-entered)
// Credentials are stored in NVS: PREF_DASHBOARD_USER and PREF_DASHBOARD_PASS
// User enters credentials via WiFi setup portal at http://192.168.4.1

// API Endpoints
#define API_FIRMWARE_LATEST "/api/firmware/latest"

// ============================================================================
// WIFI CONFIGURATION
// ============================================================================

#define AP_SSID "AquaFlow-Setup"
#define AP_PASSWORD "PassAquaWT001"
#define MDNS_HOSTNAME "watertank"       // mDNS hostname (device accessible at watertank.local)
#define WIFI_TIMEOUT_MS 20000           // 20 seconds
#define WIFI_RETRY_INTERVAL_MS 30000    // 30 seconds between reconnection attempts
#define WIFI_RECONNECT_INTERVAL 30000   // 30 seconds (legacy)
#define WIFI_TIMEOUT 20000               // 20 seconds (legacy)

// ============================================================================
// IOT SYNC INTERVALS (in milliseconds)
// ============================================================================

// How often to upload telemetry data to server
#define TELEMETRY_UPLOAD_INTERVAL 5000    // 5 seconds

// How often to fetch control data from server
#define CONTROL_FETCH_INTERVAL 2000       // 2 seconds

// How often to check for config updates
#define CONFIG_CHECK_INTERVAL 300000      // 5 minutes

// How often to check for firmware updates
#define OTA_CHECK_INTERVAL 300000         // 5 minutes

// Note: Heartbeat intervals are now defined in ArduinoIoTDevice.h library header
// Default values: HEARTBEAT_CHECK_INTERVAL=30000, HEARTBEAT_RETRY_INTERVAL=60000, MAX_HEARTBEAT_FAILURES=10

// ============================================================================
// API RETRY CONFIGURATION
// ============================================================================

#define API_RETRY_COUNT 1       // Number of retries for failed API requests
#define API_RETRY_DELAY_MS 2000 // Delay between retries
#define HTTP_TIMEOUT 5000       // HTTP request timeout (5 seconds)

// ============================================================================
// SETUP MODE CONFIGURATION
// ============================================================================

#define SETUP_MODE_TIMEOUT 600000    // 10 minutes
#define WIFI_SCAN_TIMEOUT 10000      // 10 seconds
#define WIFI_CONNECT_TIMEOUT 30000   // 30 seconds
#define WIFI_CONNECT_RETRIES 3

// ============================================================================
// PREFERENCES KEYS (NVS Storage)
// ============================================================================

#define PREF_NAMESPACE "watertank"
#define PREF_WIFI_SSID "wifi_ssid"
#define PREF_WIFI_PASS "wifi_pass"
#define PREF_WIFI_CONFIGURED "wifi_configured"
#define PREF_DASHBOARD_USER "dash_user"
#define PREF_DASHBOARD_PASS "dash_pass"
#define PREF_DEVICE_TOKEN "device_token"
#define PREF_HARDWARE_ID "hardware_id"
#define PREF_AUTO_MODE "auto_mode"

// Sync status keys
#define PREF_SERVER_SYNC "server_sync"
#define PREF_CONFIG_SYNC "config_sync"
#define PREF_SERVER_TIME "server_time"
#define PREF_MILLIS_SYNC "millis_sync"
#define PREF_OVERFLOW_CNT "overflow_cnt"

#endif // IOT_CONFIG_H
