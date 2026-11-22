// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Plugin "victron" (battery-node): Victron Instant Readout, passive BLE. Battery monitor
// (SmartShunt, BMV), solar charger (SmartSolar / BlueSolar) and DC-DC charger (Orion Smart).
// The device type is taken from the advertisement; the encryption key comes from the
// VictronConnect app. A battery monitor is also listed in <prefix>/batteries (plugin_bms.cpp,
// type 7) for the displays. Decoding: victron_decode.h.

#include "../core/plugin.h"

#if PLUGIN_VICTRON

#include "../core/devlog.h"
#include "../core/mqtt_ha.h"
#include "../core/util.h"
#include "victron_decode.h"
#include "bms_plugin.h"
#include "mbedtls/aes.h"
#include <NimBLEDevice.h>

struct VictronCfg {
    char mac[18];
    char key[34];          // 32 hex characters
};

static const Field VICTRON_FIELDS[] = {
    FLD_MAC(VictronCfg, mac, "mac", "Geräte-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_TEXT(VictronCfg, key, "key", "Schlüssel", "32 Zeichen aus der VictronConnect-App: Gerät → Einstellungen (Zahnrad) → "
                                                   "⋮ → Produktinfo → \"Instant Readout\" einschalten → \"Details anzeigen\""),
};

