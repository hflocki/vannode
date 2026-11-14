// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "mqtt_ha.h"
#include "devinfo.h"
#include "wifi_manager.h"
#include "sensor.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Topic layout (prefix = config.mqtt_prefix, default "camper/alde"):
//   <prefix>/status          online / offline (last will, retained)
//   <prefix>/rssi            WiFi signal
//   <prefix>/<key>           one topic per entry of VALUES[] (sensor.cpp), retained
//   <prefix>/cmd/measure     any payload: publish the current readings right away
//   <prefix>/cmd/target      setpoint 5-30 (0.5 steps)          \
//   <prefix>/cmd/gas         1 / 0 (also ON / OFF)               | only with ci_mode 2,
//   <prefix>/cmd/el_level    0-3 (kW)                            | sent to the Alde panel
//   <prefix>/cmd/water       0 = off, 1 = on, 2 = boost          /  over the external bus
//
// Home Assistant: one device DEVICE_NAME, one sensor entity per value (keys in
// BINARY_KEYS become binary sensors), plus WiFi signal, status and a "send now" button.
// Values of the external panel bus only with ci_mode > 0, control entities (number,
// switch, selects) only with ci_mode 2.

#define HA_NODE_ID  "camper_alde"

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static unsigned long lastPublish = 0;
static unsigned long lastReconnect = 0;
static bool discoveryPublished = false;
static uint32_t lastPublishedRead = 0;

static String topic(const String& sub) {
    return String(config.mqtt_prefix) + "/" + sub;
}

static void publishStates();

// Commands from MQTT (runs in loop(), so it may call everything)
static void onMessage(char* t, byte* payload, unsigned int len) {
    String tp(t);
    String cmdBase = topic("cmd/");
    if (!tp.startsWith(cmdBase)) return;
    String cmd = tp.substring(cmdBase.length());
    String msg;
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    Serial.printf("[MQTT] Command %s = %s\n", cmd.c_str(), msg.c_str());

    if (cmd == "measure") {
        sensorRequestMeasure();
        return;
    }

    // Commands for the Alde panel
    AldeCiCommand c;
    aldeCiCommandClear(c);
    msg.trim();
    String up = msg;
    up.toUpperCase();
    if (cmd == "target") {
        c.hasTarget = true;
        c.target = (msg.length() > 0 && (isDigit(msg[0]))) ? msg.toFloat() : -1;   // -1 is rejected
    } else if (cmd == "gas") {
        c.gas = (up == "ON" || up == "1") ? 1 : (up == "OFF" || up == "0") ? 0 : 9;
    } else if (cmd == "el_level") {
        // Junk must not become 0 (= heating element off): unknown text -> invalid value 9
        c.el_level = (msg.length() == 1 && isDigit(msg[0])) ? (int8_t)(msg[0] - '0') : 9;
    } else if (cmd == "water") {
        c.water = (up == "OFF" || up == "AUS" || up == "0") ? 0 : (up == "ON" || up == "AN" || up == "1") ? 1
                : (up == "BOOST" || up == "2") ? 2 : 9;
    } else {
        return;
    }
    const char* err = sensorControl(c);
    if (err) Serial.printf("[MQTT] Command rejected: %s\n", err);
}

void mqttInit() {
    if (strlen(config.mqtt_server) == 0) {
        Serial.println("[MQTT] No server configured");
        return;
    }
    mqtt.setServer(config.mqtt_server, config.mqtt_port);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(5);
    mqtt.setCallback(onMessage);
    Serial.printf("[MQTT] Configured: %s:%d\n", config.mqtt_server, config.mqtt_port);
}

