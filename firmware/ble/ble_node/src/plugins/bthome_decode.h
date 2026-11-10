// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef CT_BTHOME_DECODE_H
#define CT_BTHOME_DECODE_H

// Pure decoding of BTHome v2 advertisements (no Arduino types, so it can be tested on a
// PC: firmware/ble/tests/bthome_test.cpp). Format: https://bthome.io/format/
//
// Service data of UUID 0xFCD2: [device info] then objects [object id][value] ...
//   device info bit 0 = encrypted, bit 2 = sent on trigger, bits 5-7 = version (2)
// Every object id has a fixed length (except text / raw with a length byte), so the
// packet can only be parsed up to the first unknown id.

#include <stddef.h>
#include <stdint.h>

enum BthKind : uint8_t {
    BTH_SENSOR,   // number
    BTH_BINARY,   // 0 / 1
    BTH_EVENT,    // button / dimmer event (text)
    BTH_SKIP,     // known length, not shown (packet id, firmware, text, raw ...)
};

struct BthObject {
    uint8_t id;
    uint8_t len;           // bytes; 0 = length byte follows (text, raw)
    bool sign;
    float factor;
    uint8_t dec;           // decimals for MQTT / web UI
    BthKind kind;
    const char* key;       // MQTT / Home Assistant key
    const char* name;      // German name (web UI, Home Assistant)
    const char* unit;
    const char* devClass;  // Home Assistant device class (sensor or binary_sensor)
};

