# ArduinoIoTDevice Library

**Complete IoT device management library for ESP32 Arduino projects**

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-green.svg)](library.properties)

## Overview

ArduinoIoTDevice is a comprehensive library that handles all the complexity of IoT device management, allowing you to focus on your application logic. It provides:

- ✅ **WiFi Management**: Client mode + AP provisioning with automatic reconnection
- ✅ **Server Sync**: JWT authentication, NTP time sync, bidirectional config sync
- ✅ **Custom Data Structures**: Define your own config, control, and telemetry structures
- ✅ **Automatic Timestamps**: Built-in timestamp tracking for all data fields
- ✅ **Priority-Based Sync**: Smart conflict resolution (device vs server changes)
- ✅ **Local Storage**: NVS persistent storage for credentials and sync state
- ✅ **Auto-Upload**: Configurable automatic telemetry and control data sync
- ✅ **Easy API**: Simple, intuitive interface with minimal boilerplate

## Features

### WiFi Connection & Provisioning
- **Client Mode**: Connect to existing WiFi with saved or explicit credentials
- **AP Mode**: Built-in Access Point for WiFi provisioning
- **Auto-Reconnect**: Handles disconnections and retries automatically
- **Network Scanning**: Scan and list available WiFi networks
- **Credential Storage**: Securely save WiFi and dashboard credentials to NVS

### Server Communication
- **JWT Authentication**: Login and register device with server
- **HTTP Client**: Built-in GET, POST, PUT, PATCH with retry logic
- **NTP Time Sync**: Synchronize time with server (with millis() overflow protection)
- **Auto-Reconnect**: Automatic server connection recovery

### Data Synchronization
- **Bidirectional Sync**: Config can sync FROM server or TO server
- **Priority Resolution**: Device changes override server when needed
- **Timestamp Tracking**: Every field has automatic lastModified timestamp
- **Conflict Detection**: Smart 3-way merge for configuration updates
- **Auto-Upload**: Periodic telemetry and control data sync

### Custom Data Structures
- **Type-Safe Fields**: Use `IoTField<Type>` for any data type
- **Easy Access**: `deviceConfig.fieldName.value` and `.lastModified`
- **Automatic Serialization**: JSON serialization handled by library
- **Flexible Schema**: Define exactly the fields your application needs

## Installation

### Arduino Library Manager
1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for "ArduinoIoTDevice"
4. Click Install

### Manual Installation
1. Download the latest release
2. Extract to `Arduino/libraries/ArduinoIoTDevice`
3. Restart Arduino IDE

### PlatformIO
Add to `platformio.ini`:
```ini
lib_deps =
    ArduinoIoTDevice
```

## Dependencies

The library requires these dependencies (auto-installed):
- **ArduinoJson** (>=6.21.0) - JSON serialization
- **ESPAsyncWebServer** - Web server for local API (optional)
- **AsyncTCP** - Async TCP for web server (optional)

## Quick Start

### 1. Define Your Data Structures

```cpp
#include <ArduinoIoTDevice.h>

// Define device configuration
struct MyDeviceConfig {
    IoTField<String> ipAddress{"ipAddress", "192.168.1.100"};
    IoTField<float> threshold{"threshold", 25.0f};
    IoTField<bool> autoMode{"autoMode", true};
};

// Define control data (commands from server)
struct MyControlData {
    IoTField<bool> relay{"relay", false};
    IoTField<bool> config_update{"config_update", false};
};

// Define telemetry data (sensor readings to send)
struct MyTelemetryData {
    IoTField<float> temperature{"temperature", 0.0f};
    IoTField<float> humidity{"humidity", 0.0f};
    IoTField<int> Status{"Status", 1};  // 1 = online
};
```

### 2. Create IoT Device Instance

```cpp
IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;
```

### 3. Initialize in setup()

```cpp
void setup() {
    Serial.begin(115200);

    // Initialize device
    iotDevice.begin();

    // Configure server
    iotDevice.configureServer(
        "http://your-server.com",  // Server URL
        "project001",               // Project ID
        "device001",                // Device name
        "mongoDbId123",             // MongoDB device ID
        "1.0.0"                     // Firmware version
    );

    // Set device ID for AP mode
    iotDevice.setDeviceId("MyDevice-001");

    // Connect to WiFi
    iotDevice.connectWiFi("WiFi-SSID", "password");

    // Wait for connection
    while (!iotDevice.isWiFiConnected()) {
        iotDevice.update();
        delay(100);
    }

    // Login to server
    iotDevice.login("username", "password");

    // Sync time
    iotDevice.syncTimeWithServer();

    // Fetch initial config
    iotDevice.fetchDeviceConfig();
}
```