void mqttLoop() {
    if (!isConnected()) return;
    if (strlen(config.mqtt_server) == 0) return;

    if (!mqtt.connected()) {
        // connect() blocks while the broker is unreachable, so retry only every 5 s
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            Serial.println("[MQTT] Connection attempt...");
            String clientId = String(OTA_HOSTNAME) + "-" + WiFi.macAddress();
            clientId.replace(":", "");
            String willTopic = topic("status");
            bool connected;
            if (strlen(config.mqtt_user) > 0) {
                connected = mqtt.connect(clientId.c_str(), config.mqtt_user, config.mqtt_pass,
                                         willTopic.c_str(), 1, true, "offline");
            } else {
                connected = mqtt.connect(clientId.c_str(), NULL, NULL, willTopic.c_str(), 1, true, "offline");
            }
            if (connected) {
                Serial.println("[MQTT] Connected!");
                mqtt.publish(willTopic.c_str(), "online", true);
                mqtt.subscribe(topic("cmd/#").c_str());
                discoveryPublished = false;   // Always re-announce after (re)connect
                lastPublish = 0;              // Publish all states right away
            } else {
                Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
            }
        }
        return;
    }

    mqtt.loop();
    if (devinfoDue(mqtt.connected())) mqtt.publish(topic("info").c_str(), devinfoJson(FW_VERSION).c_str(), true);

    if (!discoveryPublished) {
        mqttPublishDiscovery();
        discoveryPublished = true;
        lastPublish = 0;
    }

    // After every measurement cycle, and at least every MQTT_REPUBLISH_MS
    bool newData = (sensorData.readCount != lastPublishedRead);
    if (newData || lastPublish == 0 || millis() - lastPublish > MQTT_REPUBLISH_MS) {
        lastPublish = millis();
        lastPublishedRead = sensorData.readCount;
        publishStates();
    }
}

bool mqttIsConnected() { return mqtt.connected(); }
void mqttRequestDiscovery() { discoveryPublished = false; }

// ============================================================
// States
// ============================================================

static void publishStates() {
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        float v = sensorData.values[i];
        if (isnan(v)) continue;   // HA keeps the last value
        char buf[24];
        dtostrf(v, 1, VALUES[i].decimals, buf);
        mqtt.publish(topic(VALUES[i].key).c_str(), buf, true);
    }
    mqtt.publish(topic("rssi").c_str(), String(sensorData.rssi).c_str(), true);
    mqtt.publish(topic("status").c_str(), "online", true);
}

// ============================================================
// Home Assistant discovery
// ============================================================

static void addDevice(JsonDocument& doc) {
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = HA_NODE_ID;
    device["name"] = DEVICE_NAME;
    device["model"] = "Alde Compact 3020 HE LIN reader";
    device["manufacturer"] = "DIY";
    device["sw_version"] = FW_VERSION;
    device["configuration_url"] = String("http://") + WiFi.localIP().toString() + "/";
}

static void publishJson(const String& configTopic, JsonDocument& doc) {
    static char buffer[768];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0 || len >= sizeof(buffer)) {
        Serial.printf("[MQTT] Discovery payload too large: %s\n", configTopic.c_str());
        return;
    }
    mqtt.publish(configTopic.c_str(), buffer, true);
}

static void publishSensor(const char* name, const char* id, const String& stateTopic, const char* unit,
                          const char* devClass, const char* icon, int precision, bool withAvailability) {
    JsonDocument doc;
    doc["name"] = name;
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["state_topic"] = stateTopic;
    doc["icon"] = icon;
    if (unit && unit[0]) doc["unit_of_measurement"] = unit;
    if (devClass) { doc["device_class"] = devClass; doc["state_class"] = "measurement"; }
    if (precision >= 0) doc["suggested_display_precision"] = precision;
    // The status sensor skips availability on purpose, so HA shows "offline" instead of "unavailable"
    if (withAvailability) doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/sensor/" HA_NODE_ID "/") + id + "/config", doc);
}

// Values published as 1/0 that are binary sensors in Home Assistant
static const char* const BINARY_KEYS[][2] = {
    // key          device_class
    {"gas",         nullptr},
    {"ac_active",   "running"},
    {"bus_ok",      "connectivity"},
    {"gas_enabled", nullptr},
    {"pump",        "running"},
    {"panel_on",    "power"},
    {"error",       "problem"},
    {"ac_input",    "plug"},
    {"ci_ok",       "connectivity"},
};

static const char* binaryClass(const char* key, bool* isBinary) {
    for (auto& b : BINARY_KEYS) {
        if (strcmp(b[0], key) == 0) { *isBinary = true; return b[1]; }
    }
    *isBinary = false;
    return nullptr;
}