// clang-format off
static const BthObject BTH_OBJECTS[] = {
    {0x00, 1, false, 1,     0, BTH_SKIP,   "packet",        "Paket-Nr.",          nullptr, nullptr},
    {0x01, 1, false, 1,     0, BTH_SENSOR, "battery",       "Batterie",           "%",     "battery"},
    {0x02, 2, true,  0.01f, 1, BTH_SENSOR, "temperature",   "Temperatur",         "°C",    "temperature"},
    {0x03, 2, false, 0.01f, 1, BTH_SENSOR, "humidity",      "Luftfeuchte",        "%",     "humidity"},
    {0x04, 3, false, 0.01f, 1, BTH_SENSOR, "pressure",      "Luftdruck",          "hPa",   "pressure"},
    {0x05, 3, false, 0.01f, 0, BTH_SENSOR, "illuminance",   "Helligkeit",         "lx",    "illuminance"},
    {0x06, 2, false, 0.01f, 2, BTH_SENSOR, "mass",          "Gewicht",            "kg",    "weight"},
    {0x07, 2, false, 0.01f, 2, BTH_SENSOR, "mass_lb",       "Gewicht",            "lb",    "weight"},
    {0x08, 2, true,  0.01f, 1, BTH_SENSOR, "dewpoint",      "Taupunkt",           "°C",    "temperature"},
    {0x09, 1, false, 1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x0A, 3, false, 0.001f,3, BTH_SENSOR, "energy",        "Energie",            "kWh",   "energy"},
    {0x0B, 3, false, 0.01f, 1, BTH_SENSOR, "power",         "Leistung",           "W",     "power"},
    {0x0C, 2, false, 0.001f,2, BTH_SENSOR, "voltage",       "Spannung",           "V",     "voltage"},
    {0x0D, 2, false, 1,     0, BTH_SENSOR, "pm25",          "Feinstaub PM2.5",    "µg/m³", "pm25"},
    {0x0E, 2, false, 1,     0, BTH_SENSOR, "pm10",          "Feinstaub PM10",     "µg/m³", "pm10"},
    {0x0F, 1, false, 1,     0, BTH_BINARY, "generic",       "Schalter",           nullptr, nullptr},
    {0x10, 1, false, 1,     0, BTH_BINARY, "power_on",      "Strom an",           nullptr, "power"},
    {0x11, 1, false, 1,     0, BTH_BINARY, "opening",       "Geöffnet",           nullptr, "opening"},
    {0x12, 2, false, 1,     0, BTH_SENSOR, "co2",           "CO2",                "ppm",   "carbon_dioxide"},
    {0x13, 2, false, 1,     0, BTH_SENSOR, "tvoc",          "TVOC",               "µg/m³", "volatile_organic_compounds"},
    {0x14, 2, false, 0.01f, 1, BTH_SENSOR, "moisture",      "Feuchtigkeit",       "%",     "moisture"},
    {0x15, 1, false, 1,     0, BTH_BINARY, "battery_low",   "Batterie schwach",   nullptr, "battery"},
    {0x16, 1, false, 1,     0, BTH_BINARY, "charging",      "Lädt",               nullptr, "battery_charging"},
    {0x17, 1, false, 1,     0, BTH_BINARY, "co",            "Kohlenmonoxid",      nullptr, "carbon_monoxide"},
    {0x18, 1, false, 1,     0, BTH_BINARY, "cold",          "Kalt",               nullptr, "cold"},
    {0x19, 1, false, 1,     0, BTH_BINARY, "connectivity",  "Verbunden",          nullptr, "connectivity"},
    {0x1A, 1, false, 1,     0, BTH_BINARY, "door",          "Tür",                nullptr, "door"},
    {0x1B, 1, false, 1,     0, BTH_BINARY, "garage_door",   "Tor",                nullptr, "garage_door"},
    {0x1C, 1, false, 1,     0, BTH_BINARY, "gas",           "Gas",                nullptr, "gas"},
    {0x1D, 1, false, 1,     0, BTH_BINARY, "heat",          "Hitze",              nullptr, "heat"},
    {0x1E, 1, false, 1,     0, BTH_BINARY, "light",         "Licht",              nullptr, "light"},
    {0x1F, 1, false, 1,     0, BTH_BINARY, "lock",          "Entriegelt",         nullptr, "lock"},
    {0x20, 1, false, 1,     0, BTH_BINARY, "wet",           "Nässe",              nullptr, "moisture"},
    {0x21, 1, false, 1,     0, BTH_BINARY, "motion",        "Bewegung",           nullptr, "motion"},
    {0x22, 1, false, 1,     0, BTH_BINARY, "moving",        "In Bewegung",        nullptr, "moving"},
    {0x23, 1, false, 1,     0, BTH_BINARY, "occupancy",     "Belegt",             nullptr, "occupancy"},
    {0x24, 1, false, 1,     0, BTH_BINARY, "plug",          "Eingesteckt",        nullptr, "plug"},
    {0x25, 1, false, 1,     0, BTH_BINARY, "presence",      "Anwesenheit",        nullptr, "presence"},
    {0x26, 1, false, 1,     0, BTH_BINARY, "problem",       "Problem",            nullptr, "problem"},
    {0x27, 1, false, 1,     0, BTH_BINARY, "running",       "Läuft",              nullptr, "running"},
    {0x28, 1, false, 1,     0, BTH_BINARY, "safety",        "Unsicher",           nullptr, "safety"},
    {0x29, 1, false, 1,     0, BTH_BINARY, "smoke",         "Rauch",              nullptr, "smoke"},
    {0x2A, 1, false, 1,     0, BTH_BINARY, "sound",         "Geräusch",           nullptr, "sound"},
    {0x2B, 1, false, 1,     0, BTH_BINARY, "tamper",        "Manipulation",       nullptr, "tamper"},
    {0x2C, 1, false, 1,     0, BTH_BINARY, "vibration",     "Vibration",          nullptr, "vibration"},
    {0x2D, 1, false, 1,     0, BTH_BINARY, "window",        "Fenster",            nullptr, "window"},
    {0x2E, 1, false, 1,     0, BTH_SENSOR, "humidity",      "Luftfeuchte",        "%",     "humidity"},
    {0x2F, 1, false, 1,     0, BTH_SENSOR, "moisture",      "Feuchtigkeit",       "%",     "moisture"},
    {0x3A, 1, false, 1,     0, BTH_EVENT,  "button",        "Taster",             nullptr, nullptr},
    {0x3C, 2, false, 1,     0, BTH_EVENT,  "dimmer",        "Drehregler",         nullptr, nullptr},
    {0x3D, 2, false, 1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x3E, 4, false, 1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x3F, 2, true,  0.1f,  1, BTH_SENSOR, "rotation",      "Drehung",            "°",     nullptr},
    {0x40, 2, false, 1,     0, BTH_SENSOR, "distance",      "Abstand",            "mm",    "distance"},
    {0x41, 2, false, 0.1f,  1, BTH_SENSOR, "distance_m",    "Abstand",            "m",     "distance"},
    {0x42, 3, false, 0.001f,1, BTH_SENSOR, "duration",      "Dauer",              "s",     "duration"},
    {0x43, 2, false, 0.001f,2, BTH_SENSOR, "current",       "Strom",              "A",     "current"},
    {0x44, 2, false, 0.01f, 2, BTH_SENSOR, "speed",         "Geschwindigkeit",    "m/s",   "speed"},
    {0x45, 2, true,  0.1f,  1, BTH_SENSOR, "temperature",   "Temperatur",         "°C",    "temperature"},
    {0x46, 1, false, 0.1f,  1, BTH_SENSOR, "uv",            "UV-Index",           nullptr, nullptr},
    {0x47, 2, false, 0.1f,  1, BTH_SENSOR, "volume",        "Volumen",            "L",     "volume"},
    {0x48, 2, false, 1,     0, BTH_SENSOR, "volume_ml",     "Volumen",            "mL",    "volume"},
    {0x49, 2, false, 0.001f,3, BTH_SENSOR, "flow",          "Durchfluss",         "m³/h",  "volume_flow_rate"},
    {0x4A, 2, false, 0.1f,  1, BTH_SENSOR, "voltage",       "Spannung",           "V",     "voltage"},
    {0x4B, 3, false, 0.001f,3, BTH_SENSOR, "gas_m3",        "Gas",                "m³",    "gas"},
    {0x4C, 4, false, 0.001f,3, BTH_SENSOR, "gas_m3",        "Gas",                "m³",    "gas"},
    {0x4D, 4, false, 0.001f,3, BTH_SENSOR, "energy",        "Energie",            "kWh",   "energy"},
    {0x4E, 4, false, 0.001f,1, BTH_SENSOR, "volume",        "Volumen",            "L",     "volume"},
    {0x4F, 4, false, 0.001f,1, BTH_SENSOR, "water",         "Wasser",             "L",     "water"},
    {0x50, 4, false, 1,     0, BTH_SKIP,   "timestamp",     "Zeitstempel",        nullptr, nullptr},
    {0x51, 2, false, 0.001f,2, BTH_SENSOR, "acceleration",  "Beschleunigung",     "m/s²",  nullptr},
    {0x52, 2, false, 0.001f,2, BTH_SENSOR, "gyroscope",     "Drehrate",           "°/s",   nullptr},
    {0x53, 0, false, 1,     0, BTH_SKIP,   "text",          "Text",               nullptr, nullptr},
    {0x54, 0, false, 1,     0, BTH_SKIP,   "raw",           "Rohdaten",           nullptr, nullptr},
    {0x55, 4, false, 0.001f,1, BTH_SENSOR, "volume_storage","Füllmenge",          "L",     "volume_storage"},
    {0x56, 2, false, 1,     0, BTH_SENSOR, "conductivity",  "Leitfähigkeit",      "µS/cm", "conductivity"},
    {0x57, 1, true,  1,     0, BTH_SENSOR, "temperature",   "Temperatur",         "°C",    "temperature"},
    {0x58, 1, true,  0.35f, 1, BTH_SENSOR, "temperature",   "Temperatur",         "°C",    "temperature"},
    {0x59, 1, true,  1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x5A, 2, true,  1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x5B, 4, true,  1,     0, BTH_SENSOR, "count",         "Zähler",             nullptr, nullptr},
    {0x5C, 4, true,  0.01f, 1, BTH_SENSOR, "power",         "Leistung",           "W",     "power"},
    {0x5D, 2, true,  0.001f,2, BTH_SENSOR, "current",       "Strom",              "A",     "current"},
    {0x5E, 2, false, 0.01f, 0, BTH_SENSOR, "direction",     "Richtung",           "°",     nullptr},
    {0x5F, 2, false, 0.1f,  1, BTH_SENSOR, "precipitation", "Niederschlag",       "mm",    "precipitation"},
    {0x60, 1, false, 1,     0, BTH_SENSOR, "channel",       "Kanal",              nullptr, nullptr},
    {0xF0, 2, false, 1,     0, BTH_SKIP,   "device_type",   "Gerätetyp",          nullptr, nullptr},
    {0xF1, 4, false, 1,     0, BTH_SKIP,   "firmware",      "Firmware",           nullptr, nullptr},
    {0xF2, 3, false, 1,     0, BTH_SKIP,   "firmware",      "Firmware",           nullptr, nullptr},
};
// clang-format on
#define BTH_OBJECT_COUNT (sizeof(BTH_OBJECTS) / sizeof(BTH_OBJECTS[0]))

