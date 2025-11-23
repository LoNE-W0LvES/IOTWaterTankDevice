/**
 * @file IoTWebServer.cpp
 * @brief Implementation of local web server with conditional endpoints
 */

#include "IoTWebServer.h"

IoTWebServer::IoTWebServer(uint16_t port)
    : server(nullptr),
      port(port),
      deviceId(""),
      running(false),
      currentMode(WS_MODE_CLIENT),
      saveWiFiCallback(nullptr),
      scanWiFiCallback(nullptr),
      telemetryCallback(nullptr),
      getControlCallback(nullptr),
      setControlCallback(nullptr),
      getConfigCallback(nullptr),
      setConfigCallback(nullptr),
      getTimestampCallback(nullptr),
      setTimestampCallback(nullptr) {
}

IoTWebServer::~IoTWebServer() {
    if (server) {
        delete server;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void IoTWebServer::begin(WebServerMode mode) {
    if (running) {
        Serial.println("[WebServer] Already running");
        return;
    }

    currentMode = mode;
    server = new AsyncWebServer(port);

    // Add root handler for diagnostics (works in both modes)
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] GET / (root)");
        String html = "<html><body><h1>IoT Device Web Server</h1>";
        html += "<p>Mode: " + String(currentMode == WS_MODE_PROVISIONING ? "PROVISIONING" : "CLIENT") + "</p>";
        html += "<p>Device ID: " + deviceId + "</p>";
        html += "<p>Status: Running</p>";
        html += "<p>Try: <a href=\"/" + deviceId + "/status\">/" + deviceId + "/status</a></p>";
        html += "</body></html>";

        AsyncWebServerResponse* resp = request->beginResponse(200, "text/html", html);
        addCORSHeaders(resp);
        request->send(resp);
    });

    if (currentMode == WS_MODE_PROVISIONING) {
        setupProvisioningRoutes();
    } else {
        setupClientRoutes();
    }

    server->begin();
    running = true;

    Serial.printf("[WebServer] Started on port %d\n", port);
    printEndpoints();
}

void IoTWebServer::stop() {
    if (server && running) {
        server->end();
        running = false;
        Serial.println("[WebServer] Stopped");
    }
}

bool IoTWebServer::isRunning() {
    return running;
}

void IoTWebServer::setDeviceId(const String& deviceId) {
    this->deviceId = deviceId;
}

// ============================================================================
// CALLBACK REGISTRATION
// ============================================================================

void IoTWebServer::onSaveWiFi(SaveWiFiCallback callback) {
    saveWiFiCallback = callback;
}

void IoTWebServer::onScanWiFi(ScanWiFiCallback callback) {
    scanWiFiCallback = callback;
}

void IoTWebServer::onGetTelemetry(GetTelemetryCallback callback) {
    telemetryCallback = callback;
}

void IoTWebServer::onGetControl(GetControlCallback callback) {
    getControlCallback = callback;
}

void IoTWebServer::onSetControl(SetControlCallback callback) {
    setControlCallback = callback;
}

void IoTWebServer::onGetConfig(GetConfigCallback callback) {
    getConfigCallback = callback;
}

void IoTWebServer::onSetConfig(SetConfigCallback callback) {
    setConfigCallback = callback;
}

void IoTWebServer::onGetTimestamp(GetTimestampCallback callback) {
    getTimestampCallback = callback;
}

void IoTWebServer::onSetTimestamp(SetTimestampCallback callback) {
    setTimestampCallback = callback;
}

// ============================================================================
// PROVISIONING ROUTES (AP MODE ONLY)
// ============================================================================

