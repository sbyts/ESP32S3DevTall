#include "net_manager.h"
#include "web_ui.h"
#include "../config/config.h"
#include "../system/sys_logger.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

NetManager netManager;
static AsyncWebServer server(80);
static Preferences prefs;

// Default credentials for initial bootstrap (override via Web UI)
static const char *DEFAULT_SSID = "YOUR_WIFI_SSID";
static const char *DEFAULT_PASS = "YOUR_WIFI_PASSWORD";

// Background Wi-Fi health check timer
static unsigned long lastWifiCheckMs = 0;
static constexpr unsigned long WIFI_WATCHDOG_INTERVAL_MS = 15000;

void NetManager::begin(LampState *statePtr)
{
    _lampState = statePtr;

    prefs.begin("ridy_net", false);
    _adminUser = DEFAULT_ADMIN_USER;
    _adminPass = prefs.getString("admin_pass", DEFAULT_ADMIN_PASS);
    _hostname = prefs.getString("hostname", DEFAULT_HOSTNAME);

    if (prefs.getString("ssid_0", "").isEmpty() && String(DEFAULT_SSID) != "YOUR_WIFI_SSID")
    {
        prefs.putString("ssid_0", DEFAULT_SSID);
        prefs.putString("pass_0", DEFAULT_PASS);
    }

    // Reset radio state cleanly before scanning (prevents post-OTA hang)
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    delay(100);

    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.setHostname(_hostname.c_str());

    connectMultiAP();
    setupOTA();
    MDNS.addService("http", "tcp", 80);
    setupWebServer();
}

void NetManager::connectMultiAP()
{
    LOG_I("Scanning RF spectrum for saved APs...");
    int n = WiFi.scanNetworks();

    int bestSlot = -1;
    int32_t bestRssi = -1000;
    String bestSsid = "";

    for (int slot = 0; slot < WIFI_MAX_SLOTS; slot++)
    {
        String savedSsid = prefs.getString(("ssid_" + String(slot)).c_str(), "");
        if (savedSsid.isEmpty())
            continue;

        for (int i = 0; i < n; i++)
        {
            if (WiFi.SSID(i) == savedSsid && WiFi.RSSI(i) > bestRssi)
            {
                bestRssi = WiFi.RSSI(i);
                bestSlot = slot;
                bestSsid = savedSsid;
            }
        }
    }

    if (bestSlot != -1)
    {
        String bestPass = prefs.getString(("pass_" + String(bestSlot)).c_str(), "");
        LOG_I("Connecting to '%s' (Slot %d, %d dBm)", bestSsid.c_str(), bestSlot + 1, bestRssi);

        WiFi.begin(bestSsid.c_str(), bestPass.c_str());
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS)
        {
            delay(250);
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            _apMode = false;
            _connectedSsid = bestSsid;

            // Register both HTTP and ArduinoOTA services via mDNS
            if (MDNS.begin(_hostname.c_str()))
            {
                MDNS.addService("http", "tcp", 80);
            }

            LOG_I("Wi-Fi OK: %s (%s.local)", WiFi.localIP().toString().c_str(), _hostname.c_str());
            WiFi.scanDelete();
            return;
        }
        LOG_W("Wi-Fi timeout on '%s'", bestSsid.c_str());
    }

    WiFi.scanDelete();
    startFallbackAP();
}

