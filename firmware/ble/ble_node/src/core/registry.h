#ifndef CT_REGISTRY_H
#define CT_REGISTRY_H

#include "plugin.h"

// All compiled-in plugin instances (enabled or not), in a fixed order
void registryCreate();
uint8_t pluginCount();
Plugin* pluginAt(uint8_t i);
Plugin* pluginById(const char* id);

// True if at least one enabled plugin uses Bluetooth
bool registryNeedsBle();
// Enabled plugins that keep a Bluetooth connection (except one, e.g. the one being changed)
uint8_t registryLinksOn(const Plugin* except = nullptr);
// More than MAX_BLE_LINKS connecting plugins switched on (edited config): switch off the rest
void registryEnforceLinkLimit();

#define FOR_EACH_PLUGIN(p) \
    for (uint8_t _i = 0; _i < pluginCount(); _i++) \
        for (Plugin* p = pluginAt(_i); p; p = nullptr)

#define FOR_EACH_ENABLED_PLUGIN(p) \
    for (uint8_t _i = 0; _i < pluginCount(); _i++) \
        for (Plugin* p = pluginAt(_i); p && p->enabled; p = nullptr)

#endif // CT_REGISTRY_H
