// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "mqtt_ha.h"
#include "mqttfb.h"
#include "devinfo.h"
#include "wifi_manager.h"
#include "io.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Topic layout (prefix = config.mqtt_prefix, default "camper/io"):
//   <prefix>/status          online / offline (last will, retained)
//   <prefix>/rssi            WiFi signal
//   <prefix>/<id>            one topic per channel (io.h), retained:
//                            relay / input ON or OFF, temp and analog the value
//   <prefix>/cmd/<id>        relay: ON, OFF or TOGGLE (also 1 / 0)
//
// Home Assistant: one device DEVICE_NAME; relays as switch, inputs as binary_sensor,
// temperatures and analog values as sensor, plus WiFi signal and status.

#define HA_NODE_ID  "camper_io"

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static unsigned long lastPublish = 0;
static unsigned long lastReconnect = 0;
static bool discoveryPublished = false;

static String topic(const String& sub) {
    return String(config.mqtt_prefix) + "/" + sub;
}

static void publishStates(bool all);

// Commands from MQTT (runs in loop(), so it may call everything)
static void onMessage(char* t, byte* payload, unsigned int len) {
    String tp(t);
    String cmdBase = topic("cmd/");
    if (!tp.startsWith(cmdBase)) return;
    String cmd = tp.substring(cmdBase.length());
    String msg;
    for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
    Serial.printf("[MQTT] Command %s = %s\n", cmd.c_str(), msg.c_str());

    if (!ioCommand(cmd.c_str(), msg.c_str())) Serial.printf("[MQTT] Unknown channel or payload: %s\n", cmd.c_str());
}

void mqttInit() {
    if (strlen(config.mqtt_server) == 0) {
        Serial.println("[MQTT] No server configured");
        return;
    }
    mqttfb::begin(config.mqtt_server, config.mqtt_port, config.mqtt_fb);
    mqtt.setServer(mqttfb::host(), mqttfb::port());
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
            mqtt.setServer(mqttfb::host(), mqttfb::port());   // main or fallback broker (mqttfb.h)
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
            mqttfb::result(connected);
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
    if (mqttfb::backDue()) { mqtt.disconnect(); return; }   // main broker is back
    if (devinfoDue(mqtt.connected())) mqtt.publish(topic("info").c_str(), devinfoJson(FW_VERSION).c_str(), true);

    if (!discoveryPublished) {
        mqttPublishDiscovery();
        discoveryPublished = true;
        lastPublish = 0;
    }

    // Changes right away, everything at least every MQTT_REPUBLISH_MS
    bool all = lastPublish == 0 || millis() - lastPublish > MQTT_REPUBLISH_MS;
    if (all) lastPublish = millis();
    publishStates(all);
}

bool mqttIsConnected() { return mqtt.connected(); }
void mqttRequestDiscovery() { discoveryPublished = false; }

// ============================================================
// States
// ============================================================

static void publishStates(bool all) {
    for (uint8_t i = 0; i < ioCount; i++) {
        IoLive& l = ioLive[i];
        if (!all && !l.dirty) continue;
        l.dirty = false;
        const IoChannel& c = ioCh[i];
        if (c.type == IO_RELAY || c.type == IO_INPUT) {
            mqtt.publish(topic(c.id).c_str(), l.on ? "ON" : "OFF", true);
        } else if (!isnan(l.v)) {   // unknown value: HA keeps the last one
            char buf[24];
            dtostrf(l.v, 1, c.type == IO_TEMP ? 1 : c.dec, buf);
            mqtt.publish(topic(c.id).c_str(), buf, true);
        }
    }
    if (!all) return;
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
    device["model"] = "ESP32";
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

// HA ids: "-" is not allowed in object ids
static String haId(const char* id) {
    String s = id;
    s.replace("-", "_");
    return s;
}

static void publishSwitch(const IoChannel& c) {
    JsonDocument doc;
    String id = haId(c.id);
    doc["name"] = c.name;
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["state_topic"] = topic(c.id);
    doc["command_topic"] = topic(String("cmd/") + c.id);
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["icon"] = "mdi:electric-switch";
    doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/switch/" HA_NODE_ID "/") + id + "/config", doc);
}

static void publishBinary(const IoChannel& c) {
    JsonDocument doc;
    String id = haId(c.id);
    doc["name"] = c.name;
    doc["unique_id"] = String(HA_NODE_ID "_") + id;
    doc["state_topic"] = topic(c.id);
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["availability_topic"] = topic("status");
    addDevice(doc);
    publishJson(String("homeassistant/binary_sensor/" HA_NODE_ID "/") + id + "/config", doc);
}

// Channels deleted in the web UI: drop their HA entity and the retained state
static void clearRemoved() {
    String r = ioTakeRemoved();   // "id:type id:type "
    int pos = 0;
    while (pos < (int)r.length()) {
        int sp = r.indexOf(' ', pos);
        if (sp < 0) sp = r.length();
        String item = r.substring(pos, sp);
        pos = sp + 1;
        int c = item.indexOf(':');
        if (c <= 0) continue;
        String id = item.substring(0, c), type = item.substring(c + 1);
        const char* comp = type == "relay" ? "switch" : type == "input" ? "binary_sensor" : "sensor";
        mqtt.publish((String("homeassistant/") + comp + "/" HA_NODE_ID "/" + haId(id.c_str()) + "/config").c_str(), "", true);
        mqtt.publish(topic(id).c_str(), "", true);
    }
}

void mqttPublishDiscovery() {
    Serial.println("[MQTT] Publishing HA Discovery...");
    clearRemoved();
    for (uint8_t i = 0; i < ioCount; i++) {
        const IoChannel& c = ioCh[i];
        String id = haId(c.id);
        switch (c.type) {
            case IO_RELAY: publishSwitch(c); break;
            case IO_INPUT: publishBinary(c); break;
            case IO_TEMP:
                publishSensor(c.name, id.c_str(), topic(c.id), "°C", "temperature", "mdi:thermometer", 1, true);
                break;
            case IO_ANALOG:
                publishSensor(c.name, id.c_str(), topic(c.id), c.unit, nullptr, "mdi:gauge", c.dec, true);
                break;
        }
        mqtt.loop();   // keep the connection alive with many channels
    }
    publishSensor("WLAN Signal", "rssi", topic("rssi"), "dBm", "signal_strength", "mdi:wifi", 0, true);
    publishSensor("Status", "status", topic("status"), "", nullptr, "mdi:check-circle", -1, false);
    Serial.println("[MQTT] Discovery complete");
}