### 4. Use in loop()

```cpp
void loop() {
    // Update device (handles WiFi, sync, auto-upload)
    iotDevice.update();

    // Read config values
    float threshold = iotDevice.deviceConfig.threshold.value;
    bool autoMode = iotDevice.deviceConfig.autoMode.value;

    // Update telemetry
    iotDevice.telemetryData.temperature.value = readTemperature();
    iotDevice.telemetryData.humidity.value = readHumidity();

    // Check control commands
    if (iotDevice.controlData.relay.value) {
        digitalWrite(RELAY_PIN, HIGH);
    } else {
        digitalWrite(RELAY_PIN, LOW);
    }

    delay(100);
}
```

## Usage Examples

### Accessing Field Values

```cpp
// Read field value
float value = iotDevice.deviceConfig.threshold.value;

// Read timestamp
uint64_t timestamp = iotDevice.deviceConfig.threshold.lastModified;

// Check field metadata
String key = iotDevice.deviceConfig.threshold.key;
String label = iotDevice.deviceConfig.threshold.label;
```

### Updating Fields

```cpp
// Method 1: Direct assignment
iotDevice.deviceConfig.threshold.value = 30.0f;
iotDevice.deviceConfig.threshold.lastModified = iotDevice.getCurrentTimestamp();
iotDevice.markConfigModified();

// Method 2: Using helper (recommended)
iotDevice.updateField(iotDevice.deviceConfig.threshold, 30.0f);
```

### WiFi Provisioning

```cpp
// Start AP mode for provisioning
iotDevice.startAPMode();
// Device creates AP: "IoTDevice-MyDevice-001"

// Scan networks
String networks = iotDevice.scanWiFiNetworks();
Serial.println(networks);

// Connect with credentials
iotDevice.connectWiFi("NewSSID", "NewPassword");
```

### Manual Data Sync

```cpp
// Fetch device config from server
iotDevice.fetchDeviceConfig();

// Upload config to server (priority = true to override server)
iotDevice.uploadDeviceConfig(true);

// Fetch control data
iotDevice.fetchControlData();

// Upload control data
iotDevice.uploadControlData();

// Upload telemetry
iotDevice.uploadTelemetry();
```

### Configuration Intervals

```cpp
// Set telemetry upload interval (default: 30 seconds)
iotDevice.setTelemetryInterval(60000);  // 60 seconds

// Set control fetch interval (default: 5 minutes)
iotDevice.setControlFetchInterval(120000);  // 2 minutes
```

## Field Types

The `IoTField<T>` template supports any type:

```cpp
struct MyConfig {
    IoTField<String> name{"name", "Device"};
    IoTField<int> count{"count", 0};
    IoTField<float> value{"value", 0.0f};
    IoTField<double> precise{"precise", 0.0};
    IoTField<bool> enabled{"enabled", true};
};
```

### Field Macros

For convenience, use macros:

```cpp
struct MyConfig {
    // IOT_FIELD(type, name, key, label, type)
    IOT_FIELD(float, temperature, "temperature", "Temperature", "number");

    // IOT_FIELD_SIMPLE(type, name, key)
    IOT_FIELD_SIMPLE(bool, enabled, "enabled");
};
```

## Server API Format

The library expects this JSON format from the server:

### Device Config (GET /api/device/config)
```json
{
  "data": {
    "deviceConfig": {
      "threshold": {
        "key": "threshold",
        "label": "Threshold",
        "type": "number",
        "value": 25.0,
        "lastModified": 1642345678901
      }
    }
  }
}
```

### Control Data (GET /api/device/control)
```json
{
  "data": {
    "controlData": {
      "relay": {
        "key": "relay",
        "type": "boolean",
        "value": true
      }
    }
  }
}
```

### Telemetry Upload (POST /api/device/telemetry)
```json
{
  "sensorData": {
    "temperature": {
      "key": "temperature",
      "label": "Temperature",
      "type": "number",
      "value": 23.5
    }
  }
}
```

## Advanced Usage

### Access Internal Managers

```cpp
// WiFi manager
IoTWiFiManager& wifi = iotDevice.getWiFiManager();
String macAddress = wifi.getMACAddress();

// API client
IoTAPIClient& api = iotDevice.getAPIClient();
api.setRetryCount(5);

// Sync manager
IoTSyncManager& sync = iotDevice.getSyncManager();
bool serverOnline = sync.isServerOnline();

// Storage
IoTStorage& storage = iotDevice.getStorage();
storage.saveString("custom_key", "custom_value");
```

### Custom HTTP Requests

