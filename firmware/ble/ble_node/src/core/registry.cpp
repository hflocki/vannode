// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "registry.h"

// ============================================================
// Plugin types (one factory per type, implemented in src/plugins/)
// To add a plugin type: write src/plugins/plugin_<name>.cpp with a factory, add a
// PLUGIN_xxx switch to config.h and one line to TYPES below.
// Devices (instances) are not fixed here: they are created from /config.json, which the
// web UI changes with "+ Gerät hinzufügen" / "Entfernen".
// ============================================================
#if PLUGIN_DIMES
Plugin* createDimes(uint8_t index);
#endif
#if PLUGIN_MOPEKA_PRO
Plugin* createMopekaPro(uint8_t index);
#endif
#if PLUGIN_MOPEKA_STD
Plugin* createMopekaStd(uint8_t index);
#endif
#if PLUGIN_BLESENSOR
Plugin* createBleSensor(uint8_t index);
#endif
#if PLUGIN_GOVEE
Plugin* createGovee(uint8_t index);
#endif
#if PLUGIN_BTHOME
Plugin* createBtHome(uint8_t index);
#endif
#if PLUGIN_DALY
Plugin* createDaly(uint8_t index);
#endif
#if PLUGIN_ECTIVE
Plugin* createEctive(uint8_t index);
#endif
#if PLUGIN_BM2
Plugin* createBm2(uint8_t index);
#endif
#if PLUGIN_CARAWATER
Plugin* createCaraWater(uint8_t index);
#endif
#if PLUGIN_RVTECH
Plugin* createRvTech(uint8_t index);
#endif
#if PLUGIN_WCS_FRIDGE
Plugin* createWcsFridge(uint8_t index);
#endif
#if PLUGIN_WIPRO
Plugin* createWipro(uint8_t index);
#endif

// type id (= id of the first device of this type), name for the web UI, Bluetooth use
static const PluginType TYPES[] = {
#if PLUGIN_RVTECH
    {"rvtech", "Dachhaube RV Tech", BLE_LINK, createRvTech},
#endif
#if PLUGIN_CARAWATER
    {"pump", "Wasserpumpe CaraWater", BLE_LINK, createCaraWater},
#endif
#if PLUGIN_WCS_FRIDGE
    {"fridge", "Kühlschrank WCS", BLE_LINK, createWcsFridge},
#endif
#if PLUGIN_WIPRO
    {"wipro", "Alarm WiPro III (nur lesen)", BLE_LINK, createWipro},
#endif
#if PLUGIN_DALY
    {"daly", "Batterie: Daly BMS", BLE_LINK, createDaly},
#endif
#if PLUGIN_ECTIVE
    {"ective", "Batterie: Ective BM X", BLE_LINK, createEctive},
#endif
#if PLUGIN_BM2
    {"bm2", "Batterie: BM2 Batteriewächter", BLE_LINK, createBm2},
#endif
#if PLUGIN_DIMES
    {"dimes", "Gas: DIMES / SRG WAVE", BLE_PASSIVE, createDimes},
#endif
#if PLUGIN_MOPEKA_PRO
    {"mopeka", "Gas: Mopeka Pro", BLE_PASSIVE, createMopekaPro},
#endif
#if PLUGIN_MOPEKA_STD
    {"mopeka_std", "Gas: Mopeka Standard", BLE_PASSIVE, createMopekaStd},
#endif
#if PLUGIN_GOVEE
    {"govee", "Thermometer Govee", BLE_PASSIVE, createGovee},
#endif
#if PLUGIN_BTHOME
    {"bthome", "BTHome-Sensor (ATC/pvvx, Shelly BLU …)", BLE_PASSIVE, createBtHome},
#endif
#if PLUGIN_BLESENSOR
    {"blesensor", "Eigener BLE-Sensor", BLE_PASSIVE, createBleSensor},
#endif
};
static const uint8_t TYPE_COUNT = sizeof(TYPES) / sizeof(TYPES[0]);