void IoTWebServer::setupProvisioningRoutes() {
    Serial.printf("[WebServer] Setting up provisioning routes, deviceId='%s' (length=%d)\n",
                  deviceId.c_str(), deviceId.length());

    if (deviceId.length() == 0) {
        Serial.println("[WebServer] ERROR: Device ID not set for provisioning - routes NOT created!");
        return;
    }

    String statusEndpoint = "/" + deviceId + "/status";
    String scanEndpoint = "/" + deviceId + "/scanWifi";
    String saveEndpoint = "/" + deviceId + "/save";

    Serial.printf("[WebServer] Registering provisioning endpoints:\n");
    Serial.printf("  - %s\n", statusEndpoint.c_str());
    Serial.printf("  - %s\n", scanEndpoint.c_str());
    Serial.printf("  - %s\n", saveEndpoint.c_str());

    // OPTIONS handlers
    server->on(statusEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on(scanEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on(saveEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });

    // GET /{deviceId}/status - Provisioning status
    server->on(statusEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/status").c_str());

        DynamicJsonDocument doc(256);
        doc["status"] = "ready";
        doc["deviceId"] = deviceId;

        String response;
        serializeJson(doc, response);

        AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
        addCORSHeaders(resp);
        request->send(resp);
    });

    // GET /{deviceId}/scanWifi - Scan WiFi networks
    server->on(scanEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/scanWifi").c_str());

        if (scanWiFiCallback) {
            String response = scanWiFiCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"WiFi scan not available\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // POST /{deviceId}/save - Save WiFi and dashboard credentials
    server->on(saveEndpoint.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body;
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            Serial.printf("[WebServer] POST %s: %s\n", ("/" + deviceId + "/save").c_str(), body.c_str());

            DynamicJsonDocument doc(512);
            DeserializationError error = deserializeJson(doc, body);

            if (!error && doc.containsKey("ssid") && doc.containsKey("password")) {
                String ssid = doc["ssid"].as<String>();
                String password = doc["password"].as<String>();
                String dashboardUser = doc.containsKey("dashboardUsername") ? doc["dashboardUsername"].as<String>() : "";
                String dashboardPass = doc.containsKey("dashboardPassword") ? doc["dashboardPassword"].as<String>() : "";

                if (saveWiFiCallback) {
                    saveWiFiCallback(ssid, password, dashboardUser, dashboardPass);

                    DynamicJsonDocument responseDoc(256);
                    responseDoc["success"] = true;
                    responseDoc["message"] = "Connecting to WiFi...";

                    String response;
                    serializeJson(responseDoc, response);

                    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
                    addCORSHeaders(resp);
                    request->send(resp);
                } else {
                    AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                        "{\"error\":\"Save callback not set\"}");
                    addCORSHeaders(resp);
                    request->send(resp);
                }
            } else {
                AsyncWebServerResponse* resp = request->beginResponse(400, "application/json",
                    "{\"error\":\"Invalid request - missing ssid or password\"}");
                addCORSHeaders(resp);
                request->send(resp);
            }
        });
}

// ============================================================================
// CLIENT ROUTES (CONNECTED MODE ONLY)
// ============================================================================