static inline const BthObject* bthObject(uint8_t id) {
    for (size_t i = 0; i < BTH_OBJECT_COUNT; i++)
        if (BTH_OBJECTS[i].id == id) return &BTH_OBJECTS[i];
    return nullptr;
}

// Button event 0x3A / dimmer 0x3C as text ("" = no event)
static inline const char* bthEventText(uint8_t id, float v) {
    if (id == 0x3C) {
        int dir = (int)v & 0xFF;
        return dir == 1 ? "rotate_left" : dir == 2 ? "rotate_right" : "";
    }
    switch ((int)v) {
        case 0x01: return "press";
        case 0x02: return "double_press";
        case 0x03: return "triple_press";
        case 0x04: return "long_press";
        case 0x05: return "long_double_press";
        case 0x06: return "long_triple_press";
        case 0x80: return "hold_press";
        default: return "";
    }
}

struct BthValue {
    uint8_t id;
    float v;
};

enum BthResult : uint8_t {
    BTH_OK,
    BTH_TOO_SHORT,
    BTH_ENCRYPTED,
    BTH_WRONG_VERSION,
    BTH_UNKNOWN_OBJECT,   // values before it are still in out
};

// Parses the service data payload (without the UUID). Shown objects (not BTH_SKIP) go to
// out in packet order, at most max; n = how many.
static inline BthResult bthParse(const uint8_t* d, size_t len, BthValue* out, size_t max, size_t& n) {
    n = 0;
    if (len < 1) return BTH_TOO_SHORT;
    if (d[0] & 0x01) return BTH_ENCRYPTED;
    if (((d[0] >> 5) & 0x07) != 2) return BTH_WRONG_VERSION;
    size_t i = 1;
    while (i < len) {
        const BthObject* o = bthObject(d[i]);
        if (!o) return BTH_UNKNOWN_OBJECT;
        size_t l = o->len;
        size_t start = i + 1;
        if (l == 0) {   // text / raw: length byte
            if (start >= len) return BTH_TOO_SHORT;
            l = d[start];
            start++;
        }
        if (start + l > len) return BTH_TOO_SHORT;
        if (o->kind != BTH_SKIP && n < max && l <= 4) {
            uint32_t raw = 0;
            for (size_t k = 0; k < l; k++) raw |= (uint32_t)d[start + k] << (8 * k);   // little endian
            double x;
            if (o->sign) {
                uint32_t bit = 1UL << (8 * l - 1);
                x = (raw & bit) ? (double)raw - (double)(bit * 2.0) : (double)raw;
            } else {
                x = raw;
            }
            out[n].id = o->id;
            out[n].v = (float)(x * o->factor);
            n++;
        }
        i = start + l;
    }
    return BTH_OK;
}

#endif // CT_BTHOME_DECODE_H