static Plugin* plugins[MAX_PLUGINS];
static uint8_t count = 0;
static Plugin* pending[MAX_PLUGINS];
static uint8_t nPending = 0;

uint8_t pluginTypeCount() { return TYPE_COUNT; }
const PluginType* pluginTypeAt(uint8_t i) { return i < TYPE_COUNT ? &TYPES[i] : nullptr; }

const PluginType* pluginType(const char* type) {
    for (uint8_t i = 0; i < TYPE_COUNT; i++) {
        if (!strcmp(TYPES[i].type, type)) return &TYPES[i];
    }
    return nullptr;
}

// "rvtech" -> type rvtech, index 0; "rvtech2" -> index 1; "mopeka_std3" -> mopeka_std, 2.
// The longest matching type wins ("mopeka_std2" is not "mopeka" + "_std2").
const PluginType* pluginTypeOfId(const char* id, uint8_t& index) {
    const PluginType* best = nullptr;
    size_t bestLen = 0;
    for (uint8_t i = 0; i < TYPE_COUNT; i++) {
        size_t n = strlen(TYPES[i].type);
        if (n <= bestLen || strncmp(id, TYPES[i].type, n) != 0) continue;
        const char* rest = id + n;
        bool digits = true;
        for (const char* c = rest; *c; c++) if (!isdigit((unsigned char)*c)) digits = false;
        if (!digits) continue;
        int num = *rest ? atoi(rest) : 1;
        if (num < 1 || num > 99 || (*rest == '0')) continue;
        best = &TYPES[i];
        bestLen = n;
        index = (uint8_t)(num - 1);
    }
    return best;
}

static void idFor(const PluginType* t, uint8_t index, char* out, size_t size) {
    if (index == 0) strlcpy(out, t->type, size);
    else snprintf(out, size, "%s%u", t->type, (unsigned)(index + 1));
}

int8_t registryFreeIndex(const PluginType* t) {
    char id[16];
    for (uint8_t i = 0; i < 99; i++) {
        idFor(t, i, id, sizeof(id));
        if (!pluginOrPendingById(id)) return (int8_t)i;
    }
    return -1;
}

Plugin* registryNew(const PluginType* t, uint8_t index) {
    if (!t) return nullptr;
    Plugin* p = t->create(index);
    if (p) p->setDefaults();
    return p;
}

bool registryAdd(Plugin* p) {
    if (!p || count >= MAX_PLUGINS || pluginById(p->id)) return false;
    plugins[count++] = p;
    return true;
}

bool registryAddPending(Plugin* p) {
    if (!p || nPending >= MAX_PLUGINS || pluginOrPendingById(p->id)) return false;
    pending[nPending++] = p;
    return true;
}

bool registryDropPending(const char* id) {
    for (uint8_t i = 0; i < nPending; i++) {
        if (strcmp(pending[i]->id, id) != 0) continue;
        // Not deleted: the web task may be reading it right now (a few hundred bytes until the restart)
        for (uint8_t j = i + 1; j < nPending; j++) pending[j - 1] = pending[j];
        nPending--;
        return true;
    }
    return false;
}

uint8_t pendingCount() { return nPending; }
Plugin* pendingAt(uint8_t i) { return i < nPending ? pending[i] : nullptr; }

Plugin* pluginOrPendingById(const char* id) {
    Plugin* p = pluginById(id);
    if (p) return p;
    for (uint8_t i = 0; i < nPending; i++) {
        if (strcmp(pending[i]->id, id) == 0) return pending[i];
    }
    return nullptr;
}

uint8_t registryCountNext() {
    uint8_t n = nPending;
    for (uint8_t i = 0; i < count; i++) if (!plugins[i]->removed) n++;
    return n;
}

bool registryRestartNeeded() {
    if (nPending) return true;
    for (uint8_t i = 0; i < count; i++) {
        if (plugins[i]->removed || plugins[i]->enabled != plugins[i]->enabledNext) return true;
    }
    return false;
}

uint8_t pluginCount() {
    return count;
}

