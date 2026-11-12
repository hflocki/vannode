// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "mqtt_ha.h"
#include "wifi_manager.h"
#include "registry.h"
#include "../../devinfo.h"
#include "../plugins/bms_plugin.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Topic layout (prefix = config.mqtt_prefix, default "camper/tool"):
//   <prefix>/status                        online / offline (last will)
//   <prefix>/info                          diagnostics JSON (devinfo.h), retained
//   <prefix>/rssi                          WiFi signal
//   <prefix>/<plugin>/available            online / offline (device of the plugin reachable)
//   <prefix>/<plugin>/<key>                values, retained
//   <prefix>/cmd/<plugin>/<key>/set        commands, not retained
//   <prefix>/batteries                     all battery devices as one JSON list (displays)
//
// Home Assistant: one device "BLE Node" (WiFi, status) plus one device per plugin
// instance, linked via via_device. Entities come from the plugins' Entity tables.

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static unsigned long lastCorePublish = 0;
static unsigned long lastReconnect = 0;
static bool discoveryPublished = false;

#define MAX_TRACKED MAX_PLUGINS   // one entry per device (config.h)
static uint32_t lastUpdates[MAX_TRACKED];
static unsigned long lastPublish[MAX_TRACKED];

static String topic(const char* sub) {
    return String(config.mqtt_prefix) + "/" + sub;
}

static String pluginTopic(const Plugin* p, const char* key) {
    return String(config.mqtt_prefix) + "/" + p->id + "/" + key;
}

static String cmdTopic(const Plugin* p, const char* key) {
    return String(config.mqtt_prefix) + "/cmd/" + p->id + "/" + key + "/set";
}

const char* mqttNodeId() {
    static char node[40];
    size_t i = 0;
    for (const char* c = config.mqtt_prefix; *c && i < sizeof(node) - 1; c++) {
        node[i++] = isalnum((unsigned char)*c) ? tolower((unsigned char)*c) : '_';
    }
    node[i] = '\0';
    return node;
}

// ============================================================
// Plugin output
// ============================================================
class PluginOut : public MqttOut {
public:
    explicit PluginOut(Plugin* p) : plugin(p) {}
    void text(const char* key, const char* value) override {
        mqtt.publish(pluginTopic(plugin, key).c_str(), value ? value : "", true);
    }
    void num(const char* key, float value, int decimals) override {
        if (isnan(value)) return;
        char buf[20];
        dtostrf(value, 1, decimals, buf);
        text(key, buf);
    }
    void integer(const char* key, long value) override {
        char buf[16];
        ltoa(value, buf, 10);
        text(key, buf);
    }
private:
    Plugin* plugin;
};

static void publishPlugin(Plugin* p) {
    mqtt.publish(pluginTopic(p, "available").c_str(), p->available() ? "online" : "offline", true);
    PluginOut out(p);
    p->publish(out);
}

// ============================================================
// Discovery
// ============================================================

static void publishJson(const String& configTopic, JsonDocument& doc, const char* id) {
    static char buffer[1200];   // static: keeps it off the loop task stack
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0 || len >= sizeof(buffer)) {
        Serial.printf("[MQTT] Discovery payload for '%s' too large\n", id);
        return;
    }
    if (!mqtt.publish(configTopic.c_str(), (const uint8_t*)buffer, len, true)) {
        Serial.printf("[MQTT] Discovery publish for '%s' failed\n", id);
    }
}

static const char* component(EntityKind k) {
    switch (k) {
        case E_SENSOR: return "sensor";
        case E_BINARY: return "binary_sensor";
        case E_SWITCH: return "switch";
        case E_SELECT: return "select";
        case E_BUTTON: return "button";
        case E_COVER:  return "cover";
    }
    return "sensor";
}

static String discoveryTopic(const char* comp, const char* objectId) {
    return String("homeassistant/") + comp + "/" + mqttNodeId() + "/" + objectId + "/config";
}

static void addCoreDevice(JsonDocument& doc) {
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = mqttNodeId();
    device["name"] = (const char*)config.node_name;
    device["model"] = "BLE Node";
    device["sw_version"] = FW_VERSION;
    device["configuration_url"] = String("http://") + WiFi.localIP().toString() + "/";
}