void NetManager::startFallbackAP()
{
    _apMode = true;
    _connectedSsid = FALLBACK_AP_SSID;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(FALLBACK_AP_SSID, FALLBACK_AP_PASS);

    if (MDNS.begin(_hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }

    LOG_W("Fallback AP '%s' started at %s", FALLBACK_AP_SSID, WiFi.softAPIP().toString().c_str());
}

void NetManager::setupOTA()
{
    ArduinoOTA.setHostname(_hostname.c_str());
    ArduinoOTA.setPassword(_adminPass.c_str());

    ArduinoOTA.onStart([]()
                       { LOG_W("OTA update started..."); });
    ArduinoOTA.onEnd([]()
                     { LOG_I("OTA complete, rebooting..."); });
    ArduinoOTA.onError([](ota_error_t error)
                       { LOG_E("OTA failed with code [%u]", error); });

    ArduinoOTA.begin();
}

void NetManager::setupWebServer()
{
    // Serve embedded dark-mode SPA (protected by HTTP Basic Auth)
    server.on("/", HTTP_GET, [this](AsyncWebServerRequest *req)
              {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        req->send_P(200, "text/html", INDEX_HTML); });

    // Live hardware telemetry, lighting state, and 12-entry ring log
    server.on("/api/telemetry", HTTP_GET, [this](AsyncWebServerRequest *req)
              {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        JsonDocument doc;
        doc["on"]       = _lampState->isLampOn;
        doc["mode"]     = _lampState->colorMode;
        doc["bri"]      = (int)roundf(_lampState->brightness * 100.0f);
        doc["touch"]    = _lampState->touchDelta;
        doc["apMode"]   = _apMode;
        doc["ssid"]     = _connectedSsid;
        doc["rssi"]     = _apMode ? 0 : WiFi.RSSI();
        doc["ip"]       = _apMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["minHeap"]  = ESP.getMinFreeHeap();
        doc["temp"]     = temperatureRead();
        doc["uptime"]   = millis() / 1000;

        JsonArray logs = doc["logs"].to<JsonArray>();
        size_t count = sysLog.getCount();
        for (size_t i = 0; i < count; i++) {
            const LogEntry& e = sysLog.getEntry(i);
            JsonObject item = logs.add<JsonObject>();
            item["t"] = e.timestampSec;
            item["l"] = e.level;
            item["m"] = e.message;
        }

        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out); });

    // Clear ring-buffer diagnostic log
    server.on("/api/logs/clear", HTTP_POST, [this](AsyncWebServerRequest *req)
              {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        sysLog.clear();
        LOG_I("System log cleared via Web UI");
        req->send(200, "application/json", "{\"ok\":true}"); });

    // Apply lighting state changes from Web UI
    server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
                return req->requestAuthentication();
            }
            JsonDocument doc;
            if (!deserializeJson(doc, data, len)) {
                if (doc["on"].is<bool>())  _lampState->isLampOn   = doc["on"];
                if (doc["mode"].is<int>()) _lampState->colorMode  = constrain((int)doc["mode"], 1, 3);
                if (doc["bri"].is<int>())  _lampState->brightness = constrain((int)doc["bri"], 10, 100) / 100.0f;
                LOG_I("Web UI -> %s | Mode %u | %d%%",
                      _lampState->isLampOn ? "ON" : "OFF",
                      _lampState->colorMode,
                      (int)roundf(_lampState->brightness * 100.0f));
            }
            req->send(200, "application/json", "{\"ok\":true}"); });

    // Read stored 6-slot Wi-Fi credentials and hostname
    server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *req)
              {
        if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
            return req->requestAuthentication();
        }
        JsonDocument doc;
        doc["host"] = _hostname;
        JsonArray nets = doc["nets"].to<JsonArray>();
        for (int i = 0; i < WIFI_MAX_SLOTS; i++) {
            JsonObject obj = nets.add<JsonObject>();
            obj["ssid"] = prefs.getString(("ssid_" + String(i)).c_str(), "");
            obj["pass"] = prefs.getString(("pass_" + String(i)).c_str(), "");
        }
        String out;
        serializeJson(doc, out);
        req->send(200, "application/json", out); });

    // Fast lightweight telemetry & presence-trigger endpoint for ESP32S3DevRound station
    server.on("/api/link", HTTP_ANY, [this](AsyncWebServerRequest* req) {
        if (!req->hasHeader("X-Link-Key") || req->getHeader("X-Link-Key")->value() != LINK_API_KEY) {
            return req->send(401, "application/json", "{\"error\":\"unauthorized\"}");
        }

        if (req->method() == HTTP_POST && req->hasParam("presence")) {
            bool present = (req->getParam("presence")->value() == "1");
            if (!present && _lampState->isLampOn) {
                _lampState->isLampOn = false;
                LOG_I("Station Radar -> User away, fading OFF");
            } else if (present && !_lampState->isLampOn) {
                _lampState->isLampOn = true;
                LOG_I("Station Radar -> User detected, fading ON");
            }
        }

        char resp[64];
        snprintf(resp, sizeof(resp), "{\"on\":%s,\"mode\":%u,\"bri\":%d}",
                 _lampState->isLampOn ? "true" : "false",
                 _lampState->colorMode,
                 (int)roundf(_lampState->brightness * 100.0f));
        req->send(200, "application/json", resp);
    });

    // Persist Wi-Fi slots, hostname, and Web/OTA password to NVS
    server.on("/api/save", HTTP_POST, [](AsyncWebServerRequest *req) {}, nullptr, [this](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total)
              {
            if (!req->authenticate(_adminUser.c_str(), _adminPass.c_str())) {
                return req->requestAuthentication();
            }
            JsonDocument doc;
            if (!deserializeJson(doc, data, len)) {
                JsonArray nets = doc["nets"].as<JsonArray>();
                for (int i = 0; i < WIFI_MAX_SLOTS && i < (int)nets.size(); i++) {
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
                LOG_W("Network config saved, rebooting...");
                req->send(200, "application/json", "{\"saved\":true}");
                delay(600);
                ESP.restart();
            } else {
                req->send(400, "application/json", "{\"error\":\"bad json\"}");
            } });

    server.onNotFound([](AsyncWebServerRequest *req)
                      { req->send(404, "text/plain", "Not Found"); });

    server.begin();
}

void NetManager::loop()
{
    ArduinoOTA.handle();

    // Non-blocking Wi-Fi reconnect watchdog in STA mode
    if (!_apMode && (millis() - lastWifiCheckMs >= WIFI_WATCHDOG_INTERVAL_MS))
    {
        lastWifiCheckMs = millis();
        if (WiFi.status() != WL_CONNECTED)
        {
            LOG_W("Wi-Fi link lost, attempting reconnect...");
            WiFi.reconnect();
        }
    }
}
bool NetManager::isConnected() const
{
    return !_apMode && (WiFi.status() == WL_CONNECTED);
}