static void publishBinary(const char* name, const char* id, const char* devClass, const char* icon) {
    JsonDocument doc;
    doc["name"] = name;
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["state_topic"] = topic(id);
    doc["payload_on"] = "1";
    doc["payload_off"] = "0";
    doc["icon"] = icon;
    if (devClass) doc["device_class"] = devClass;
    doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/binary_sensor/" HA_NODE_ID "/") + id + "/config", doc);
}

static void publishButton(const char* name, const char* id, const char* cmd, const char* icon) {
    JsonDocument doc;
    doc["name"] = name;
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["command_topic"] = topic(String("cmd/") + cmd);
    doc["payload_press"] = "1";
    doc["icon"] = icon;
    doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/button/" HA_NODE_ID "/") + id + "/config", doc);
}

// Empty retained config = entity removed in Home Assistant
static void removeEntity(const char* component, const char* id) {
    mqtt.publish((String("homeassistant/") + component + "/" HA_NODE_ID "/" + id + "/config").c_str(), "", true);
}

static void publishControl(const char* component, const char* id, JsonDocument& doc) {
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/") + component + "/" HA_NODE_ID "/" + id + "/config", doc);
}

// Control entities for the Alde panel (ci_mode 2). The state comes from the info frame.
static void publishControls(bool on) {
    if (!on) {
        removeEntity("number", "set_target");
        removeEntity("switch", "set_gas");
        removeEntity("select", "set_el_level");
        removeEntity("select", "set_water");
        return;
    }
    {
        JsonDocument d;
        d["name"] = "Sollwert einstellen";
        d["command_topic"] = topic("cmd/target");
        d["state_topic"] = topic("target");
        d["min"] = 5; d["max"] = 30; d["step"] = 0.5;
        d["unit_of_measurement"] = "°C";
        d["mode"] = "box";
        d["icon"] = "mdi:thermostat";
        publishControl("number", "set_target", d);
    }
    {
        JsonDocument d;
        d["name"] = "Gasbetrieb schalten";
        d["command_topic"] = topic("cmd/gas");
        d["state_topic"] = topic("gas_enabled");
        d["payload_on"] = "1"; d["payload_off"] = "0";
        d["icon"] = "mdi:fire";
        publishControl("switch", "set_gas", d);
    }
    {
        JsonDocument d;
        d["name"] = "Heizstab Stufe einstellen";
        d["command_topic"] = topic("cmd/el_level");
        d["state_topic"] = topic("el_level");
        JsonArray o = d["options"].to<JsonArray>();
        o.add("0"); o.add("1"); o.add("2"); o.add("3");
        d["icon"] = "mdi:flash";
        publishControl("select", "set_el_level", d);
    }
    {
        JsonDocument d;
        d["name"] = "Warmwasser einstellen";
        d["command_topic"] = topic("cmd/water");
        d["state_topic"] = topic("water_mode");
        d["value_template"] = "{{ ['aus','an','boost'][value|int(0)] }}";
        JsonArray o = d["options"].to<JsonArray>();
        o.add("aus"); o.add("an"); o.add("boost");
        d["icon"] = "mdi:water-boiler";
        publishControl("select", "set_water", d);
    }
}

void mqttPublishDiscovery() {
    Serial.println("[MQTT] Publishing HA Discovery...");
    for (uint8_t i = 0; i < VALUE_COUNT; i++) {
        const ValueDef& v = VALUES[i];
        bool isBinary;
        const char* binClass = binaryClass(v.key, &isBinary);
        if (v.ci && config.ci_mode == 0) {
            removeEntity(isBinary ? "binary_sensor" : "sensor", v.key);
            continue;
        }
        if (isBinary) publishBinary(v.name, v.key, binClass, v.icon);
        else publishSensor(v.name, v.key, topic(v.key), v.unit, v.devClass, v.icon, v.decimals, true);
    }
    publishControls(config.ci_mode == 2);
    publishSensor("WLAN Signal", "rssi", topic("rssi"), "dBm", "signal_strength", "mdi:wifi", 0, true);
    publishSensor("Status", "status", topic("status"), "", nullptr, "mdi:check-circle", -1, false);
    publishButton("Jetzt senden", "measure", "measure", "mdi:refresh");
    Serial.println("[MQTT] Discovery complete");
}
