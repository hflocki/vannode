// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "webserver.h"
#include "wifi_manager.h"
#include "sensor.h"
#include "mqtt_ha.h"
#include "safeguard.h"
#include "fwcheck.h"
#include "evlog.h"
#include "alde_ci_sniff.h"
#include "dbglog.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>

// Routes:
//   GET  /               web UI (LittleFS /index.html.gz), upload form if missing
//   GET  /api/config     all settings (passwords never)
//   POST /api/config     partial settings as JSON, validated first, saved in loop()
//   POST /api/update     file upload: .bin = firmware (restart), .gz = web page (no restart)
//   POST /api/measure    publish the current readings right away
//   POST /api/control    command for the Alde panel (query: target, gas, el_level, water, power), only with ci_mode 2
//   GET  /api/sniff      ci_mode 3: frames seen on the external panel bus, per LIN id
//   POST /api/sniff_reset clear those statistics
//   GET  /api/log        debug log as text, POST /api/log_clear empties it
//   GET  /api/debug      bus statistics for the debug tab, POST /api/debug_dump writes the
//                        state of all values into the debug log
//   POST /api/restart
//   WS   /ws             live values every WS_UPDATE_MS

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static unsigned long lastWsUpdate = 0;

static bool fsReady = false;
static bool restartPending = false;
static unsigned long restartAt = 0;
static volatile bool configDirty = false;   // Save request from the async web task, handled in loop()

// ---- Upload (web UI): firmware .bin or the web page index.html.gz --------------
// A complete LittleFS image is NOT accepted here: it would also replace /config.json.
static bool upAuthorized = false;
static bool upWebPage = false;
static bool upFailed = false;
static File upFile;
static const char* WEB_FILE = "/index.html.gz";
static const char* WEB_TMP = "/index.html.gz.tmp";

static const size_t MAX_BODY_SIZE = 4096;

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len);
static void handleApiGetConfig(AsyncWebServerRequest* request);
static void handleApiSaveConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);

// Web UI login (HTTP basic auth), always on. Factory login: DEFAULT_WEB_USER / DEFAULT_WEB_PASSWORD
// (admin / admin); devices without a stored login get it at the start (loadConfig).
static bool authOk(AsyncWebServerRequest* request) {
    return request->authenticate(config.web_user, config.web_pass);
}

static bool auth(AsyncWebServerRequest* request) {
    if (authOk(request)) return true;
    request->requestAuthentication();
    return false;
}

// ============================================================
// Helpers
// ============================================================

static void setDefaults() {
    strlcpy(config.ap_ssid, defaultApSsid(), sizeof(config.ap_ssid));
    strlcpy(config.ap_pass, DEFAULT_AP_PASSWORD, sizeof(config.ap_pass));
    config.wifi_tx_power = DEFAULT_TX_POWER;
    config.ap_always = DEFAULT_AP_ALWAYS;
    strlcpy(config.wifi_ssid, DEFAULT_WIFI_SSID, sizeof(config.wifi_ssid));
    strlcpy(config.wifi_pass, DEFAULT_WIFI_PASSWORD, sizeof(config.wifi_pass));
    strlcpy(config.mqtt_server, DEFAULT_MQTT_SERVER, sizeof(config.mqtt_server));
    config.mqtt_port = DEFAULT_MQTT_PORT;
    strlcpy(config.mqtt_user, DEFAULT_MQTT_USER, sizeof(config.mqtt_user));
    strlcpy(config.mqtt_pass, DEFAULT_MQTT_PASSWORD, sizeof(config.mqtt_pass));
    strlcpy(config.mqtt_prefix, DEFAULT_MQTT_PREFIX, sizeof(config.mqtt_prefix));
    strlcpy(config.web_user, DEFAULT_WEB_USER, sizeof(config.web_user));
    strlcpy(config.web_pass, DEFAULT_WEB_PASSWORD, sizeof(config.web_pass));

    // Device settings
    config.interval_s = DEFAULT_INTERVAL_S;
    config.ci_mode = DEFAULT_CI_MODE;
    config.ci_function = DEFAULT_CI_FUNCTION;
    config.lin_enabled = DEFAULT_LIN_ENABLED;
    config.el_max = DEFAULT_EL_MAX;
    config.debug = false;
}

