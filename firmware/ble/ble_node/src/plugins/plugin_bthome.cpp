// Plugin "bthome": any sensor speaking BTHome v2 without encryption (ATC / pvvx firmware,
// Shelly BLU H&T / Door / Button / Motion, ...). Only the MAC is set; the values are
// recognized from the packet (decoding in bthome_decode.h, host test in firmware/ble/tests).
//
// The recognized values are kept in the setting "objs" (object ids in packet order, e.g.
// "02,03,01"), so the Home Assistant entities exist right after a restart, before the
// first packet. New values are added when they show up; clearing the field starts over.

#include "../core/plugin.h"

#if PLUGIN_BTHOME

#include "../core/util.h"
#include "../core/settings.h"
#include "../core/mqtt_ha.h"
#include "bthome_decode.h"
#include <NimBLEDevice.h>

#define BTH_MAX BTHOME_VALUES

struct BthCfg {
    char mac[18];
    char objs[BTH_MAX * 3 + 1];   // "02,03,01" - filled automatically
};

static const Field BTH_FIELDS[] = {
    FLD_MAC(BthCfg, mac, "mac", "Sensor-MAC", "Über \"Suchen\" aus der Liste wählen (passt: Dienst FCD2)"),
    FLD_TEXT(BthCfg, objs, "objs", "Erkannte Werte",
             "Füllt sich beim Empfang von selbst. Leeren und speichern, um die Werte neu zu erkennen"),
};

static const char* const BTH_KEYS_FALLBACK[] = {"v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12"};
static_assert(BTH_MAX >= 1 && BTH_MAX <= 12, "BTHOME_VALUES must be 1..12");

class BtHomePlugin : public Plugin {
public:
    explicit BtHomePlugin(uint8_t index) : Plugin("bthome", "BTHome-Sensor", "BTHome", index) {
        portMUX_INITIALIZE(&mux);
        memset(&c, 0, sizeof(c));
        buildSlots();
    }

    size_t fields(const Field*& out) override {
        out = BTH_FIELDS;
        return sizeof(BTH_FIELDS) / sizeof(BTH_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override { memset(&c, 0, sizeof(c)); }
    void onConfigChanged() override {
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
        }
        buildSlots();
        updates++;
    }

    uint8_t bleUse() const override { return BLE_PASSIVE; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t, const char* uuid, const char*) const override {
        return uuid && containsIgnoreCase(uuid, "fcd2");
    }

    // NimBLE host task: copy the payload, decoding happens in loop()
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        if (c.mac[0] == '\0' || strcmp(addr, c.mac) != 0) return;
        std::string p = dev->getServiceData(NimBLEUUID((uint16_t)0xFCD2));
        if (p.empty()) return;
        size_t n = p.size() > BLE_RAW_MAX ? BLE_RAW_MAX : p.size();
        portENTER_CRITICAL(&mux);
        memcpy(buf, p.data(), n);
        bufLen = n;
        bufRssi = dev->getRSSI();
        pending = true;
        portEXIT_CRITICAL(&mux);
    }

