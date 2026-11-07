// Plugin "blesensor": own BLE sensor without its own plugin.
// Many simple sensors (thermometers, level sensors, contacts ...) do not connect: they
// broadcast a short advertisement every few seconds with their values at fixed positions.
// Here the positions are set in the web UI, so such a sensor works without new firmware:
//   source  : manufacturer data (without the 2 company id bytes, like nRF Connect) or
//             service data of a UUID
//   value i : name, position, length (1/2/4 bytes), signed, byte order, value * factor + offset,
//             decimals, unit and kind (Home Assistant device class)
// Encrypted advertisements (e.g. Victron, some Xiaomi) can not be read this way.

#include "../core/plugin.h"

#if PLUGIN_BLESENSOR

#include "../core/util.h"
#include "blesensor_decode.h"
#include <NimBLEDevice.h>

#define BS_N BLESENSOR_VALUES

struct BsValue {
    char name[24];         // empty = value not used
    int32_t pos;           // byte position in the payload (0 = first byte)
    uint8_t len;           // 0 = 1 byte, 1 = 2 bytes, 2 = 4 bytes
    bool sign;             // signed (two's complement)
    bool big;              // big endian (most significant byte first)
    float factor;
    float add;
    int32_t dec;           // decimals for MQTT / web UI
    char unit[10];
    uint8_t kind;          // index into BS_KINDS
};

struct BsCfg {
    char mac[18];
    uint8_t source;        // 0 = manufacturer data, 1 = service data
    char company[8];       // manufacturer data: company id in hex ("FFFF"), empty = any
    char uuid[40];         // service data: UUID ("181A"), empty = first one
    BsValue v[BS_N];
};

// Kind -> Home Assistant device class
static const char* const BS_KINDS = "Allgemein|Temperatur|Luftfeuchte|Batterie|Spannung|Strom|Leistung|Druck";
static const char* const BS_CLASSES[] = {nullptr, "temperature", "humidity", "battery", "voltage", "current", "power", "pressure"};
static const char* const BS_KEYS[] = {"v1", "v2", "v3", "v4", "v5", "v6"};
static_assert(BS_N >= 1 && BS_N <= 6, "BLESENSOR_VALUES must be 1..6");

