/**
 * @file IoTWebServer.cpp
 * @brief Implementation of local web server
 */

#include "IoTWebServer.h"

IoTWebServer::IoTWebServer(uint16_t port)
    : server(nullptr),
      port(port),
      deviceId(""),
      running(false),
      provisioningEnabled(false),
      statusCallback(nullptr),
      configCallback(nullptr),
      controlCallback(nullptr),
      saveWiFiCallback(nullptr),
      scanWiFiCallback(nullptr) {
}

IoTWebServer::~IoTWebServer() {
    if (server) {
        delete server;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void IoTWebServer::begin() {
    if (running) {
        Serial.println("[WebServer] Already running");
        return;
    }

    server = new AsyncWebServer(port);
    setupRoutes();

    if (provisioningEnabled) {
        setupProvisioningRoutes();
    }

    server->begin();
    running = true;

    Serial.printf("[WebServer] Started on port %d\n", port);
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

// ============================================================================
// CALLBACK REGISTRATION
// ============================================================================

void IoTWebServer::onGetStatus(GetStatusCallback callback) {
    statusCallback = callback;
}

void IoTWebServer::onGetConfig(GetConfigCallback callback) {
    configCallback = callback;
}

void IoTWebServer::onSetControl(SetControlCallback callback) {
    controlCallback = callback;
}

void IoTWebServer::onSaveWiFi(SaveWiFiCallback callback) {
    saveWiFiCallback = callback;
}

void IoTWebServer::setDeviceId(const String& deviceId) {
    this->deviceId = deviceId;
}

void IoTWebServer::enableProvisioning(bool enable) {
    provisioningEnabled = enable;
}

void IoTWebServer::onScanWiFi(GetStatusCallback callback) {
    scanWiFiCallback = callback;
}

// ============================================================================
// PRIVATE METHODS
// ============================================================================

void IoTWebServer::setupRoutes() {
    // Handle OPTIONS for CORS preflight
    server->on("/api/status", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on("/api/config", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });
    server->on("/api/control", HTTP_OPTIONS, [this](AsyncWebServerRequest* request) {
        handleOptions(request);
    });

    // GET /api/status - Get telemetry/status
    server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] GET /api/status");

        if (statusCallback) {
            String response = statusCallback();
            AsyncWebServerResponse* resp = request->beginResponse(200, "application/json", response);
            addCORSHeaders(resp);
            request->send(resp);
        } else {
            AsyncWebServerResponse* resp = request->beginResponse(500, "application/json",
                "{\"error\":\"Status callback not set\"}");
            addCORSHeaders(resp);
            request->send(resp);
        }
    });

    // GET /api/config - Get device configuration
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* request) {
        Serial.println("[WebServer] GET /api/config");

        if (configCallback) {
            String response = configCallback();
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

    // POST /api/control - Send control commands
    server->on("/api/control", HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // This is called after body is received
        },
        nullptr,
        [this](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
            // Body handler
            String body;
            for (size_t i = 0; i < len; i++) {
                body += (char)data[i];
            }

            Serial.printf("[WebServer] POST /api/control: %s\n", body.c_str());

            if (controlCallback) {
                bool success = controlCallback(body);
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
}

void IoTWebServer::setupProvisioningRoutes() {
    if (deviceId.length() == 0) {
        Serial.println("[WebServer] Warning: Device ID not set for provisioning");
        return;
    }

    String statusEndpoint = "/" + deviceId + "/status";
    String scanEndpoint = "/" + deviceId + "/scanWifi";
    String saveEndpoint = "/" + deviceId + "/save";

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

    // GET /{deviceId}/status
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

    // GET /{deviceId}/scanWifi
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

    // POST /{deviceId}/save
    server->on(saveEndpoint.c_str(), HTTP_POST,
        [this](AsyncWebServerRequest* request) {
            // This is called after body is received
        },
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

                if (saveWiFiCallback) {
                    saveWiFiCallback(ssid, password);

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
                        "{\"error\":\"Save WiFi callback not set\"}");
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

    Serial.printf("[WebServer] Provisioning endpoints enabled for device: %s\n", deviceId.c_str());
}

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
