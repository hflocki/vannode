#ifndef CT_SETTINGS_H
#define CT_SETTINGS_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "plugin.h"

// /config.json layout:
//   { <core keys: wifi_ssid, mqtt_server, ...>,
//     "plugins": { "<id>": { "type": "dimes", "enabled": true, "name": "Gas", <plugin fields> }, ... } }
// Every entry of "plugins" is one device; they are created at boot in this order.

bool settingsInit();       // Mounts LittleFS, loads the config (defaults if missing)
bool settingsFsReady();
// Writes the devices as they will be after the next restart: removed ones are left out,
// pending ones (added in the web UI) are included, switches as set (enabledNext)
void saveConfig();
void settingsFactoryReset();   // Deletes /config.json (applied after a restart)

// Core settings
void coreConfigToJson(JsonObject o);   // Without passwords (only "<key>_set")
// Validates the web UI values. Returns an error text or nullptr; restart = network settings changed
const char* coreConfigCheck(JsonObjectConst in, bool& restart);
void coreConfigApply(JsonObjectConst in);

// Plugin settings (driven by the plugin's Field table)
void pluginSchemaToJson(Plugin* p, JsonArray fields);
void pluginConfigToJson(Plugin* p, JsonObject o);   // includes "enabled" and "name"
// write = false: only validate. Returns an error text or nullptr.
// restart is set when "enabled" changes.
const char* pluginConfigFromJson(Plugin* p, JsonObjectConst in, bool write, bool& restart);

#endif // CT_SETTINGS_H