static void addPluginDevice(JsonDocument& doc, const Plugin* p, bool needsDevice) {
    String ident = String(mqttNodeId()) + "_" + p->id;
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = ident;
    device["name"] = (const char*)p->name;
    device["model"] = p->model();
    if (p->manufacturer()[0]) device["manufacturer"] = p->manufacturer();
    device["via_device"] = mqttNodeId();

    // Availability: ESP online AND (optionally) the plugin's device reachable
    JsonArray av = doc["availability"].to<JsonArray>();
    JsonObject a1 = av.add<JsonObject>();
    a1["topic"] = topic("status");
    if (needsDevice) {
        JsonObject a2 = av.add<JsonObject>();
        a2["topic"] = pluginTopic(p, "available");
        doc["availability_mode"] = "all";
    }
}

static void publishEntity(Plugin* p, const Entity& e) {
    String objectId = String(p->id) + "_" + e.key;
    String uid = String(mqttNodeId()) + "_" + objectId;
    JsonDocument doc;
    doc["name"] = e.name;
    doc["unique_id"] = uid;
    if (e.icon) doc["icon"] = e.icon;
    if (e.devClass) doc["device_class"] = e.devClass;
    if (e.flags & EF_DIAG) doc["entity_category"] = "diagnostic";
    else if (e.flags & EF_CONFIG) doc["entity_category"] = "config";
    if (e.flags & EF_HIDDEN) doc["enabled_by_default"] = false;

    switch (e.kind) {
        case E_SENSOR:
            doc["state_topic"] = pluginTopic(p, e.key);
            if (e.unit) doc["unit_of_measurement"] = e.unit;
            if (e.stateClass) doc["state_class"] = e.stateClass;
            break;
        case E_BINARY:
            doc["state_topic"] = pluginTopic(p, e.key);
            doc["payload_on"] = "ON";
            doc["payload_off"] = "OFF";
            break;
        case E_SWITCH:
            doc["state_topic"] = pluginTopic(p, e.key);
            doc["command_topic"] = cmdTopic(p, e.key);
            doc["payload_on"] = "ON";
            doc["payload_off"] = "OFF";
            break;
        case E_SELECT: {
            doc["state_topic"] = pluginTopic(p, e.key);
            doc["command_topic"] = cmdTopic(p, e.key);
            JsonArray opts = doc["options"].to<JsonArray>();
            String all = e.options ? e.options : "";
            int start = 0;
            while (start <= (int)all.length()) {
                int bar = all.indexOf('|', start);
                if (bar < 0) bar = all.length();
                if (bar > start) opts.add(all.substring(start, bar));
                start = bar + 1;
            }
            break;
        }
        case E_BUTTON:
            doc["command_topic"] = cmdTopic(p, e.key);
            doc["payload_press"] = "PRESS";
            break;
        case E_COVER: {
            String posKey = String(e.key) + "_position";
            doc["state_topic"] = pluginTopic(p, e.key);
            doc["position_topic"] = pluginTopic(p, posKey.c_str());
            doc["command_topic"] = cmdTopic(p, e.key);
            doc["set_position_topic"] = cmdTopic(p, posKey.c_str());
            doc["payload_open"] = "OPEN";
            doc["payload_close"] = "CLOSE";
            doc["payload_stop"] = "STOP";   // Plugin decides whether stop is possible
            break;
        }
    }
    addPluginDevice(doc, p, !(e.flags & EF_ALWAYS));
    publishJson(discoveryTopic(component(e.kind), objectId.c_str()), doc, objectId.c_str());
}

static void removeEntity(Plugin* p, const Entity& e) {
    String objectId = String(p->id) + "_" + e.key;
    mqtt.publish(discoveryTopic(component(e.kind), objectId.c_str()).c_str(), "", true);
}