static bool isNumber(JsonVariantConst v) {
    return v.is<float>() || v.is<int>();
}

static void copyIfPresent(JsonDocument& doc, const char* key, char* dest, size_t size, bool skipEmpty) {
    if (doc[key].isNull()) return;
    const char* v = doc[key].as<const char*>();
    if (!v) return;
    if (skipEmpty && v[0] == '\0') return;
    strlcpy(dest, v, size);
}

static void sendError(AsyncWebServerRequest* request, const char* msg) {
    request->send(400, "application/json", String("{\"error\":\"") + msg + "\"}");
}

// ============================================================
// Setup / loop
// ============================================================

void webserverInit() {
    fsReady = LittleFS.begin(true);
    if (!fsReady) Serial.println("[Web] LittleFS mount failed - running with defaults");

    // Always load the config (falls back to defaults if the filesystem is unavailable)
    loadConfig();

    ws.onEvent(onWsEvent);
    // Browsers send the basic auth credentials of the page along with the WebSocket upgrade
    ws.handleHandshake([](AsyncWebServerRequest* request) { return authOk(request); });
    server.addHandler(&ws);

    // Serve ONLY index.html. Serving the whole filesystem would expose /config.json
    // (WiFi and MQTT credentials) to everybody in the network.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        if (fsReady && LittleFS.exists(WEB_FILE)) {
            AsyncWebServerResponse* response = request->beginResponse(LittleFS, WEB_FILE, "text/html");
            response->addHeader("Content-Encoding", "gzip");
            response->addHeader("Cache-Control", "no-cache");
            request->send(response);
        } else {
            request->send(200, "text/html",
                "<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width'>"
                "<h2>" DEVICE_NAME "</h2><p>Web page missing. Upload <code>data/index.html.gz</code> "
                "or a firmware (.bin):</p><form method=POST action=/api/update enctype=multipart/form-data>"
                "<input type=file name=f accept='.bin,.gz'> <button>Upload</button></form>");
        }
    });

    server.on("/api/config", HTTP_GET, handleApiGetConfig);
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL,
              [](AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
                  // Checked per chunk: the body handler runs before the request handler
                  if (!authOk(request)) {
                      if (index + len == total) request->requestAuthentication();
                      return;
                  }
                  handleApiSaveConfig(request, data, len, index, total);
              });

    // Firmware (.bin) or web page (.gz) upload; the file name decides
    server.on("/api/update", HTTP_POST,
        [](AsyncWebServerRequest* request) {
            if (!auth(request)) return;
            if (!upAuthorized) { request->send(401, "text/plain", "Login fehlt"); return; }
            if (upWebPage) {
                if (upFailed) request->send(500, "text/plain", "Web-Oberfläche nicht gespeichert (LittleFS voll oder nicht bereit)");
                else request->send(200, "text/plain", "OK web");
                return;
            }
            // firmware: checked and written by fwcheck
            if (!fwcheck::ok()) { request->send(400, "text/plain", "Update abgelehnt: " + fwcheck::error()); return; }
            guard::otaDone();
            request->send(200, "text/plain", "OK");
            restartPending = true;
            restartAt = millis() + 1000;
        },
        [](AsyncWebServerRequest* request, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (index == 0) {
                upAuthorized = authOk(request);
                if (!upAuthorized) return;
                String n = filename;
                n.toLowerCase();
                upWebPage = n.endsWith(".gz");
                upFailed = false;
                Serial.printf("[OTA] Web upload (%s): %s\n", upWebPage ? "web page" : "firmware", filename.c_str());
                if (upWebPage) {
                    upFile = fsReady ? LittleFS.open(WEB_TMP, "w") : File();
                    if (!upFile) upFailed = true;
                } else {
                    fwcheck::begin(request->contentLength(), request->hasParam("force"));
                }
            }
            if (!upAuthorized) return;
            if (upWebPage) {
                if (upFailed) return;
                if (len && upFile.write(data, len) != len) upFailed = true;
                if (final) {
                    upFile.close();
                    if (!upFailed) { LittleFS.remove(WEB_FILE); upFailed = !LittleFS.rename(WEB_TMP, WEB_FILE); }
                    else LittleFS.remove(WEB_TMP);
                    if (upFailed) evlog::add("Web-Oberfläche nicht gespeichert");
                    else evlog::add("Web-Oberfläche ersetzt (%u kB)", (unsigned)((index + len) / 1024));
                }
                return;
            }
            fwcheck::chunk(data, len, final);
        });

    // Own actions: keep the handler short, real work in loop()
    server.on("/api/measure", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        sensorRequestMeasure();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Command for the Alde panel. Only queued here, sent in loop() (sensor.cpp)
    server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        AldeCiCommand cmd;
        aldeCiCommandClear(cmd);
        // One digit each; anything else becomes 9 and is rejected (junk must never turn into 0 = off)
        auto digit = [request](const char* name) -> int8_t {
            const String& v = request->getParam(name)->value();
            return (v.length() == 1 && isDigit(v[0])) ? (int8_t)(v[0] - '0') : 9;
        };
        if (request->hasParam("target")) {
            const String& v = request->getParam("target")->value();
            cmd.hasTarget = true;
            cmd.target = (v.length() > 0 && isDigit(v[0])) ? v.toFloat() : -1;
        }
        if (request->hasParam("gas")) cmd.gas = digit("gas");
        if (request->hasParam("el_level")) cmd.el_level = digit("el_level");
        if (request->hasParam("water")) cmd.water = digit("water");
        if (request->hasParam("power")) cmd.power = digit("power");
        const char* err = sensorControl(cmd);
        if (err) { sendError(request, err); return; }
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Firmware info for the update check in the browser, and the event log

    server.on("/api/system", HTTP_GET, [](AsyncWebServerRequest* request) {

        if (!auth(request)) return;

        JsonDocument d;

        fwcheck::info(d["fw"].to<JsonObject>());

        evlog::json(d["log"].to<JsonArray>());

        d["up"] = millis() / 1000;

        String out;

        serializeJson(d, out);

        request->send(200, "application/json", out);

    });


    // ci_mode 3: frames other devices put on the external panel bus
    server.on("/api/sniff", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        JsonDocument d;
        d["active"] = sensorSniffActive();
        const AldeSniffer& s = sensorSniffer();
        d["bytes"] = s.bytes;
        d["frames"] = s.frames;
        d["junk"] = s.junk;
        unsigned long now = millis();
        JsonArray ids = d["ids"].to<JsonArray>();
        for (uint8_t id = 0; id < 64; id++) {
            const AldeSniffId& e = s.ids[id];
            if (!e.headers) continue;
            JsonObject o = ids.add<JsonObject>();
            o["id"] = id;
            o["pid"] = aldeLinPid(id);
            o["headers"] = e.headers;
            o["answers"] = e.responses;
            o["bad"] = e.bad;
            o["len"] = e.len;
            if (e.len) {
                char hex[3 * 8 + 1];
                int p = 0;
                for (int k = 0; k < e.len; k++) p += snprintf(hex + p, sizeof(hex) - p, "%02X ", e.data[k]);
                if (p > 0) hex[p - 1] = 0;
                o["data"] = hex;
                o["lin2"] = e.enhanced;
            }
            o["age"] = (now - e.last_ms) / 1000;
        }
        String out;
        serializeJson(d, out);
        request->send(200, "application/json", out);
    });
    server.on("/api/sniff_reset", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        sensorSniffReset();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // Debug tab
    server.on("/api/log", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        String out;
        if (!dbglogGet(out)) out = "";
        request->send(200, "text/plain; charset=utf-8", out);
    });
    server.on("/api/log_clear", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        dbglogClear();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    server.on("/api/debug", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        JsonDocument d;
        sensorDebugJson(d.to<JsonObject>());
        d["uptime"] = millis() / 1000;
        d["heap"] = ESP.getFreeHeap();
        String out;
        serializeJson(d, out);
        request->send(200, "application/json", out);
    });
    server.on("/api/debug_dump", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        sensorRequestDump();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* request) {
        if (!auth(request)) return;
        evlog::add("Neustart über die Web-Oberfläche");
        request->send(200, "application/json", "{\"status\":\"restarting\"}");
        restartPending = true;   // do not block the async web task
        restartAt = millis() + 500;
    });
}