    void loop() override {
        uint8_t d[BLE_RAW_MAX];
        size_t n = 0;
        bool have = false;
        portENTER_CRITICAL(&mux);
        if (pending) {
            memcpy(d, buf, bufLen);
            n = bufLen;
            rssi = bufRssi;
            pending = false;
            have = true;
        }
        portEXIT_CRITICAL(&mux);
        if (have) handle(d, n);

        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            updates++;
        }
    }

    bool available() override {
        return hasData && millis() - lastUpdate <= BTHOME_TIMEOUT_MS;
    }

    void live(JsonObject o) override {
        const char* st = c.mac[0] == '\0'        ? "Kein Sensor"
                         : result == BTH_ENCRYPTED ? "Verschlüsselt (nicht unterstützt)"
                         : result == BTH_WRONG_VERSION ? "Kein BTHome v2"
                         : !hasData                ? "Warte auf Daten"
                         : available()             ? "OK"
                                                   : "Keine Daten";
        o["state"] = st;
        o["ok"] = available();
        for (uint8_t i = 0; i < nSlots; i++) {
            const BthObject* ob = bthObject(slotId[i]);
            if (!ob) continue;
            if (ob->kind == BTH_BINARY) liveRowText(o, names[i], isnan(val[i]) ? "–" : binText(ob, val[i] != 0));
            else if (ob->kind == BTH_EVENT) liveRowText(o, names[i], evt[i][0] ? evt[i] : "–");
            else liveRowNum(o, names[i], val[i], ob->dec, ob->unit);
        }
        if (!nSlots) liveRowText(o, "Werte", "noch keine erkannt");
        if (result == BTH_UNKNOWN_OBJECT) liveRowText(o, "Hinweis", "Paket enthält unbekannte Werte");
        liveRowNum(o, "Bluetooth", hasData ? (float)rssi : NAN, 0, "dBm");
        liveRowNum(o, "Alter", hasData ? (millis() - lastUpdate) / 1000.0f : NAN, 0, "s");
        o["raw"] = (const char*)rawHex;
    }

    size_t entities(const Entity*& out) override {
        out = ents;
        return BTH_MAX + 1;
    }
    bool entityActive(const Entity& e) override {
        for (uint8_t i = 0; i < BTH_MAX; i++)
            if (&e == &ents[i]) return i < nSlots;
        return true;
    }
    uint32_t publishIntervalMs() const override { return MQTT_BTHOME_MS; }

    void publish(MqttOut& out) override {
        if (!hasData) return;
        for (uint8_t i = 0; i < nSlots; i++) {
            const BthObject* ob = bthObject(slotId[i]);
            if (!ob) continue;
            if (ob->kind == BTH_BINARY) {
                if (!isnan(val[i])) out.onoff(keys[i], val[i] != 0);
            } else if (ob->kind == BTH_EVENT) {
                if (evt[i][0]) out.text(keys[i], evt[i]);
            } else {
                out.num(keys[i], val[i], ob->dec);
            }
        }
        out.integer("rssi", rssi);
    }

