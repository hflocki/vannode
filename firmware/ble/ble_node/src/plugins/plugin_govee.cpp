// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "govee": Govee thermo-hygrometers (H5072, H5074, H5075, H5101, H5102, H5104,
// H5174, H5177, H5179), passive BLE advertising. Only the MAC is set; the model is
// recognized from the packet. Decoding in govee_decode.h (host test in firmware/ble/tests).

#include "../core/plugin.h"

#if PLUGIN_GOVEE

#include "../core/util.h"
#include "govee_decode.h"
#include <NimBLEDevice.h>

struct GoveeCfg {
    char mac[18];
    float temp_offset;   // calibration, added to the measured value
    float hum_offset;
};

static const Field GOVEE_FIELDS[] = {
    FLD_MAC(GoveeCfg, mac, "mac", "Sensor-MAC", "Über \"Suchen\" aus der Liste wählen (Name GVH5…)"),
    FLD_FLOAT(GoveeCfg, temp_offset, "temp_offset", "Korrektur Temperatur", "°C", -10, 10, 0.1f,
              "Wird zum Messwert addiert, z. B. -0,5"),
    FLD_FLOAT(GoveeCfg, hum_offset, "hum_offset", "Korrektur Luftfeuchte", "%", -20, 20, 0.5f, nullptr),
};

static const Entity GOVEE_ENTITIES[] = {
    {E_SENSOR, "temperature", "Temperatur", "°C", "temperature", "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "humidity", "Luftfeuchte", "%", "humidity", "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "battery", "Batterie", "%", "battery", "measurement", nullptr, EF_DIAG, nullptr},
    {E_SENSOR, "rssi", "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
};

class GoveePlugin : public Plugin {
public:
    explicit GoveePlugin(uint8_t index) : Plugin("govee", "Thermometer Govee", "Thermometer", index) {
        portMUX_INITIALIZE(&mux);
        memset(&c, 0, sizeof(c));
    }

    size_t fields(const Field*& out) override {
        out = GOVEE_FIELDS;
        return sizeof(GOVEE_FIELDS) / sizeof(GOVEE_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override { memset(&c, 0, sizeof(c)); }
    void onConfigChanged() override {
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
        }
        updates++;
    }
    const char* manufacturer() const override { return "Govee"; }
    const char* model() const override { return hasData ? cur.model : typeName; }

    uint8_t bleUse() const override { return BLE_PASSIVE; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        // 0x0001 is used by other devices too, so the H510x family is recognized by its name
        if (company == GOVEE_CID_H5075 || company == GOVEE_CID_H5179) return true;
        return name && (containsIgnoreCase(name, "GVH5") || containsIgnoreCase(name, "Govee"));
    }

    // NimBLE host task: decode (cheap, no allocation) and hand the result to loop()
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        if (c.mac[0] == '\0' || strcmp(addr, c.mac) != 0) return;
        for (uint8_t i = 0; i < dev->getManufacturerDataCount(); i++) {
            std::string m = dev->getManufacturerData(i);
            if (m.size() < 2) continue;
            int32_t cid = (uint8_t)m[0] | ((uint8_t)m[1] << 8);
            GoveeReading r;
            if (!goveeDecode(cid, (const uint8_t*)m.data() + 2, m.size() - 2, r)) continue;
            portENTER_CRITICAL(&mux);
            next = r;
            nextRssi = dev->getRSSI();
            pending = true;
            portEXIT_CRITICAL(&mux);
            return;
        }
    }

    void loop() override {
        bool have = false;
        GoveeReading r;
        int rs = 0;
        portENTER_CRITICAL(&mux);
        if (pending) {
            r = next;
            rs = nextRssi;
            pending = false;
            have = true;
        }
        portEXIT_CRITICAL(&mux);

        if (have) {
            float t = r.temperature + c.temp_offset;
            float h = constrain(r.humidity + c.hum_offset, 0.0f, 100.0f);
            // Publish right away when a shown value changes (0.1 °C, 1 %, battery)
            bool changed = !hasData || fabsf(t - temp) >= 0.05f || fabsf(h - hum) >= 0.5f || r.battery != cur.battery;
            cur = r;
            temp = t;
            hum = h;
            rssi = rs;
            hasData = true;
            lastUpdate = millis();
            if (changed) updates++;
        }

        bool avail = available();
        if (avail != lastAvail) {
            lastAvail = avail;
            updates++;
        }
    }

    bool available() override {
        return hasData && millis() - lastUpdate <= GOVEE_TIMEOUT_MS;
    }

    void live(JsonObject o) override {
        o["state"] = c.mac[0] == '\0' ? "Kein Sensor" : !hasData ? "Warte auf Daten" : available() ? "OK" : "Keine Daten";
        o["ok"] = available();
        liveRowNum(o, "Temperatur", hasData ? temp : NAN, 1, "°C");
        liveRowNum(o, "Luftfeuchte", hasData ? hum : NAN, 0, "%");
        liveRowNum(o, "Batterie", hasData ? (float)cur.battery : NAN, 0, "%");
        if (hasData) liveRowText(o, "Modell", cur.model);
        liveRowNum(o, "Bluetooth", hasData ? (float)rssi : NAN, 0, "dBm");
        liveRowNum(o, "Alter", hasData ? (millis() - lastUpdate) / 1000.0f : NAN, 0, "s");
    }

    size_t entities(const Entity*& out) override {
        out = GOVEE_ENTITIES;
        return sizeof(GOVEE_ENTITIES) / sizeof(GOVEE_ENTITIES[0]);
    }
    uint32_t publishIntervalMs() const override { return MQTT_GOVEE_MS; }

    void publish(MqttOut& out) override {
        if (!hasData) return;
        out.num("temperature", temp, 1);
        out.num("humidity", hum, 1);
        out.integer("battery", cur.battery);
        out.integer("rssi", rssi);
    }

private:
    GoveeCfg c;
    char lastMac[18] = "";

    portMUX_TYPE mux;
    GoveeReading next = {NAN, NAN, 0, ""};
    int nextRssi = 0;
    bool pending = false;

    GoveeReading cur = {NAN, NAN, 0, ""};
    float temp = NAN, hum = NAN;
    int rssi = 0;
    bool hasData = false;
    bool lastAvail = false;
    unsigned long lastUpdate = 0;
};

Plugin* createGovee(uint8_t index) {
    return new GoveePlugin(index);
}

#endif // PLUGIN_GOVEE