static void publishCoreSensor(const char* name, const char* id, const char* unit,
                              const char* devClass, const char* icon, bool measurement) {
    JsonDocument doc;
    doc["name"] = name;
    doc["unique_id"] = String(mqttNodeId()) + "_" + id;
    doc["state_topic"] = topic(id);
    if (unit) doc["unit_of_measurement"] = unit;
    if (devClass) doc["device_class"] = devClass;
    if (measurement) doc["state_class"] = "measurement";
    doc["icon"] = icon;
    doc["entity_category"] = "diagnostic";
    addCoreDevice(doc);
    JsonArray av = doc["availability"].to<JsonArray>();
    av.add<JsonObject>()["topic"] = topic("status");
    publishJson(discoveryTopic("sensor", id), doc, id);
}

static void publishDiscovery() {
    Serial.println("[MQTT] Publishing HA Discovery...");

    publishCoreSensor("WLAN Signal", "rssi", "dBm", "signal_strength", "mdi:wifi", true);

    FOR_EACH_PLUGIN(p) {
        const Entity* ents = nullptr;
        size_t n = p->entities(ents);
        for (size_t i = 0; i < n; i++) {
            // Disabled plugins and inactive entities are removed from Home Assistant
            if (p->enabled && !p->removed && p->entityActive(ents[i])) publishEntity(p, ents[i]);
            else removeEntity(p, ents[i]);
            mqtt.loop();   // Keep the connection serviced during the burst
        }
    }
    Serial.println("[MQTT] Discovery done");
}

// ============================================================
// Commands
// ============================================================

// Commands are only queued in the MQTT callback and run after mqtt.loop() has returned:
// a command may block for a while (Bluetooth write with response) and must not run
// inside the MQTT client's own receive code (the web UI runs them in its own task).
struct PendingCmd {
    char id[24];
    char key[32];
    char value[64];
};
#define CMD_QUEUE 6
static PendingCmd cmdQueue[CMD_QUEUE];
static uint8_t cmdHead = 0, cmdCount = 0;

static void runQueuedCommands() {
    while (cmdCount) {
        PendingCmd c = cmdQueue[cmdHead];
        cmdHead = (cmdHead + 1) % CMD_QUEUE;
        cmdCount--;
        Plugin* p = pluginById(c.id);
        if (!p || !p->enabled || p->removed) continue;
        Serial.printf("[MQTT] Command %s/%s = %s\n", c.id, c.key, c.value);
        p->command(c.key, c.value);
    }
}

static void mqttCallback(char* topicName, byte* payload, unsigned int length) {
    // <prefix>/cmd/<plugin>/<key>/set
    String prefix = topic("cmd/");
    String t = topicName;
    if (!t.startsWith(prefix) || !t.endsWith("/set")) return;
    String rest = t.substring(prefix.length(), t.length() - 4);
    int slash = rest.indexOf('/');
    if (slash <= 0) return;
    String id = rest.substring(0, slash);
    String key = rest.substring(slash + 1);

    char value[64];
    unsigned int n = length < sizeof(value) - 1 ? length : sizeof(value) - 1;
    memcpy(value, payload, n);
    value[n] = '\0';
    String v = value;
    v.trim();

    if (cmdCount == CMD_QUEUE) {
        Serial.printf("[MQTT] Command queue full, %s/%s dropped\n", id.c_str(), key.c_str());
        return;
    }
    PendingCmd& c = cmdQueue[(cmdHead + cmdCount) % CMD_QUEUE];
    strlcpy(c.id, id.c_str(), sizeof(c.id));
    strlcpy(c.key, key.c_str(), sizeof(c.key));
    strlcpy(c.value, v.c_str(), sizeof(c.value));
    cmdCount++;
}

// ============================================================
// Public API
// ============================================================