private:
    BthCfg c;
    char lastMac[18] = "";

    portMUX_TYPE mux;
    uint8_t buf[BLE_RAW_MAX];
    size_t bufLen = 0;
    int bufRssi = 0;
    bool pending = false;

    // Slots = recognized values (from c.objs)
    uint8_t nSlots = 0;
    uint8_t slotId[BTH_MAX];
    char keys[BTH_MAX][24];
    char names[BTH_MAX][32];
    float val[BTH_MAX];
    char evt[BTH_MAX][20];
    Entity ents[BTH_MAX + 1];

    BthResult result = BTH_OK;
    bool hasData = false;
    bool lastAvail = false;
    int rssi = 0;
    unsigned long lastUpdate = 0;
    char rawHex[BLE_RAW_MAX * 3 + 1] = "";

    static const char* binText(const BthObject* ob, bool on) {
        if (ob->devClass && (!strcmp(ob->devClass, "door") || !strcmp(ob->devClass, "window") ||
                             !strcmp(ob->devClass, "opening") || !strcmp(ob->devClass, "garage_door")))
            return on ? "offen" : "zu";
        return on ? "ja" : "nein";
    }

    void handle(const uint8_t* d, size_t n) {
        bytesToHex(d, n, rawHex, sizeof(rawHex));
        BthValue v[BTH_MAX * 2];
        size_t cnt = 0;
        result = bthParse(d, n, v, BTH_MAX * 2, cnt);
        if (result != BTH_OK && result != BTH_UNKNOWN_OBJECT) {
            updates++;
            return;
        }

        // Match the values to the slots: the k-th object of an id goes to the k-th slot of that id.
        // Values not known yet get a new slot (some devices send different packets).
        bool grown = false, changed = !hasData;
        for (size_t i = 0; i < cnt; i++) {
            uint8_t occ = 0;
            for (size_t j = 0; j < i; j++) if (v[j].id == v[i].id) occ++;
            int slot = -1;
            uint8_t seen = 0;
            for (uint8_t s = 0; s < nSlots; s++) {
                if (slotId[s] != v[i].id) continue;
                if (seen++ == occ) { slot = s; break; }
            }
            if (slot < 0) {
                if (nSlots >= BTH_MAX) continue;
                slot = nSlots;
                addSlot(v[i].id);
                grown = true;
            }
            const BthObject* ob = bthObject(v[i].id);
            if (ob && ob->kind == BTH_EVENT) {
                const char* t = bthEventText(v[i].id, v[i].v);
                if (t[0]) {   // every event is published, also the same one again
                    strlcpy(evt[slot], t, sizeof(evt[slot]));
                    changed = true;
                }
            } else {
                if (isnan(val[slot]) || fabsf(val[slot] - v[i].v) > 1e-6f) changed = true;
                val[slot] = v[i].v;
            }
        }
        hasData = true;
        lastUpdate = millis();
        if (grown) {
            // Remember the values, so Home Assistant keeps the entities after a restart
            objsFromSlots();
            saveConfig();
            mqttRequestDiscovery();
        }
        if (changed || grown) updates++;
    }

    void addSlot(uint8_t id) {
        uint8_t i = nSlots++;
        slotId[i] = id;
        const BthObject* ob = bthObject(id);
        // Key: "temperature", second one "temperature_2" (unique within the device)
        const char* base = ob ? ob->key : BTH_KEYS_FALLBACK[i];
        uint8_t dup = 1;
        for (uint8_t j = 0; j < i; j++) {
            const BthObject* o2 = bthObject(slotId[j]);
            if (o2 && ob && !strcmp(o2->key, ob->key)) dup++;
        }
        if (dup > 1) {
            snprintf(keys[i], sizeof(keys[i]), "%s_%u", base, dup);
            snprintf(names[i], sizeof(names[i]), "%s %u", ob ? ob->name : base, dup);
        } else {
            strlcpy(keys[i], base, sizeof(keys[i]));
            strlcpy(names[i], ob ? ob->name : base, sizeof(names[i]));
        }
        val[i] = NAN;
        evt[i][0] = '\0';
        if (!ob) {
            ents[i] = {E_SENSOR, keys[i], names[i], nullptr, nullptr, nullptr, nullptr, 0, nullptr};
        } else if (ob->kind == BTH_BINARY) {
            ents[i] = {E_BINARY, keys[i], names[i], nullptr, ob->devClass, nullptr, nullptr, 0, nullptr};
        } else if (ob->kind == BTH_EVENT) {
            ents[i] = {E_SENSOR, keys[i], names[i], nullptr, nullptr, nullptr, "mdi:gesture-tap-button", 0, nullptr};
        } else {
            uint8_t flags = !strcmp(ob->key, "battery") ? EF_DIAG : 0;
            ents[i] = {E_SENSOR, keys[i], names[i], ob->unit, ob->devClass, "measurement", nullptr, flags, nullptr};
        }
    }

    // Slots from the setting "objs" ("02,03,01"); invalid entries are skipped
    void buildSlots() {
        nSlots = 0;
        const char* s = c.objs;
        while (*s && nSlots < BTH_MAX) {
            char* end = nullptr;
            long id = strtol(s, &end, 16);
            if (end == s) { s++; continue; }
            if (id >= 0 && id <= 0xFF) {
                const BthObject* ob = bthObject((uint8_t)id);
                if (ob && ob->kind != BTH_SKIP) addSlot((uint8_t)id);
            }
            s = end;
        }
        for (uint8_t i = nSlots; i < BTH_MAX; i++)
            ents[i] = {E_SENSOR, BTH_KEYS_FALLBACK[i], BTH_KEYS_FALLBACK[i], nullptr, nullptr, nullptr, nullptr, 0, nullptr};
        ents[BTH_MAX] = {E_SENSOR, "rssi", "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr};
    }

    void objsFromSlots() {
        size_t o = 0;
        c.objs[0] = '\0';
        for (uint8_t i = 0; i < nSlots && o + 3 < sizeof(c.objs); i++)
            o += snprintf(c.objs + o, sizeof(c.objs) - o, "%s%02X", i ? "," : "", slotId[i]);
    }
};

Plugin* createBtHome(uint8_t index) {
    return new BtHomePlugin(index);
}

#endif // PLUGIN_BTHOME
