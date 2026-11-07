#ifndef CT_REGISTRY_H
#define CT_REGISTRY_H

#include "plugin.h"

// A device type the firmware knows (config.h PLUGIN_xxx = 1)
struct PluginType {
    const char* type;      // "rvtech"; devices get the ids rvtech, rvtech2, rvtech3 ...
    const char* name;      // Name in the web UI ("Dachhaube RV Tech")
    uint8_t ble;           // BLE_PASSIVE or BLE_LINK
    Plugin* (*create)(uint8_t index);
};

uint8_t pluginTypeCount();
const PluginType* pluginTypeAt(uint8_t i);
const PluginType* pluginType(const char* type);
// Type and index of a device id ("rvtech2" -> rvtech, 1), nullptr if unknown
const PluginType* pluginTypeOfId(const char* id, uint8_t& index);
// Lowest index whose id is not used yet (-1 = none)
int8_t registryFreeIndex(const PluginType* t);
// New device object with factory values (not registered yet)
Plugin* registryNew(const PluginType* t, uint8_t index);
// Register a device (only while loading the config at boot); false = full or id taken
bool registryAdd(Plugin* p);

// Devices added in the web UI since this boot: saved to /config.json, created at the restart
bool registryAddPending(Plugin* p);
// Drop a pending device again (removed before the restart); false = not pending
bool registryDropPending(const char* id);
uint8_t pendingCount();
Plugin* pendingAt(uint8_t i);
// Running or pending device (for settings changes in the web UI)
Plugin* pluginOrPendingById(const char* id);
// Devices after the next restart (not removed + pending): checked against MAX_PLUGINS
uint8_t registryCountNext();
// True if the saved device list differs from the running one (added, removed, switched)
bool registryRestartNeeded();

// All devices of this boot (enabled or not), in the order of /config.json
uint8_t pluginCount();
Plugin* pluginAt(uint8_t i);
Plugin* pluginById(const char* id);

// True if at least one enabled plugin uses Bluetooth
bool registryNeedsBle();
// Plugins switched on for the next restart that keep a Bluetooth connection
// (except one, e.g. the one being changed); pending ones count, removed ones do not
uint8_t registryLinksOn(const Plugin* except = nullptr);
// At boot: more than MAX_BLE_LINKS connecting plugins switched on (edited config): switch off the rest
void registryEnforceLinkLimit();

#define FOR_EACH_PLUGIN(p) \
    for (uint8_t _i = 0; _i < pluginCount(); _i++) \
        for (Plugin* p = pluginAt(_i); p; p = nullptr)

#define FOR_EACH_ENABLED_PLUGIN(p) \
    for (uint8_t _i = 0; _i < pluginCount(); _i++) \
        for (Plugin* p = pluginAt(_i); p && p->enabled && !p->removed; p = nullptr)

#endif // CT_REGISTRY_H