void mqttInit() {
    if (strlen(config.mqtt_server) == 0) {
        Serial.println("[MQTT] No server configured");
        return;
    }
    mqtt.setServer(config.mqtt_server, config.mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(1280);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(5);
    Serial.printf("[MQTT] Configured: %s:%d, prefix %s, HA node %s\n",
        config.mqtt_server, config.mqtt_port, config.mqtt_prefix, mqttNodeId());
}

void mqttLoop() {
    if (!isConnected()) return;
    if (strlen(config.mqtt_server) == 0) return;

    if (!mqtt.connected()) {
        // connect() blocks while the broker is unreachable, so retry only every 5 s
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            Serial.println("[MQTT] Connection attempt...");

            String clientId = String(config.node_name) + "-" + WiFi.macAddress();
            clientId.replace(":", "");
            String willTopic = topic("status");
            bool connected;
            if (strlen(config.mqtt_user) > 0) {
                connected = mqtt.connect(clientId.c_str(), config.mqtt_user, config.mqtt_pass,
                                         willTopic.c_str(), 0, true, "offline");
            } else {
                connected = mqtt.connect(clientId.c_str(), NULL, NULL, willTopic.c_str(), 0, true, "offline");
            }

            if (connected) {
                Serial.println("[MQTT] Connected!");
                mqtt.subscribe(topic("cmd/#").c_str());
                discoveryPublished = false;   // Always re-announce after (re)connect
                mqtt.publish(willTopic.c_str(), "online", true);
                lastCorePublish = 0;          // Publish all states right away
                memset(lastPublish, 0, sizeof(lastPublish));
            } else {
                Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
            }
        }
        return;
    }

    mqtt.loop();
    runQueuedCommands();
    if (!mqtt.connected()) return;

    if (devinfoDue(mqtt.connected())) mqtt.publish(topic("info").c_str(), devinfoJson(FW_VERSION).c_str(), true);

    if (!discoveryPublished) {
        publishDiscovery();
        discoveryPublished = true;
        memset(lastPublish, 0, sizeof(lastPublish));
    }

    if (lastCorePublish == 0 || millis() - lastCorePublish > MQTT_CORE_MS) {
        lastCorePublish = millis();
        char buf[8];
        itoa(coreData.rssi, buf, 10);
        mqtt.publish(topic("rssi").c_str(), buf, true);
        mqtt.publish(topic("status").c_str(), "online", true);
    }

    // Plugins: immediately on changes, otherwise periodically
    for (uint8_t i = 0; i < pluginCount() && i < MAX_TRACKED; i++) {
        Plugin* p = pluginAt(i);
        if (!p->enabled || p->removed) continue;
        bool changed = p->updates != lastUpdates[i];
        if (changed || lastPublish[i] == 0 || millis() - lastPublish[i] > p->publishIntervalMs()) {
            lastPublish[i] = millis();
            if (lastPublish[i] == 0) lastPublish[i] = 1;
            lastUpdates[i] = p->updates;
            publishPlugin(p);
        }
    }

#if CT_BMS
    // Batteries: one list for the displays, on every new reading (at most every 2 s) and once a minute
    static uint32_t listSeen = 0;
    static unsigned long listAt = 0;
    if (bmsAny() && ((bmsListChanges != listSeen && millis() - listAt > 2000) || listAt == 0 ||
                     millis() - listAt > 60000)) {
        listSeen = bmsListChanges;
        listAt = millis() ? millis() : 1;
        static char buf[1000];
        size_t n = bmsBatteryList(buf, sizeof(buf));
        if (n) mqtt.publish(topic("batteries").c_str(), buf, true);
    }
#endif
}

void mqttForget(Plugin* p) {
    if (!p || !mqtt.connected()) return;
    const Entity* ents = nullptr;
    size_t n = p->entities(ents);
    for (size_t i = 0; i < n; i++) {
        removeEntity(p, ents[i]);
        // retained state of the entity (a cover also has <key>_position)
        mqtt.publish(pluginTopic(p, ents[i].key).c_str(), "", true);
        if (ents[i].kind == E_COVER) mqtt.publish(pluginTopic(p, (String(ents[i].key) + "_position").c_str()).c_str(), "", true);
        mqtt.loop();
    }
    mqtt.publish(pluginTopic(p, "available").c_str(), "", true);
    Serial.printf("[MQTT] %s removed from Home Assistant\n", p->id);
}

bool mqttIsConnected() {
    return mqtt.connected();
}

void mqttRequestDiscovery() {
    discoveryPublished = false;
}