#define BS_VALUE_FIELDS(i, n) \
    FLD_TEXT(BsCfg, v[i].name, "v" #n "_name", "Wert " #n ": Name", "Leer = Wert nicht benutzt, z. B. Temperatur"), \
    FLD_INT(BsCfg, v[i].pos, "v" #n "_pos", "Wert " #n ": Stelle", nullptr, 0, 63, "Position im Paket, 0 = erstes Byte (siehe Rohdaten)"), \
    FLD_SELECT(BsCfg, v[i].len, "v" #n "_len", "Wert " #n ": Länge", "1 Byte|2 Byte|4 Byte", nullptr), \
    FLD_BOOL(BsCfg, v[i].sign, "v" #n "_signed", "Wert " #n ": mit Vorzeichen", "Für Werte, die negativ sein können (Temperatur)"), \
    FLD_BOOL(BsCfg, v[i].big, "v" #n "_big", "Wert " #n ": höchstes Byte zuerst", "Aus = niedrigstes Byte zuerst (die meisten Sensoren)"), \
    FLD_FLOAT(BsCfg, v[i].factor, "v" #n "_factor", "Wert " #n ": Faktor", nullptr, -1000000, 1000000, 0.000001f, "Wert = Rohzahl × Faktor + Offset, z. B. 0,01"), \
    FLD_FLOAT(BsCfg, v[i].add, "v" #n "_add", "Wert " #n ": Offset", nullptr, -1000000, 1000000, 0.000001f, nullptr), \
    FLD_INT(BsCfg, v[i].dec, "v" #n "_dec", "Wert " #n ": Nachkommastellen", nullptr, 0, 4, nullptr), \
    FLD_TEXT(BsCfg, v[i].unit, "v" #n "_unit", "Wert " #n ": Einheit", "z. B. °C, %, V"), \
    FLD_SELECT(BsCfg, v[i].kind, "v" #n "_kind", "Wert " #n ": Art", BS_KINDS, "Für Home Assistant (Symbol, Statistik)")

static const Field BS_FIELDS[] = {
    FLD_MAC(BsCfg, mac, "mac", "Sensor-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_SELECT(BsCfg, source, "source", "Datenquelle", "Herstellerdaten|Service-Daten", "Wo die Werte im Funkpaket stehen (nRF Connect zeigt beides)"),
    FLD_TEXT(BsCfg, company, "company", "Hersteller-ID", "Nur Herstellerdaten: z. B. FFFF, leer = beliebig. Die Rohdaten beginnen nach diesen 2 Bytes"),
    FLD_TEXT(BsCfg, uuid, "uuid", "Service-UUID", "Nur Service-Daten: z. B. 181A, leer = die erste"),
    BS_VALUE_FIELDS(0, 1),
#if BS_N >= 2
    BS_VALUE_FIELDS(1, 2),
#endif
#if BS_N >= 3
    BS_VALUE_FIELDS(2, 3),
#endif
#if BS_N >= 4
    BS_VALUE_FIELDS(3, 4),
#endif
#if BS_N >= 5
    BS_VALUE_FIELDS(4, 5),
#endif
#if BS_N >= 6
    BS_VALUE_FIELDS(5, 6),
#endif
};

class BleSensorPlugin : public Plugin {
public:
    explicit BleSensorPlugin(uint8_t index) : Plugin("blesensor", "Eigener BLE-Sensor", "BLE-Sensor", index) {
        portMUX_INITIALIZE(&mux);
        memset(&c, 0, sizeof(c));   // buildEntities() reads the settings
        buildEntities();
    }

    size_t fields(const Field*& out) override {
        out = BS_FIELDS;
        return sizeof(BS_FIELDS) / sizeof(BS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        for (uint8_t i = 0; i < BS_N; i++) {
            c.v[i].factor = 1.0f;
            c.v[i].dec = 1;
        }
    }
    void onConfigChanged() override {
        company = bsParseCompany(c.company);
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            for (uint8_t i = 0; i < BS_N; i++) val[i] = NAN;
        }
        buildEntities();
        updates++;
    }

    uint8_t bleUse() const override { return BLE_PASSIVE; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t comp, const char* uuid, const char* name) const override {
        if (c.source == 0) return company >= 0 && comp == company;
        return c.uuid[0] && containsIgnoreCase(uuid, c.uuid);
    }

    // NimBLE host task: copy the payload, decoding happens in loop()
    void onAdvertisement(const NimBLEAdvertisedDevice* dev, const char* addr) override {
        if (c.mac[0] == '\0' || strcmp(addr, c.mac) != 0) return;
        std::string payload;
        bool found = false;
        if (c.source == 0) {
            for (uint8_t i = 0; i < dev->getManufacturerDataCount(); i++) {
                std::string m = dev->getManufacturerData(i);
                if (m.size() < 2) continue;
                int32_t cid = (uint8_t)m[0] | ((uint8_t)m[1] << 8);
                if (company < 0 || cid == company) {
                    payload = m.substr(2);
                    found = true;
                    break;
                }
            }
        } else if (c.uuid[0]) {
            payload = dev->getServiceData(NimBLEUUID(std::string(c.uuid)));
            found = !payload.empty();
        } else if (dev->getServiceDataCount() > 0) {
            payload = dev->getServiceData((uint8_t)0);
            found = true;
        }
        if (!found) return;
        size_t n = payload.size() > BLE_RAW_MAX ? BLE_RAW_MAX : payload.size();
        portENTER_CRITICAL(&mux);
        memcpy(buf, payload.data(), n);
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

        if (have) {
            bool changed = !hasData;
            for (uint8_t i = 0; i < BS_N; i++) {
                const BsValue& v = c.v[i];
                float x = NAN;
                if (v.name[0] && v.pos >= 0) {
                    static const uint8_t LEN[] = {1, 2, 4};
                    if (!bsDecode(d, n, (uint32_t)v.pos, LEN[v.len % 3], v.sign, v.big, v.factor, v.add, x)) x = NAN;
                }
                if (!same(x, val[i])) changed = true;
                val[i] = x;
            }
            bsHexWithPositions(d, n, rawPos, sizeof(rawPos));
            bytesToHex(d, n, rawHex, sizeof(rawHex));
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
        return hasData && millis() - lastUpdate <= BLESENSOR_TIMEOUT_MS;
    }

    void live(JsonObject o) override {
        o["state"] = c.mac[0] == '\0' ? "Kein Sensor" : !hasData ? "Warte auf Daten" : available() ? "OK" : "Keine Daten";
        o["ok"] = available();
        bool any = false;
        for (uint8_t i = 0; i < BS_N; i++) {
            if (!c.v[i].name[0]) continue;
            any = true;
            liveRowNum(o, c.v[i].name, val[i], constrain((int)c.v[i].dec, 0, 4), c.v[i].unit);
        }
        if (!any) liveRowText(o, "Werte", "noch keine eingetragen");
        liveRowNum(o, "Bluetooth", hasData ? (float)rssi : NAN, 0, "dBm");
        liveRowNum(o, "Alter", hasData ? (millis() - lastUpdate) / 1000.0f : NAN, 0, "s");
        o["raw"] = (const char*)rawPos;
    }

    size_t entities(const Entity*& out) override {
        out = ents;
        return BS_N + 2;
    }
    bool entityActive(const Entity& e) override {
        for (uint8_t i = 0; i < BS_N; i++) {
            if (!strcmp(e.key, BS_KEYS[i])) return c.v[i].name[0] != '\0';
        }
        return true;
    }
    uint32_t publishIntervalMs() const override { return MQTT_BLESENSOR_MS; }

    void publish(MqttOut& out) override {
        if (!hasData) return;
        for (uint8_t i = 0; i < BS_N; i++) {
            if (c.v[i].name[0]) out.num(BS_KEYS[i], val[i], constrain((int)c.v[i].dec, 0, 4));
        }
        out.integer("rssi", rssi);
        out.text("raw", rawHex);
    }

private:
    BsCfg c;
    int32_t company = -1;
    char lastMac[18] = "";
    Entity ents[BS_N + 2];

    portMUX_TYPE mux;
    uint8_t buf[BLE_RAW_MAX];
    size_t bufLen = 0;
    int bufRssi = 0;
    bool pending = false;

    bool hasData = false;
    bool lastAvail = false;
    float val[BS_N];
    int rssi = 0;
    unsigned long lastUpdate = 0;
    char rawHex[BLE_RAW_MAX * 3 + 1] = "";
    char rawPos[BLE_RAW_MAX * 3 + (BLE_RAW_MAX / 4) * 6 + 8] = "";

    static bool same(float a, float b) {
        if (isnan(a) || isnan(b)) return isnan(a) && isnan(b);
        return fabsf(a - b) < 1e-6f;
    }

    // Entities depend on the settings (names, units, kinds): rebuilt after every change
    void buildEntities() {
        for (uint8_t i = 0; i < BS_N; i++) {
            uint8_t k = c.v[i].kind < sizeof(BS_CLASSES) / sizeof(BS_CLASSES[0]) ? c.v[i].kind : 0;
            ents[i] = {E_SENSOR, BS_KEYS[i], c.v[i].name[0] ? c.v[i].name : BS_KEYS[i],
                       c.v[i].unit[0] ? c.v[i].unit : nullptr, BS_CLASSES[k], "measurement", nullptr, 0, nullptr};
            val[i] = NAN;
        }
        ents[BS_N] = {E_SENSOR, "rssi", "Bluetooth Signal", "dBm", "signal_strength", "measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr};
        ents[BS_N + 1] = {E_SENSOR, "raw", "Rohdaten", nullptr, nullptr, nullptr, "mdi:code-brackets", EF_DIAG | EF_HIDDEN, nullptr};
    }
};

Plugin* createBleSensor(uint8_t index) {
    return new BleSensorPlugin(index);
}

#endif // PLUGIN_BLESENSOR
