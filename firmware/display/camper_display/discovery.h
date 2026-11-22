// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#pragma once
// Home Assistant MQTT discovery configs of other devices (ESPHome, Tasmota, OpenMQTTGateway,
// zigbee2mqtt ...) for the topic finder: <prefix>/<component>/[<node>/]<object>/config.
// discSummary() turns one config into a short JSON the web page makes a tile from:
//   {"d":1,"c":"sensor","n":"Bad Temperatur","t":"<state topic>","k":"<json key>","u":"°C",
//    "dc":"temperature","ct":"<command topic>","on":"ON","off":"OFF","so":"ON"}
// Only sensor, binary_sensor, switch and light (on/off). "" = not usable: other component,
// no state topic, removed (empty payload) or one of VanNode's own devices (own prefixes).
// Abbreviated keys ("stat_t", "cmd_t", "~" ...) are understood. Header only, no Arduino types
// besides String, so the logic stays easy to follow.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>

namespace disc {

// full key or its abbreviation
inline JsonVariantConst get(JsonObjectConst o, const char* full, const char* abbr) {
  JsonVariantConst v = o[full];
  if (v.isNull() && abbr) v = o[abbr];
  return v;
}

// "~/state" with "~" = "tasmota/bad" -> "tasmota/bad/state" (also "~" at the end)
inline String topicOf(JsonObjectConst o, const char* full, const char* abbr, const String& base) {
  String t = get(o, full, abbr) | "";
  if (base.length()) {
    if (t.startsWith("~")) t = base + t.substring(1);
    else if (t.endsWith("~")) t = t.substring(0, t.length() - 1) + base;
  }
  return t;
}

// JSON key from a value template: "{{ value_json.temperature }}", "{{ value_json['a b'] }}",
// "{{ value_json.ENERGY.Power }}" -> "ENERGY.Power". No value_json = raw payload ("").
inline String keyOf(const String& tpl) {
  int i = tpl.indexOf("value_json");
  if (i < 0) return "";
  String k;
  size_t p = i + 10;
  while (p < tpl.length()) {
    char c = tpl[p];
    if (c == '.') {
      p++;
      String part;
      while (p < tpl.length() && (isalnum((unsigned char)tpl[p]) || tpl[p] == '_' || tpl[p] == '-')) part += tpl[p++];
      if (part.isEmpty()) break;
      if (k.length()) k += '.';
      k += part;
    } else if (c == '[') {
      int q = tpl.indexOf(']', p);
      if (q < 0) break;
      String part = tpl.substring(p + 1, q);
      part.trim();
      if (part.length() >= 2 && (part[0] == '\'' || part[0] == '"')) part = part.substring(1, part.length() - 1);
      if (k.length()) k += '.';
      k += part;
      p = q + 1;
    } else {
      break;
    }
  }
  return k;
}

inline String summary(const char* topic, const char* payload, unsigned len, const std::vector<String>& own) {
  if (!len) return "";   // empty retained config = entity removed
  // <prefix>/<component>/[<node>/]<object>/config
  String t = topic;
  int a = t.indexOf('/');
  int b = a < 0 ? -1 : t.indexOf('/', a + 1);
  if (b < 0 || !t.endsWith("/config")) return "";
  String comp = t.substring(a + 1, b);
  if (comp != "sensor" && comp != "binary_sensor" && comp != "switch" && comp != "light") return "";
  // VanNode devices announce themselves as node "camper_<device>": not offered again
  if (t.substring(b + 1).startsWith("camper_")) return "";

  JsonDocument d;
  if (deserializeJson(d, payload, len) || !d.is<JsonObject>()) return "";
  JsonObjectConst o = d.as<JsonObjectConst>();
  String base = o["~"] | "";
  String st = topicOf(o, "state_topic", "stat_t", base);
  if (st.isEmpty()) return "";
  for (auto& p : own)
    if (p.length() && (st == p || st.startsWith(p + "/"))) return "";   // VanNode's own nodes

  JsonObjectConst dev = get(o, "device", "dev");
  String devName = dev.isNull() ? String("") : String(dev["name"] | "");
  String name = get(o, "name", nullptr) | "";
  if (name.isEmpty()) name = devName;
  else if (devName.length() && !name.startsWith(devName)) name = devName + " " + name;
  if (name.isEmpty()) name = st;

  String tpl = get(o, "value_template", "val_tpl") | "";
  bool jsonLight = comp == "light" && String(o["schema"] | "") == "json";
  String key = jsonLight ? String("state") : keyOf(tpl);
  if (comp == "light" && !jsonLight) {
    String stpl = get(o, "state_value_template", "stat_val_tpl") | "";
    if (stpl.length()) key = keyOf(stpl);
  }

  JsonDocument out;
  out["d"] = 1;
  out["c"] = comp;
  out["n"] = name;
  out["t"] = st;
  if (key.length()) out["k"] = key;
  String unit = get(o, "unit_of_measurement", "unit_of_meas") | "";
  if (unit.length()) out["u"] = unit;
  String dc = get(o, "device_class", "dev_cla") | "";
  if (dc.length()) out["dc"] = dc;
  String on = get(o, "payload_on", "pl_on") | "ON";
  String off = get(o, "payload_off", "pl_off") | "OFF";
  if (comp == "switch" || comp == "light") {
    String ct = topicOf(o, "command_topic", "cmd_t", base);
    if (ct.length()) out["ct"] = ct;
    String so = get(o, "state_on", "stat_on") | "";
    out["so"] = so.length() ? so : (jsonLight ? String("ON") : on);
    if (jsonLight) { on = "{\"state\":\"ON\"}"; off = "{\"state\":\"OFF\"}"; }
  }
  out["on"] = on;
  out["off"] = off;
  String s;
  serializeJson(out, s);
  return s;
}

}  // namespace disc