Plugin* pluginAt(uint8_t i) {
    return i < count ? plugins[i] : nullptr;
}

Plugin* pluginById(const char* id) {
    for (uint8_t i = 0; i < count; i++) {
        if (strcmp(plugins[i]->id, id) == 0) return plugins[i];
    }
    return nullptr;
}

uint8_t registryLinksOn(const Plugin* except) {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++) {
        Plugin* p = plugins[i];
        if (p != except && !p->removed && p->enabledNext && p->bleUse() == BLE_LINK) n++;
    }
    for (uint8_t i = 0; i < nPending; i++) {
        if (pending[i] != except && pending[i]->enabledNext && pending[i]->bleUse() == BLE_LINK) n++;
    }
    return n;
}

void registryEnforceLinkLimit() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < count; i++) {
        Plugin* p = plugins[i];
        if (!p->enabled || p->bleUse() != BLE_LINK) continue;
        if (++n > MAX_BLE_LINKS) {
            p->enabled = p->enabledNext = false;
            Serial.printf("[MAIN] %s switched off: more than %d Bluetooth connections\n", p->id, MAX_BLE_LINKS);
        }
    }
}

bool registryNeedsBle() {
    for (uint8_t i = 0; i < count; i++) {
        if (plugins[i]->enabled && plugins[i]->bleUse() != BLE_NONE) return true;
    }
    return false;
}

// ============================================================
// Plugin base class + helpers
// ============================================================
Plugin::Plugin(const char* type_, const char* typeName_, const char* title, uint8_t index_)
    : type(type_), typeName(typeName_), index(index_) {
    if (index == 0) {
        strlcpy(id, type, sizeof(id));
        strlcpy(name, title, sizeof(name));
    } else {
        snprintf(id, sizeof(id), "%s%u", type, (unsigned)(index + 1));
        snprintf(name, sizeof(name), "%s %u", title, (unsigned)(index + 1));
    }
}

JsonArray liveRow(JsonObject o, const char* label) {
    JsonArray rows = o["rows"].is<JsonArray>() ? o["rows"].as<JsonArray>() : o["rows"].to<JsonArray>();
    JsonArray r = rows.add<JsonArray>();
    r.add(label);
    return r;
}

void liveRowNum(JsonObject o, const char* label, float v, int decimals, const char* unit) {
    JsonArray r = liveRow(o, label);
    if (isnan(v)) {
        r.add(nullptr);
    } else {
        float f = powf(10.0f, (float)decimals);
        r.add(roundf(v * f) / f);
    }
    r.add(unit ? unit : "");
}

void liveRowText(JsonObject o, const char* label, const char* v) {
    JsonArray r = liveRow(o, label);
    r.add(v);
    r.add("");
}

JsonObject liveCtrl(JsonObject o, const char* t, const char* key, const char* label) {
    JsonArray c = o["ctrl"].is<JsonArray>() ? o["ctrl"].as<JsonArray>() : o["ctrl"].to<JsonArray>();
    JsonObject x = c.add<JsonObject>();
    x["t"] = t;
    x["k"] = key;
    if (label) x["l"] = label;
    return x;
}

void liveZone(JsonObject o, const char* label, float v, int decimals, const char* unit,
              const char* sub, const char* switchKey, bool on) {
    JsonArray zs = o["zones"].is<JsonArray>() ? o["zones"].as<JsonArray>() : o["zones"].to<JsonArray>();
    JsonObject z = zs.add<JsonObject>();
    z["l"] = label;
    if (isnan(v)) {
        z["v"] = nullptr;
    } else {
        float f = powf(10.0f, (float)decimals);
        z["v"] = roundf(v * f) / f;
    }
    if (unit) z["unit"] = unit;
    if (sub) z["s"] = sub;
    if (switchKey) {
        z["k"] = switchKey;
        z["on"] = on;
    }
}

bool payloadOn(const char* v) {
    return v && (strcasecmp(v, "on") == 0 || strcasecmp(v, "true") == 0 || strcmp(v, "1") == 0);
}