void webserverStart() {
    server.begin();
    Serial.println("[Web] Server started on port 80");
}

void webserverLoop() {
    ws.cleanupClients(2);

    if (configDirty) {
        configDirty = false;
        saveConfig();
    }

    // Deferred restart: the HTTP response can be sent before the chip resets
    if (restartPending && millis() >= restartAt) { guard::plannedRestart(); ESP.restart(); }

    if (millis() - lastWsUpdate > WS_UPDATE_MS) {
        lastWsUpdate = millis();
        if (ws.count() > 0) wsSendLiveData();
    }
}

// Live data for the web UI: the value list (with names and units) plus device status
void wsSendLiveData() {
    if (!ws.availableForWriteAll()) return;   // Slow client: skip this update instead of queueing

    JsonDocument doc;
    unsigned long now = millis();
    JsonArray arr = doc["values"].to<JsonArray>();
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        JsonObject o = arr.add<JsonObject>();
        o["key"] = VALUES[i].key;
        o["name"] = VALUES[i].name;
        o["unit"] = VALUES[i].unit;
        o["dec"] = VALUES[i].decimals;
        float v = sensorData.values[i];
        if (isnan(v)) o["v"] = nullptr; else o["v"] = v;
    }
    doc["ok"] = sensorData.ok;
    doc["age"] = sensorData.last_ok ? (long)((now - sensorData.last_ok) / 1000) : -1;
    doc["errors"] = sensorData.errors;
    doc["reads"] = sensorData.readCount;
    doc["rssi"] = sensorData.rssi;
    doc["uptime"] = now / 1000;
    doc["heap"] = (uint32_t)ESP.getFreeHeap();
    doc["wifi_drops"] = sensorData.wifi_drops;
    doc["wifi_reason"] = (const char*)sensorData.wifi_reason;
    doc["wifi"] = getWiFiStateString();
    doc["mqtt"] = mqttIsConnected();

    static char buffer[4096];   // static: keeps it off the stack
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0 || len >= sizeof(buffer)) return;
    ws.textAll(buffer, len);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) lastWsUpdate = 0;   // Send the current values with the next loop
}

