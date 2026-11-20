#include "net_manager.h"
#include "web_ui.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "../config/config.h"

NetManager netManager;
static AsyncWebServer server(80);
static Preferences prefs;

// Дефолтна мережа для першого старту (потім редагується через Web UI)
static const char* DEFAULT_SSID = "YOUR_WIFI_SSID";
static const char* DEFAULT_PASS = "YOUR_WIFI_PASSWORD";

void NetManager::begin(LampState* statePtr) {
    _lampState = statePtr;

    prefs.begin("ridy_net", false);
    _adminUser = DEFAULT_ADMIN_USER;
    _adminPass = prefs.getString("admin_pass", DEFAULT_ADMIN_PASS);
    _hostname  = prefs.getString("hostname", DEFAULT_HOSTNAME);



    // Якщо слот 0 порожній і вказано дефолтний SSID — записуємо його
    if (prefs.getString("ssid_0", "").isEmpty() && String(DEFAULT_SSID) != "YOUR_WIFI_SSID") {
        prefs.putString("ssid_0", DEFAULT_SSID);
        prefs.putString("pass_0", DEFAULT_PASS);
    }

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false); // Стабільність модему (як у ESP32S3DevRound)
    WiFi.setHostname(_hostname.c_str());

    connectMultiAP();
    setupOTA();
    setupWebServer();
}

void NetManager::connectMultiAP() {
    Serial.println("[NET] Scanning RF spectrum for known APs...");
    int n = WiFi.scanNetworks();

    int bestSlot = -1;
    int32_t bestRssi = -1000;
    String bestSsid = "";

    for (int slot = 0; slot < 6; slot++) {
        String savedSsid = prefs.getString(("ssid_" + String(slot)).c_str(), "");
        if (savedSsid.isEmpty()) continue;

        for (int i = 0; i < n; i++) {
            if (WiFi.SSID(i) == savedSsid && WiFi.RSSI(i) > bestRssi) {
                bestRssi = WiFi.RSSI(i);
                bestSlot = slot;
                bestSsid = savedSsid;
            }
        }
    }

    if (bestSlot != -1) {
        String bestPass = prefs.getString(("pass_" + String(bestSlot)).c_str(), "");
        Serial.printf("[NET] Best match: '%s' (Slot %d, RSSI %d dBm). Connecting...\n",
                      bestSsid.c_str(), bestSlot + 1, bestRssi);

        WiFi.begin(bestSsid.c_str(), bestPass.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(250);
            Serial.print(".");
        }
        Serial.println();

        if (WiFi.status() == WL_CONNECTED) {
            _apMode = false;
            _connectedSsid = bestSsid;
            Serial.printf("[NET] Connected! IP: %s | Host: %s.local\n",
                          WiFi.localIP().toString().c_str(), _hostname.c_str());
            WiFi.scanDelete();
            return;
        }
    }

    WiFi.scanDelete();
    startFallbackAP();
}

void NetManager::startFallbackAP() {
    _apMode = true;
    _connectedSsid = "RidyLamp-AP";
    WiFi.mode(WIFI_AP);
    WiFi.softAP("RidyLamp-AP", "12345678");
    Serial.printf("[NET] Offline fallback -> AP 'RidyLamp-AP' started at %s\n",
                  WiFi.softAPIP().toString().c_str());
}

void NetManager::setupOTA() {
    ArduinoOTA.setHostname(_hostname.c_str());
    ArduinoOTA.setPassword(_adminPass.c_str());

    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Firmware update started...");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Complete! Rebooting...");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error [%u]\n", error);
    });

    ArduinoOTA.begin();
}

void NetManager::setupWebServer() {
    // Головна сторінка (захищена HTTP Basic Auth)
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        req->send_P(200, "text/html", INDEX_HTML);
    });

    // Телеметрія + поточний стан світла
    server.on("/api/telemetry", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        JsonDocument doc;
        doc["on"]       = _lampState->isLampOn;
        doc["mode"]     = _lampState->colorMode;
        doc["bri"]      = (int)roundf(_lampState->brightness * 100.0f);
        doc["apMode"]   = _apMode;
        doc["ssid"]     = _connectedSsid;
        doc["rssi"]     = _apMode ? 0 : WiFi.RSSI();
        doc["ip"]       = _apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["minHeap"]  = ESP.getMinFreeHeap();
        doc["temp"]     = temperatureRead();
        doc["uptime"]   = millis() / 1000;

        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // Керування світлом із Web UI
    server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest* req) {}, nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
                return req->requestAuthentication();
            }
            JsonDocument doc;
            if (!deserializeJson(doc, data, len)) {
                if (doc["on"].is<bool>())   _lampState->isLampOn   = doc["on"];
                if (doc["mode"].is<int>())  _lampState->colorMode  = constrain((int)doc["mode"], 1, 3);
                if (doc["bri"].is<int>())   _lampState->brightness = constrain((int)doc["bri"], 10, 100) / 100.0f;
            }
            req->send(200, "application/json", "{\"ok\":true}");
        }
    );

    // Читання збережених 6 Wi-Fi слотів
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest* req) {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        JsonDocument doc;
        doc["host"] = _hostname;
        JsonArray nets = doc["nets"].to<JsonArray>();
        for (int i = 0; i < 6; i++) {
            JsonObject obj = nets.add<JsonObject>();
            obj["ssid"] = prefs.getString(("ssid_" + String(i)).c_str(), "");
            obj["pass"] = prefs.getString(("pass_" + String(i)).c_str(), "");
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out);
    });

    // Збереження Wi-Fi слотів та пароля у NVS
    server.on("/api/save", HTTP_POST, [](AsyncWebServerRequest* req) {}, nullptr,
        [this](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
            if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
                return req->requestAuthentication();
            }
            JsonDocument doc;
            if (!deserializeJson(doc, data, len)) {
                JsonArray nets = doc["nets"].as<JsonArray>();
                for (int i = 0; i < 6 && i < (int)nets.size(); i++) {
                    prefs.putString(("ssid_" + String(i)).c_str(), nets[i]["ssid"].as<String>());
                    prefs.putString(("pass_" + String(i)).c_str(), nets[i]["pass"].as<String>());
                }
                String newPass = doc["pass"].as<String>();
                if (newPass.length() >= 4) {
                    prefs.putString("admin_pass", newPass);
                }
                String newHost = doc["host"].as<String>();
                if (newHost.length() >= 2) {
                    prefs.putString("hostname", newHost);
                }
                req->send(200, "application/json", "{\"saved\":true}");
                delay(500);
                ESP.restart();
            } else {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
            }
        }
    );

    server.begin();
}

void NetManager::loop() {
    ArduinoOTA.handle();
}

bool NetManager::isConnected() const {
    return !_apMode && (WiFi.status() == WL_CONNECTED);
}