void IoTWebServer::setupClientRoutes() {
    if (deviceId.length() == 0) {
        Serial.println("[WebServer] Warning: Device ID not set");
        return;
    }

    String telemetryEndpoint = "/" + deviceId + "/telemetry";
    String controlEndpoint = "/" + deviceId + "/control";
    String configEndpoint = "/" + deviceId + "/config";
    String timestampEndpoint = "/" + deviceId + "/timestamp";

    // OPTIONS handlers
    server->on(telemetryEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on(controlEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on(configEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on(timestampEndpoint.c_str(), HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });

    // GET /{deviceId}/telemetry - Get current sensor readings
    server->on(telemetryEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/telemetry").c_str());

        if (telemetryCallback) {
            String response = telemetryCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"Telemetry callback not set\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // GET /{deviceId}/control - Get control data with timestamps
    server->on(controlEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/control").c_str());

        if (getControlCallback) {
            String response = getControlCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"Control callback not set\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // POST /{deviceId}/control - Update control data from app
    server->on(controlEndpoint.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body;
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            Serial.printf("[WebServer] POST %s: %s\n", ("/" + deviceId + "/control").c_str(), body.c_str());

            if (setControlCallback) {
                bool success = setControlCallback(body);
                AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
                    success ? "{\"success\":true}" : "{\"success\":false}");
                addCORSHeaders(resp);
                request->send(resp);
            } else {
                AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                    "{\"error\":\"Control callback not set\"}");
                addCORSHeaders(resp);
                request->send(resp);
            }
        });

    // GET /{deviceId}/config - Get device configuration
    server->on(configEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/config").c_str());

        if (getConfigCallback) {
            String response = getConfigCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"Config callback not set\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // POST /{deviceId}/config - Update device configuration from app
    server->on(configEndpoint.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body;
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            Serial.printf("[WebServer] POST %s: %s\n", ("/" + deviceId + "/config").c_str(), body.c_str());

            if (setConfigCallback) {
                bool success = setConfigCallback(body);
                AsyncWebServerResponse* resp = request->beginResponse(200, "application/json",
                    success ? "{\"success\":true}" : "{\"success\":false}");
                addCORSHeaders(resp);
                request->send(resp);
            } else {
                AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                    "{\"error\":\"Config callback not set\"}");
                addCORSHeaders(resp);
                request->send(resp);
            }
        });

    // GET /{deviceId}/timestamp - Get device timestamp and sync status
    server->on(timestampEndpoint.c_str(), HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.printf("[WebServer] GET %s\n", ("/" + deviceId + "/timestamp").c_str());

        if (getTimestampCallback) {
            String response = getTimestampCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"Timestamp callback not set\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // POST /{deviceId}/timestamp - Sync device time from app (auto-detects seconds/millis)
    server->on(timestampEndpoint.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest* request) {},
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            String body;
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            Serial.printf("[WebServer] POST %s: %s\n", ("/" + deviceId + "/timestamp").c_str(), body.c_str());

            DynamicJsonDocument doc(256);
            DeserializationError error = deserializeJson(doc, body);

            if (!error && doc.containsKey("timestamp")) {
                uint64_t timestamp = doc["timestamp"].as<uint64_t>();

                // Auto-detect if seconds or milliseconds (if < 10 billion, it's probably seconds)
                if (timestamp < 10000000000ULL) {
                    timestamp *= 1000;  // Convert to milliseconds
                }

                if (setTimestampCallback) {
                    bool success = setTimestampCallback(timestamp);
                    DynamicJsonDocument responseDoc(256);
                    responseDoc["success"] = success;
                    responseDoc["timestamp"] = timestamp;

                    String response;
                    serializeJson(responseDoc, response);

                    AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
                    addCORSHeaders(resp);
                    request->send(resp);
                } else {
                    AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                        "{\"error\":\"Timestamp callback not set\"}");
                    addCORSHeaders(resp);
                    request->send(resp);
                }
            } else {
                AsyncWebServerResponse* resp = request->beginResponse(400, "application/json",
                    "{\"error\":\"Invalid request - missing timestamp\"}");
                addCORSHeaders(resp);
                request->send(resp);
            }
        });
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void IoTWebServer::addCORSHeaders(AsyncWebServerResponse* response) {
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void IoTWebServer::handleOptions(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200);
    addCORSHeaders(response);
    request->send(response);
}

void IoTWebServer::printEndpoints() {
    Serial.println("\n========== WEB SERVER ENDPOINTS ==========");
    Serial.printf("Device ID: %s\n", deviceId.c_str());

    if (currentMode == WS_MODE_PROVISIONING) {
        Serial.println("\n📡 PROVISIONING MODE (AP Mode):");
        Serial.println("  GET  /" + deviceId + "/status         - Provisioning status");
        Serial.println("  GET  /" + deviceId + "/scanWifi       - Scan WiFi networks");
        Serial.println("  POST /" + deviceId + "/save           - Save WiFi & dashboard credentials");
        Serial.println("\n  WiFi + Dashboard credentials expected in /save:");
        Serial.println("  { \"ssid\": \"...\", \"password\": \"...\",");
        Serial.println("    \"dashboardUsername\": \"...\", \"dashboardPassword\": \"...\" }");
    } else {
        Serial.println("\n📱 CLIENT MODE (Connected to WiFi):");
        Serial.println("  GET  /" + deviceId + "/telemetry      - Get current sensor readings");
        Serial.println("  GET  /" + deviceId + "/control        - Get control data with timestamps");
        Serial.println("  POST /" + deviceId + "/control        - Update control data from app");
        Serial.println("  GET  /" + deviceId + "/config         - Get device configuration");
        Serial.println("  POST /" + deviceId + "/config         - Update device configuration from app");
        Serial.println("  GET  /" + deviceId + "/timestamp      - Get device timestamp and sync status");
        Serial.println("  POST /" + deviceId + "/timestamp      - Sync device time from app (auto-detects seconds/millis)");
    }

    Serial.println("==========================================\n");
}