```cpp
IoTAPIClient& api = iotDevice.getAPIClient();

// GET request
String response;
int status = api.httpGET("/api/custom/endpoint", response);

// POST request
String payload = "{\"data\":\"value\"}";
status = api.httpPOST("/api/custom/endpoint", payload, response);
```

### Priority-Based Config Sync

```cpp
// Mark config as modified locally (will sync TO server on next connection)
iotDevice.markConfigModified();

// Check if device has priority
if (iotDevice.getSyncManager().hasDevicePriority()) {
    Serial.println("Device has local changes pending sync to server");
}

// Upload with priority (lastModified = 0 → server accepts unconditionally)
iotDevice.uploadDeviceConfig(true);
```

## API Reference

### Main Class: `IoTDevice<ConfigType, ControlType, TelemetryType>`

#### Initialization
- `void begin()` - Initialize device (call in setup)
- `void configureServer(url, projectId, deviceName, mongoId, version)` - Configure server connection
- `void setDeviceId(deviceId)` - Set device ID for AP mode
- `void setAPPassword(password)` - Set AP password

#### WiFi
- `bool connectWiFi()` - Connect with saved credentials
- `bool connectWiFi(ssid, password)` - Connect with explicit credentials
- `void startAPMode()` - Start provisioning AP
- `bool isWiFiConnected()` - Check connection status
- `String getWiFiStatus()` - Get status string
- `String getIPAddress()` - Get IP address
- `String scanWiFiNetworks()` - Scan networks (returns JSON)

#### Authentication
- `bool login(username, password)` - Login to server
- `bool registerDevice()` - Register device (first-time)
- `bool isAuthenticated()` - Check auth status

#### Time Sync
- `bool syncTimeWithServer()` - Sync time with server
- `uint64_t getCurrentTimestamp()` - Get current timestamp (ms)
- `bool isTimeSynced()` - Check if time is synced

#### Config Management
- `bool fetchDeviceConfig()` - Fetch config from server
- `bool uploadDeviceConfig(priority)` - Upload config to server
- `void markConfigModified()` - Mark config as locally modified

#### Control & Telemetry
- `bool fetchControlData()` - Fetch control data
- `bool uploadControlData()` - Upload control data
- `bool uploadTelemetry()` - Upload telemetry

#### Utilities
- `void update()` - Main update loop (call in loop())
- `void updateField(field, value)` - Update field with timestamp
- `void setTelemetryInterval(ms)` - Set upload interval
- `void setControlFetchInterval(ms)` - Set fetch interval

## Examples

See the `examples/` directory for:
- **BasicUsage** - Complete usage example
- **WiFiProvisioning** - WiFi setup via AP mode
- **CustomDataSync** - Advanced sync patterns

## Troubleshooting

### WiFi Won't Connect
```cpp
// Check saved credentials
if (!iotDevice.getStorage().hasWiFiCredentials()) {
    Serial.println("No saved credentials - starting AP mode");
    iotDevice.startAPMode();
}
```

### Server Authentication Fails
```cpp
// Clear stored token and retry
iotDevice.getStorage().clearDeviceToken();
iotDevice.registerDevice();
```

### Time Not Syncing
```cpp
// Check WiFi and server connection
if (iotDevice.isWiFiConnected()) {
    Serial.println("WiFi OK");
    if (iotDevice.syncTimeWithServer()) {
        Serial.println("Time synced!");
    } else {
        Serial.println("Server unreachable");
    }
}
```

## Architecture

```
┌─────────────────────────────────────┐
│       IoTDevice (Main Class)        │
│  - deviceConfig                     │
│  - controlData                      │
│  - telemetryData                    │
└─────────────────────────────────────┘
          │
          ├─> IoTWiFiManager (WiFi client + AP)
          │
          ├─> IoTAPIClient (HTTP + JWT auth)
          │
          ├─> IoTSyncManager (Time + priority sync)
          │
          └─> IoTStorage (NVS persistence)
```

## Supported Platforms

- ESP32 (all variants)
- ESP32-S2
- ESP32-S3
- ESP32-C3

## License

MIT License - see LICENSE file for details

## Contributing

Contributions welcome! Please submit pull requests or open issues on GitHub.

## Support

For issues and questions:
- GitHub Issues: [https://github.com/yourusername/ArduinoIoTDevice/issues](https://github.com/yourusername/ArduinoIoTDevice/issues)
- Documentation: [https://github.com/yourusername/ArduinoIoTDevice/wiki](https://github.com/yourusername/ArduinoIoTDevice/wiki)

## Changelog

### v1.0.0 (2025-01-XX)
- Initial release
- WiFi management with AP provisioning
- JWT authentication
- NTP time sync
- Custom data structures
- Automatic sync
- Priority-based conflict resolution

---

**Made with ❤️ for the Arduino IoT community**
