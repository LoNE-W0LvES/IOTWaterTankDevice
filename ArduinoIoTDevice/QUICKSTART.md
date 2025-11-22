# Quick Start Guide

## 5-Minute Setup

### Step 1: Install Library
```bash
# Arduino IDE: Sketch → Include Library → Manage Libraries → Search "ArduinoIoTDevice"
# Or copy to: Arduino/libraries/ArduinoIoTDevice/
```

### Step 2: Define Data Structures
```cpp
#include <ArduinoIoTDevice.h>

// Your device configuration
struct MyDeviceConfig {
    IoTField<String> ipAddress{"ipAddress"};
    IoTField<float> threshold{"threshold", 25.0f};
};

// Control commands from server/app
struct MyControlData {
    IoTField<bool> relay{"relay"};
    IoTField<bool> config_update{"config_update"};
};

// Sensor data to send
struct MyTelemetryData {
    IoTField<float> temperature{"temperature"};
    IoTField<int> Status{"Status", 1};
};
```

### Step 3: Create Instance
```cpp
IoTDevice<MyDeviceConfig, MyControlData, MyTelemetryData> iotDevice;
```

### Step 4: Initialize
```cpp
void setup() {
    Serial.begin(115200);

    // Init device
    iotDevice.begin();

    // Configure server
    iotDevice.configureServer(
        "http://your-server.com",  // Server URL
        "projectId",                // Project ID
        "deviceName",               // Device name
        "mongoDbId",                // MongoDB ID
        "1.0.0"                     // Version
    );

    // Connect WiFi
    iotDevice.connectWiFi("SSID", "password");
    while (!iotDevice.isWiFiConnected()) {
        iotDevice.update();
        delay(100);
    }

    // Login
    iotDevice.login("username", "password");

    // Sync time
    iotDevice.syncTimeWithServer();
}
```

### Step 5: Use in Loop
```cpp
void loop() {
    iotDevice.update();  // Handles everything!

    // Read config
    float threshold = iotDevice.deviceConfig.threshold.value;

    // Update telemetry
    iotDevice.telemetryData.temperature.value = readSensor();

    // Check control
    if (iotDevice.controlData.relay.value) {
        digitalWrite(RELAY_PIN, HIGH);
    }

    delay(100);
}
```

## That's It!

The library now handles:
- ✅ WiFi connection and reconnection
- ✅ Server authentication
- ✅ Time synchronization
- ✅ Automatic telemetry upload (every 30s)
- ✅ Automatic control fetch (every 5min)
- ✅ Config sync with priority resolution
- ✅ Persistent storage

## Common Operations

### Access Field Values
```cpp
// Read value
float value = iotDevice.deviceConfig.threshold.value;

// Read timestamp
uint64_t ts = iotDevice.deviceConfig.threshold.lastModified;
```

### Update Config Locally
```cpp
// Auto-updates timestamp and marks for sync
iotDevice.updateField(iotDevice.deviceConfig.threshold, 30.0f);
```

### WiFi Provisioning
```cpp
// Start AP mode
iotDevice.startAPMode();
// Creates: "IoTDevice-{deviceId}" AP

// Scan networks
String networks = iotDevice.scanWiFiNetworks();
```

### Manual Sync
```cpp
// Fetch config
iotDevice.fetchDeviceConfig();

// Upload config (priority=true to override server)
iotDevice.uploadDeviceConfig(true);

// Upload telemetry
iotDevice.uploadTelemetry();
```

### Customize Intervals
```cpp
// Telemetry every 60 seconds
iotDevice.setTelemetryInterval(60000);

// Control check every 2 minutes
iotDevice.setControlFetchInterval(120000);
```

## Field Access Patterns

As requested, you can access fields like:

```cpp
// Read value
String ip = iotDevice.deviceConfig.ipAddress.value;

// Read timestamp
uint64_t lastMod = iotDevice.deviceConfig.ipAddress.lastModified;

// Set value
iotDevice.deviceConfig.ipAddress.value = "192.168.1.252";

// Set timestamp
iotDevice.deviceConfig.ipAddress.lastModified = 1642345678901;

// Or use helper
iotDevice.updateField(iotDevice.deviceConfig.ipAddress, "192.168.1.252");
```

## Server API Format

Your server should respond with this format:

**Config**: `GET /api/device/config`
```json
{
  "data": {
    "deviceConfig": {
      "threshold": {
        "key": "threshold",
        "value": 25.0,
        "lastModified": 1642345678901
      }
    }
  }
}
```

**Control**: `GET /api/device/control`
```json
{
  "data": {
    "controlData": {
      "relay": {
        "key": "relay",
        "value": true
      },
      "config_update": {
        "key": "config_update",
        "value": false
      }
    }
  }
}
```

## Need More?

- 📖 Full documentation: [README.md](README.md)
- 💡 Examples: [examples/BasicUsage](examples/BasicUsage)
- 🐛 Issues: [GitHub Issues](https://github.com/yourusername/ArduinoIoTDevice/issues)

---

Happy IoT-ing! 🚀
