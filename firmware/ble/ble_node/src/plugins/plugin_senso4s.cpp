// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Plugin "senso4s": Senso4s gas bottle scale (Basic / Plus), passive BLE advertising.
// The scale sends the gas level in %; content from the bottle size as for the other gas plugins.
// Decoding: senso4s_decode.h.

#include "gas_base.h"

#if PLUGIN_SENSO4S

#include "../core/util.h"
#include "senso4s_decode.h"
#include <NimBLEDevice.h>

struct Senso4sCfg {
    GasCommon g;
};

static const Field SENSO4S_FIELDS[] = {
    GAS_COMMON_FIELDS(Senso4sCfg),
};

class Senso4sPlugin : public GasPlugin {
public:
    explicit Senso4sPlugin(uint8_t index) : GasPlugin("senso4s", "Senso4s", "Gas (Senso4s)", index) {}

    size_t fields(const Field*& out) override {
        out = SENSO4S_FIELDS;
        return sizeof(SENSO4S_FIELDS) / sizeof(SENSO4S_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.g.capacity = DEFAULT_CAPACITY;
        c.g.fill_stop_on = false;     // the scale already reports the usable gas
        c.g.fill_stop = DEFAULT_FILL_STOP;
    }
    const char* manufacturer() const override { return "Senso4s"; }

    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return company == S4S_COMPANY || containsIgnoreCase(name, "SENSO4S") || containsIgnoreCase(uuid, "7081");
    }

protected:
    GasCommon& common() override { return c.g; }
    const GasCommon& common() const override { return c.g; }
    bool levelIsUsable() const override { return true; }

    bool decode(const NimBLEAdvertisedDevice* dev, GasReading& r, uint8_t* raw, size_t& rawLen) override {
        uint8_t mac[6];
        const uint8_t* a = dev->getAddress().getVal();   // least significant byte first
        for (int i = 0; i < 6; i++) mac[i] = a[5 - i];
        for (uint8_t i = 0; i < dev->getManufacturerDataCount(); i++) {
            std::string m = dev->getManufacturerData(i);
            if (m.size() < 2) continue;
            uint16_t cid = (uint8_t)m[0] | ((uint8_t)m[1] << 8);
            const uint8_t* d = (const uint8_t*)m.data() + 2;
            size_t n = m.size() - 2;
            S4sReading s;
            if (!s4sDecode(cid, d, n, mac, s)) continue;
            rawLen = n > BLE_RAW_MAX ? BLE_RAW_MAX : n;
            memcpy(raw, d, rawLen);
            r.battery = s.battery;
            r.hasLevel = s.level >= 0;   // status codes keep the last level
            if (r.hasLevel) r.raw = s.level;
            return true;
        }
        return false;
    }

private:
    Senso4sCfg c;
};

Plugin* createSenso4s(uint8_t index) {
    return new Senso4sPlugin(index);
}

#endif // PLUGIN_SENSO4S