// One table for all three device types; entityActive() switches off what a type does not send
static const Entity VICTRON_ENTITIES[] = {
    {E_SENSOR, "voltage",     "Spannung",          "V",   "voltage",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "current",     "Strom",             "A",   "current",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "power",       "Leistung",          "W",   "power",       "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "pv_power",    "Solarleistung",     "W",   "power",       "measurement", "mdi:solar-power", 0, nullptr},
    {E_SENSOR, "yield_today", "Ertrag heute",      "Wh",  "energy",      "total_increasing", "mdi:solar-power-variant", 0, nullptr},
    {E_SENSOR, "load",        "Laststrom",         "A",   "current",     "measurement", nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "soc",         "Ladestand",         "%",   "battery",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "consumed",    "Entnommen",         "Ah",  nullptr,       "measurement", "mdi:battery-minus", 0, nullptr},
    {E_SENSOR, "runtime",     "Restlaufzeit",      "min", "duration",    "measurement", "mdi:timer-sand", 0, nullptr},
    {E_SENSOR, "temperature", "Temperatur",        "°C",  "temperature", "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "starter",     "Starterbatterie",   "V",   "voltage",     "measurement", "mdi:car-battery", 0, nullptr},
    {E_SENSOR, "midpoint",    "Mittelpunktspannung", "V", "voltage",     "measurement", nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "input",       "Eingangsspannung",  "V",   "voltage",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "mode",        "Zustand",           nullptr, nullptr,     nullptr,       "mdi:state-machine", 0, nullptr},
    {E_SENSOR, "error",       "Fehlercode",        nullptr, nullptr,     nullptr,       "mdi:alert-circle-outline", EF_DIAG, nullptr},
    {E_SENSOR, "off_reason",  "Grund für Aus",     nullptr, nullptr,     nullptr,       "mdi:power-off", EF_DIAG, nullptr},
    {E_BINARY, "alarm",       "Alarm",             nullptr, "problem",   nullptr,       nullptr, 0, nullptr},
    {E_SENSOR, "state",       "Status",            nullptr, nullptr,     nullptr,       "mdi:bluetooth", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "ble_rssi",    "Bluetooth Signal",  "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
};

#define VICTRON_TIMEOUT_MS  (5UL * 60UL * 1000UL)   // nothing for 5 min = no data

static bool vicAesEsp(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]) {
    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    bool ok = mbedtls_aes_setkey_enc(&aes, key, 128) == 0 &&
              mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, in, out) == 0;
    mbedtls_aes_free(&aes);
    return ok;
}

class VictronPlugin : public Plugin {
public:
    explicit VictronPlugin(uint8_t index) : Plugin("victron", "Victron Instant Readout", "Victron", index) {
        portMUX_INITIALIZE(&mux);
        memset(&c, 0, sizeof(c));
        vicClear(cur);
    }

    size_t fields(const Field*& out) override {
        out = VICTRON_FIELDS;
        return sizeof(VICTRON_FIELDS) / sizeof(VICTRON_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override { memset(&c, 0, sizeof(c)); }
    void onConfigChanged() override {
        portENTER_CRITICAL(&mux);
        keyOk = vicParseKey(c.key, keyBin);
        portEXIT_CRITICAL(&mux);
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            rec = 0;
        }
        updates++;
    }
    const char* manufacturer() const override { return "Victron Energy"; }
    const char* model() const override {
        return rec == VIC_REC_SOLAR ? "Solarladeregler" : rec == VIC_REC_BATTMON ? "Batteriemonitor"
             : rec == VIC_REC_DCDC  ? "DC-DC-Ladebooster" : typeName;
    }

    uint8_t bleUse() const override { return BLE_PASSIVE; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return company == VIC_COMPANY;
    }

    // NimBLE host task: decrypt and decode (short, no allocation) and hand over to loop()
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        if (c.mac[0] == '\0' || strcmp(addr, c.mac) != 0) return;
        for (uint8_t i = 0; i < dev->getManufacturerDataCount(); i++) {
            std::string m = dev->getManufacturerData(i);
            if (m.size() < 11) continue;
            if (((uint8_t)m[0] | ((uint8_t)m[1] << 8)) != VIC_COMPANY) continue;
            const uint8_t* p = (const uint8_t*)m.data() + 2;
            size_t n = m.size() - 2;
            uint16_t model;
            uint8_t type;
            if (!vicHeader(p, n, model, type)) continue;
            uint8_t k[16];
            bool haveKey;
            portENTER_CRITICAL(&mux);
            haveKey = keyOk;
            memcpy(k, keyBin, 16);
            portEXIT_CRITICAL(&mux);
            VicReading r;
            uint8_t plain[VIC_MAX_DATA];
            size_t pl = 0;
            bool mismatch = false;
            bool ok = haveKey && vicDecrypt(p, n, k, vicAesEsp, plain, pl, mismatch) && vicParse(type, model, plain, pl, r);
            portENTER_CRITICAL(&mux);
            if (ok) {
                next = r;
                pending = true;
            }
            nextRssi = dev->getRSSI();
            nextStatus = !haveKey ? 1 : mismatch ? 2 : ok ? 0 : 3;
            statusPending = true;
            portEXIT_CRITICAL(&mux);
            return;
        }
    }

    void loop() override {
        bool have = false, haveStatus = false;
        VicReading r;
        uint8_t st = 0;
        int rs = 0;
        portENTER_CRITICAL(&mux);
        if (pending) { r = next; pending = false; have = true; }
        if (statusPending) { st = nextStatus; rs = nextRssi; statusPending = false; haveStatus = true; }
        portEXIT_CRITICAL(&mux);
        if (haveStatus) {
            rssi = rs;
            if (st != status) {
                status = st;
                if (st == 2) devlogf(id, "Key does not match the device");
                updates++;
            }
        }
        if (have) {
            if (r.rec == VIC_REC_BATTMON || rec == VIC_REC_BATTMON) bmsListChanges = bmsListChanges + 1;
            bool changed = !hasData || r.rec != cur.rec || differs(r, cur);
            if (r.rec != rec) {
                rec = r.rec;
                mqttRequestDiscovery();   // entities depend on the device type
            }
            cur = r;
            hasData = true;
            lastUpdate = millis();
            if (changed) updates++;
        }
        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            if (rec == VIC_REC_BATTMON) bmsListChanges = bmsListChanges + 1;
            updates++;
        }
    }

    bool available() override {
        return hasData && millis() - lastUpdate <= VICTRON_TIMEOUT_MS;
    }

    bool isBatteryMonitor() const { return rec == VIC_REC_BATTMON; }

    // Entry of <prefix>/batteries, same fields as the BMS plugins (t = 7)
    void listEntry(JsonObject o) {
        o["id"] = (const char*)id;
        o["name"] = (const char*)name;
        bool ok = available();
        o["ok"] = ok;
        o["t"] = 7;
        if (!ok) return;
        const VicReading& v = cur;
        if (!isnan(v.soc)) o["soc"] = roundf(v.soc);
        if (!isnan(v.voltage)) o["v"] = roundf(v.voltage * 100) / 100;
        if (!isnan(v.current)) o["a"] = roundf(v.current * 100) / 100;
        float w = power();
        if (!isnan(w)) o["w"] = roundf(w);
        if (!isnan(v.remaining_min) && !isnan(v.current) && v.current < 0) o["rt"] = (long)v.remaining_min;
        if (v.alarm) {
            o["p"] = true;
            o["pt"] = "Alarm des Batteriemonitors";
        }
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        if (hasData) {
            const VicReading& v = cur;
            if (rec == VIC_REC_BATTMON && !isnan(v.soc)) {
                JsonObject g = o["gauge"].to<JsonObject>();
                g["v"] = roundf(v.soc);
                g["unit"] = "%";
                g["label"] = "Ladestand";
                g["max"] = 100;
            }
            if (rec == VIC_REC_SOLAR) {
                liveRowNum(o, "Solarleistung", v.power, 0, "W");
                liveRowNum(o, "Ertrag heute", v.yield_wh, 0, "Wh");
            }
            if (rec == VIC_REC_DCDC) liveRowNum(o, "Eingang", v.input_v, 2, "V");
            liveRowNum(o, rec == VIC_REC_DCDC ? "Ausgang" : "Spannung", v.voltage, 2, "V");
            if (rec != VIC_REC_DCDC) liveRowNum(o, "Strom", v.current, 2, "A");
            if (rec == VIC_REC_BATTMON) {
                liveRowNum(o, "Leistung", power(), 0, "W");
                liveRowNum(o, "Entnommen", v.consumed_ah, 1, "Ah");
                liveRowNum(o, "Restlaufzeit", isnan(v.remaining_min) ? NAN : v.remaining_min / 60.0f, 1, "h");
                if (!isnan(v.temperature)) liveRowNum(o, "Temperatur", v.temperature, 1, "°C");
                if (!isnan(v.starter_v)) liveRowNum(o, "Starterbatterie", v.starter_v, 2, "V");
                if (!isnan(v.mid_v)) liveRowNum(o, "Mittelpunkt", v.mid_v, 2, "V");
                if (v.alarm) liveRowNum(o, "Alarm", v.alarm, 0, "");
            }
            if (rec == VIC_REC_SOLAR && !isnan(v.load_a)) liveRowNum(o, "Laststrom", v.load_a, 1, "A");
            const char* m = vicStateName(v.state);
            if (m) liveRowText(o, "Zustand", m);
            if (v.error && v.error != 0xFF) liveRowNum(o, "Fehlercode", v.error, 0, "");
            if (rec == VIC_REC_DCDC && v.off_reason) liveRowText(o, "Grund für Aus", vicOffReasonName(v.off_reason));
            liveRowNum(o, "Alter", (millis() - lastUpdate) / 1000.0f, 0, "s");
        }
        liveRowNum(o, "Bluetooth", rssi ? (float)rssi : NAN, 0, "dBm");
    }

    size_t entities(const Entity*& out) override {
        out = VICTRON_ENTITIES;
        return sizeof(VICTRON_ENTITIES) / sizeof(VICTRON_ENTITIES[0]);
    }
    bool entityActive(const Entity& e) override {
        const char* k = e.key;
        if (!strcmp(k, "state") || !strcmp(k, "ble_rssi")) return true;
        bool solar = rec == VIC_REC_SOLAR, mon = rec == VIC_REC_BATTMON, dc = rec == VIC_REC_DCDC;
        if (!strcmp(k, "voltage")) return rec != 0;
        if (!strcmp(k, "current")) return solar || mon;
        if (!strcmp(k, "power") || !strcmp(k, "soc") || !strcmp(k, "consumed") || !strcmp(k, "runtime") ||
            !strcmp(k, "alarm"))
            return mon;
        if (!strcmp(k, "temperature")) return mon && !isnan(cur.temperature);
        if (!strcmp(k, "starter")) return mon && !isnan(cur.starter_v);
        if (!strcmp(k, "midpoint")) return mon && !isnan(cur.mid_v);
        if (!strcmp(k, "pv_power") || !strcmp(k, "yield_today") || !strcmp(k, "load")) return solar;
        if (!strcmp(k, "input") || !strcmp(k, "off_reason")) return dc;
        if (!strcmp(k, "mode") || !strcmp(k, "error")) return solar || dc;
        return false;
    }
    uint32_t publishIntervalMs() const override { return 30000; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        if (rssi) out.integer("ble_rssi", rssi);
        if (!available()) return;
        const VicReading& v = cur;
        out.num("voltage", v.voltage, 2);
        if (rec == VIC_REC_SOLAR) {
            out.num("current", v.current, 1);
            out.num("pv_power", v.power, 0);
            out.num("yield_today", v.yield_wh, 0);
            out.num("load", v.load_a, 1);
        } else if (rec == VIC_REC_BATTMON) {
            out.num("current", v.current, 2);
            out.num("power", power(), 0);
            out.num("soc", v.soc, 1);
            out.num("consumed", v.consumed_ah, 1);
            if (isnan(v.remaining_min)) out.text("runtime", "None");
            else out.num("runtime", v.remaining_min, 0);
            out.num("temperature", v.temperature, 1);
            out.num("starter", v.starter_v, 2);
            out.num("midpoint", v.mid_v, 2);
            out.onoff("alarm", v.alarm != 0);
        } else if (rec == VIC_REC_DCDC) {
            out.num("input", v.input_v, 2);
            out.text("off_reason", vicOffReasonName(v.off_reason));
        }
        if (rec == VIC_REC_SOLAR || rec == VIC_REC_DCDC) {
            const char* m = vicStateName(v.state);
            if (m) out.text("mode", m);
            if (v.error != 0xFF) out.integer("error", v.error);
        }
    }

private:
    VictronCfg c;
    char lastMac[18] = "";

    portMUX_TYPE mux;
    uint8_t keyBin[16] = {0};
    bool keyOk = false;
    VicReading next;
    bool pending = false;
    int nextRssi = 0;
    uint8_t nextStatus = 0;
    bool statusPending = false;

    VicReading cur;
    uint8_t rec = 0;
    uint8_t status = 0;       // 0 ok, 1 no key, 2 wrong key, 3 type not supported
    int rssi = 0;
    bool hasData = false;
    bool lastAvail = false;
    unsigned long lastUpdate = 0;

    float power() const { return cur.voltage * cur.current; }

    static bool differs(const VicReading& a, const VicReading& b) {
        auto d = [](float x, float y, float eps) {
            if (isnan(x) != isnan(y)) return true;
            return !isnan(x) && fabsf(x - y) >= eps;
        };
        return a.state != b.state || a.error != b.error || a.alarm != b.alarm || a.off_reason != b.off_reason ||
               d(a.voltage, b.voltage, 0.02f) || d(a.current, b.current, 0.1f) || d(a.power, b.power, 1) ||
               d(a.soc, b.soc, 0.5f) || d(a.input_v, b.input_v, 0.02f) || d(a.yield_wh, b.yield_wh, 10);
    }

    const char* stateText() {
        if (c.mac[0] == '\0') return "Kein Gerät";
        if (!keyOk) return "Schlüssel fehlt";
        if (status == 2) return "Falscher Schlüssel";
        if (status == 3) return "Gerätetyp nicht unterstützt";
        if (!hasData) return "Warte auf Daten";
        return available() ? "OK" : "Keine Daten";
    }
};

Plugin* createVictron(uint8_t index) {
    return new VictronPlugin(index);
}

bool victronIsBatteryMonitor(Plugin* p) { return static_cast<VictronPlugin*>(p)->isBatteryMonitor(); }
void victronListEntry(Plugin* p, JsonObject o) { static_cast<VictronPlugin*>(p)->listEntry(o); }

#endif // PLUGIN_VICTRON