// ============================================================
// REST API
// ============================================================

static void handleApiGetConfig(AsyncWebServerRequest* request) {
    if (!auth(request)) return;
    JsonDocument doc;

    // Passwords are never sent back to the browser
    doc["wifi_ssid"] = (const char*)config.wifi_ssid;
    doc["ap_ssid"] = (const char*)config.ap_ssid;
    doc["wifi_pass_set"] = strlen(config.wifi_pass) > 0;
    doc["wifi_tx_power"] = config.wifi_tx_power;
    doc["ap_always"] = config.ap_always;
    doc["reset_reason"] = resetReasonString();
    doc["mqtt_server"] = (const char*)config.mqtt_server;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = (const char*)config.mqtt_user;
    doc["mqtt_pass_set"] = strlen(config.mqtt_pass) > 0;
    doc["mqtt_prefix"] = (const char*)config.mqtt_prefix;
    doc["web_user"] = (const char*)config.web_user;
    doc["web_pass_set"] = strlen(config.web_pass) > 0;
    doc["version"] = FW_VERSION;
    doc["name"] = DEVICE_NAME;
    doc["hostname"] = OTA_HOSTNAME;

    // Device settings
    doc["interval"] = config.interval_s;
    doc["ci_mode"] = config.ci_mode;
    doc["ci_function"] = config.ci_function;
    doc["ci_ok"] = sensorCiOk();
    doc["lin_enabled"] = config.lin_enabled;
    doc["lin_active"] = sensorLinActive();      // differs from lin_enabled until the restart
    doc["el_max"] = config.el_max;
    doc["debug"] = config.debug;
    doc["ci_possible"] = sensorCiPossible();
    doc["one_uart"] = SOC_UART_NUM <= 2;        // ESP32-C3: heater bus OR panel bus

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

static void handleApiSaveConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    static bool rejected = false;

    if (index == 0) {
        body = "";
        rejected = (total > MAX_BODY_SIZE);
    }
    if (rejected) {
        if (index + len == total) request->send(413, "application/json", "{\"error\":\"Body too large\"}");
        return;
    }
    body.concat((const char*)data, len);   // Body is not 0-terminated
    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    body = "";
    if (err) { sendError(request, "Invalid JSON"); return; }

    // ---- Validate everything first, before anything is changed ----
    if (doc["ap_pass"].is<const char*>()) {
        size_t apLen = strlen(doc["ap_pass"].as<const char*>());
        if (apLen > 0 && apLen < 8) { sendError(request, "AP password needs at least 8 characters"); return; }
    }
    bool hasTx = doc["wifi_tx_power"].is<int>();
    if (hasTx && !isValidTxPower(doc["wifi_tx_power"].as<int>())) { sendError(request, "Invalid TX power"); return; }

    // The login stays on: an empty user or password keeps the stored one (see apply below)
    if (doc["mqtt_prefix"].is<const char*>()) {
        const char* p = doc["mqtt_prefix"].as<const char*>();
        if (p[0] && (strchr(p, '#') || strchr(p, '+') || p[strlen(p) - 1] == '/')) {
            sendError(request, "Invalid topic prefix"); return;
        }
    }

    // Device settings
    bool hasInterval = isNumber(doc["interval"]);
    int newInterval = hasInterval ? doc["interval"].as<int>() : 0;
    if (hasInterval && (newInterval < 1 || newInterval > 600)) { sendError(request, "Interval must be 1-600 s"); return; }
    bool hasCiMode = isNumber(doc["ci_mode"]);
    int newCiMode = hasCiMode ? doc["ci_mode"].as<int>() : 0;
    if (hasCiMode && (newCiMode < 0 || newCiMode > 3)) { sendError(request, "ci_mode must be 0-3"); return; }
    bool hasCiFn = isNumber(doc["ci_function"]);
    int newCiFn = hasCiFn ? doc["ci_function"].as<int>() : 0;
    if (hasCiFn && (newCiFn < 1 || newCiFn > 3)) { sendError(request, "ci_function must be 1-3"); return; }
    bool hasElMax = isNumber(doc["el_max"]);
    int newElMax = hasElMax ? doc["el_max"].as<int>() : 0;
    if (hasElMax && (newElMax < 0 || newElMax > 3)) { sendError(request, "el_max must be 0-3"); return; }
    bool hasLin = doc["lin_enabled"].is<bool>();
    bool hasDebug = doc["debug"].is<bool>();

    // ---- Apply ----
    // Empty values clear SSID/server/user; empty passwords are ignored (= keep the stored one)
    // WiFi change while connected: keep the working values as fallback (safeguard.h)
    if (isConnected()) {
        const char* newSsid = doc["wifi_ssid"].is<const char*>() ? doc["wifi_ssid"].as<const char*>() : config.wifi_ssid;
        const char* newPass = config.wifi_pass;
        if (doc["wifi_pass"].is<const char*>() && doc["wifi_pass"].as<const char*>()[0]) newPass = doc["wifi_pass"].as<const char*>();
        if (strcmp(newSsid, config.wifi_ssid) != 0 || strcmp(newPass, config.wifi_pass) != 0) {
            guard::wifiTrialStart(config.wifi_ssid, config.wifi_pass);
        }
    }
    evlog::add("Einstellungen gespeichert");
    copyIfPresent(doc, "wifi_ssid",   config.wifi_ssid,   sizeof(config.wifi_ssid),   false);
    copyIfPresent(doc, "wifi_pass",   config.wifi_pass,   sizeof(config.wifi_pass),   true);
    copyIfPresent(doc, "ap_ssid",     config.ap_ssid,     sizeof(config.ap_ssid),     true);
    copyIfPresent(doc, "ap_pass",     config.ap_pass,     sizeof(config.ap_pass),     true);
    copyIfPresent(doc, "mqtt_server", config.mqtt_server, sizeof(config.mqtt_server), false);
    copyIfPresent(doc, "mqtt_user",   config.mqtt_user,   sizeof(config.mqtt_user),   false);
    copyIfPresent(doc, "mqtt_pass",   config.mqtt_pass,   sizeof(config.mqtt_pass),   true);
    copyIfPresent(doc, "mqtt_prefix", config.mqtt_prefix, sizeof(config.mqtt_prefix), true);
    copyIfPresent(doc, "web_user",    config.web_user,    sizeof(config.web_user),    true);
    copyIfPresent(doc, "web_pass",    config.web_pass,    sizeof(config.web_pass),    true);

    // WiFi TX power and AP mode take effect after the restart triggered by the web UI
    if (hasTx) config.wifi_tx_power = (int8_t)doc["wifi_tx_power"].as<int>();
    if (doc["ap_always"].is<bool>()) config.ap_always = doc["ap_always"].as<bool>();
    if (doc["mqtt_port"].is<int>()) {
        int port = doc["mqtt_port"].as<int>();
        config.mqtt_port = (port > 0 && port <= 65535) ? (uint16_t)port : DEFAULT_MQTT_PORT;
    }

    // Device settings
    if (hasInterval) config.interval_s = (uint16_t)newInterval;
    if (hasInterval) sensorRequestMeasure();   // publish right away
    if (hasCiFn && newCiFn != config.ci_function) {
        config.ci_function = (uint8_t)newCiFn;
        mqttRequestDiscovery();                   // model name in HA
    }
    if (hasElMax && newElMax != config.el_max) {
        evlog::add("Heizstab höchstens %d kW", newElMax);
        config.el_max = (uint8_t)newElMax;
        mqttRequestDiscovery();                   // options of the HA select
    }
    if (hasDebug && doc["debug"].as<bool>() != config.debug) {
        config.debug = doc["debug"].as<bool>();
        evlog::add("Debug %s", config.debug ? "an" : "aus");
        dbglogf("[DBG] Detailed log %s", config.debug ? "on" : "off");
    }
    bool linRestart = false;
    if (hasLin && doc["lin_enabled"].as<bool>() != config.lin_enabled) {
        config.lin_enabled = doc["lin_enabled"].as<bool>();
        evlog::add("Heizbus %s", config.lin_enabled ? "an" : "aus");
        linRestart = true;                        // UARTs are assigned at boot
    }
    if (hasCiMode && newCiMode != config.ci_mode) {
        evlog::add("Bedienteil-Bus: Modus %d", newCiMode);
        config.ci_mode = (uint8_t)newCiMode;      // picked up by sensorLoop(), no restart needed
        mqttRequestDiscovery();                   // control entities appear / disappear in HA
        sensorRequestMeasure();
    }

    configDirty = true;   // Flash write in loop(), not in the web task
    if (linRestart) {
        request->send(200, "application/json", "{\"status\":\"ok\",\"restart\":true}");
        restartPending = true;                    // after the flash write in loop()
        restartAt = millis() + 1500;
        return;
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

// ============================================================
// Persistence (/config.json in LittleFS)
// ============================================================

void loadConfig() {
    setDefaults();
    if (!fsReady) return;
    if (!LittleFS.exists(CONFIG_FILE)) {
        saveConfig();
        return;
    }
    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Serial.printf("[Config] Parse error (%s) - using defaults\n", err.c_str());
        return;
    }

    strlcpy(config.wifi_ssid,   doc["wifi_ssid"]   | DEFAULT_WIFI_SSID,     sizeof(config.wifi_ssid));
    strlcpy(config.wifi_pass,   doc["wifi_pass"]   | DEFAULT_WIFI_PASSWORD, sizeof(config.wifi_pass));
    strlcpy(config.ap_ssid,     doc["ap_ssid"]     | defaultApSsid(),       sizeof(config.ap_ssid));
    strlcpy(config.ap_pass,     doc["ap_pass"]     | DEFAULT_AP_PASSWORD,   sizeof(config.ap_pass));
    int tx = doc["wifi_tx_power"] | DEFAULT_TX_POWER;
    config.wifi_tx_power = isValidTxPower(tx) ? (int8_t)tx : DEFAULT_TX_POWER;
    config.ap_always = doc["ap_always"] | DEFAULT_AP_ALWAYS;
    strlcpy(config.mqtt_server, doc["mqtt_server"] | DEFAULT_MQTT_SERVER,   sizeof(config.mqtt_server));
    config.mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
    strlcpy(config.mqtt_user,   doc["mqtt_user"]   | DEFAULT_MQTT_USER,     sizeof(config.mqtt_user));
    strlcpy(config.mqtt_pass,   doc["mqtt_pass"]   | DEFAULT_MQTT_PASSWORD, sizeof(config.mqtt_pass));
    strlcpy(config.mqtt_prefix, doc["mqtt_prefix"] | DEFAULT_MQTT_PREFIX,   sizeof(config.mqtt_prefix));
    strlcpy(config.web_user,    doc["web_user"]    | DEFAULT_WEB_USER,      sizeof(config.web_user));
    strlcpy(config.web_pass,    doc["web_pass"]    | DEFAULT_WEB_PASSWORD,  sizeof(config.web_pass));
    // Factory login if none is stored (up to 2026-11 empty = open) or if it is still the factory
    // login of the first 2026-11 releases (password vannode2026). A login set by the user stays.
    if (config.web_user[0] == '\0' || config.web_pass[0] == '\0' || strcmp(config.web_pass, "vannode2026") == 0) {
        strlcpy(config.web_user, DEFAULT_WEB_USER, sizeof(config.web_user));
        strlcpy(config.web_pass, DEFAULT_WEB_PASSWORD, sizeof(config.web_pass));
    }

    // Device settings (always with a range check, the file could be old or edited)
    int iv = doc["interval"] | DEFAULT_INTERVAL_S;
    config.interval_s = (iv >= 1 && iv <= 600) ? (uint16_t)iv : DEFAULT_INTERVAL_S;
    int cm = doc["ci_mode"] | DEFAULT_CI_MODE;
    config.ci_mode = (cm >= 0 && cm <= 3) ? (uint8_t)cm : DEFAULT_CI_MODE;
    int cf = doc["ci_function"] | DEFAULT_CI_FUNCTION;
    config.ci_function = (cf >= 1 && cf <= 3) ? (uint8_t)cf : DEFAULT_CI_FUNCTION;
    config.lin_enabled = doc["lin_enabled"] | DEFAULT_LIN_ENABLED;
    int em = doc["el_max"] | DEFAULT_EL_MAX;
    config.el_max = (em >= 0 && em <= 3) ? (uint8_t)em : DEFAULT_EL_MAX;
    config.debug = doc["debug"] | false;
}

void saveConfig() {
    if (!fsReady) return;
    JsonDocument doc;
    doc["wifi_ssid"] = (const char*)config.wifi_ssid;
    doc["wifi_pass"] = (const char*)config.wifi_pass;
    doc["ap_ssid"] = (const char*)config.ap_ssid;
    doc["ap_pass"] = (const char*)config.ap_pass;
    doc["wifi_tx_power"] = config.wifi_tx_power;
    doc["ap_always"] = config.ap_always;
    doc["mqtt_server"] = (const char*)config.mqtt_server;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = (const char*)config.mqtt_user;
    doc["mqtt_pass"] = (const char*)config.mqtt_pass;
    doc["mqtt_prefix"] = (const char*)config.mqtt_prefix;
    doc["web_user"] = (const char*)config.web_user;
    doc["web_pass"] = (const char*)config.web_pass;

    // Device settings
    doc["interval"] = config.interval_s;
    doc["ci_mode"] = config.ci_mode;
    doc["ci_function"] = config.ci_function;
    doc["lin_enabled"] = config.lin_enabled;
    doc["el_max"] = config.el_max;
    doc["debug"] = config.debug;

    // Write to a temp file first so a power loss cannot leave a half-written config
    const char* tmpPath = "/config.tmp";
    File file = LittleFS.open(tmpPath, "w");
    if (!file) { Serial.println("[Config] Cannot open temp file for writing"); return; }
    size_t written = serializeJsonPretty(doc, file);
    file.close();
    if (written == 0) {
        LittleFS.remove(tmpPath);
        Serial.println("[Config] Write failed");
        return;
    }
    LittleFS.remove(CONFIG_FILE);
    if (!LittleFS.rename(tmpPath, CONFIG_FILE)) Serial.println("[Config] Rename failed");
}
