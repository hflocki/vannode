// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
// Portions: record layouts from github.com/keshavdv/victron-ble (Unlicense, see NOTICE.md)

#ifndef VICTRON_DECODE_H
#define VICTRON_DECODE_H

// Pure decoding of Victron "Instant Readout" advertisements. No Arduino types; the AES block
// function is passed in (mbedtls on the ESP32, a reference implementation in the host test
// firmware/ble/tests/victron_test.cpp).
//
// Manufacturer data, company 0x02E1, after the company id:
//   [0] 0x10 (product advertisement), [1] unused, [2..3] model id (LE), [4] record type,
//   [5..6] counter / IV (LE), [7] first byte of the encryption key (key check), [8..] data
// The data is AES-128-CTR encrypted with the per-device key from the VictronConnect app
// (product info -> "Instant readout details"); the 128-bit counter starts at the IV, little
// endian. The decrypted record is a bit field, read from the least significant bit up.

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define VIC_COMPANY        0x02E1
#define VIC_REC_SOLAR      0x01   // solar charger (SmartSolar, BlueSolar with dongle)
#define VIC_REC_BATTMON    0x02   // battery monitor (SmartShunt, BMV)
#define VIC_REC_DCDC       0x04   // DC-DC charger (Orion Smart)
#define VIC_MAX_DATA       24

// AES-128 block encryption of one 16-byte block (for CTR the same as decryption)
typedef bool (*VicAesBlock)(const uint8_t key[16], const uint8_t in[16], uint8_t out[16]);

struct VicReading {
    uint8_t rec;           // record type, 0 = none
    uint16_t model;        // model id
    uint8_t state;         // operation mode (solar, DC-DC), 0xFF = unknown
    uint8_t error;         // charger error, 0xFF = unknown
    float voltage;         // battery voltage (solar, monitor), output voltage (DC-DC)
    float current;         // A (solar: charging current, monitor: + charging / - discharging)
    float power;           // W: solar power (solar)
    float yield_wh;        // Wh today (solar)
    float load_a;          // A on the load output (solar)
    float soc;             // % (monitor)
    float consumed_ah;     // Ah taken from the battery, negative (monitor)
    float remaining_min;   // min (monitor)
    float temperature;     // °C from the aux input (monitor)
    float starter_v;       // V starter battery on the aux input (monitor)
    float mid_v;           // V midpoint voltage on the aux input (monitor)
    uint16_t alarm;        // alarm bits (monitor)
    float input_v;         // V input (DC-DC)
    uint32_t off_reason;   // DC-DC, 0 = none
};

static inline void vicClear(VicReading& r) {
    memset(&r, 0, sizeof(r));
    r.state = r.error = 0xFF;
    r.voltage = r.current = r.power = r.yield_wh = r.load_a = NAN;
    r.soc = r.consumed_ah = r.remaining_min = r.temperature = r.starter_v = r.mid_v = NAN;
    r.input_v = NAN;
}

// Bits read from the least significant bit up, as Victron packs them
struct VicBits {
    const uint8_t* d;
    size_t n;
    size_t pos;
    uint32_t u(int bits) {
        uint32_t v = 0;
        for (int i = 0; i < bits; i++, pos++) {
            size_t byte = pos >> 3;
            uint32_t bit = byte < n ? (d[byte] >> (pos & 7)) & 1 : 0;
            v |= bit << i;
        }
        return v;
    }
    int32_t s(int bits) {
        uint32_t v = u(bits);
        if (v & (1u << (bits - 1))) return (int32_t)v - (int32_t)(1u << bits);
        return (int32_t)v;
    }
};

// Parses a 32-character hex key. false = invalid.
static inline bool vicParseKey(const char* hex, uint8_t key[16]) {
    if (!hex || strlen(hex) != 32) return false;
    for (int i = 0; i < 16; i++) {
        uint8_t v = 0;
        for (int k = 0; k < 2; k++) {
            char ch = hex[i * 2 + k];
            v <<= 4;
            if (ch >= '0' && ch <= '9') v |= ch - '0';
            else if (ch >= 'a' && ch <= 'f') v |= ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') v |= ch - 'A' + 10;
            else return false;
        }
        key[i] = v;
    }
    return true;
}

// Header of a manufacturer data payload (after the company id). false = not Instant Readout.
static inline bool vicHeader(const uint8_t* m, size_t n, uint16_t& model, uint8_t& rec) {
    if (n < 9 || m[0] != 0x10) return false;
    model = (uint16_t)(m[2] | (m[3] << 8));
    rec = m[4];
    return true;
}

// Decrypts the payload. keyMismatch = the key check byte does not fit (wrong key).
static inline bool vicDecrypt(const uint8_t* m, size_t n, const uint8_t key[16], VicAesBlock aes,
                              uint8_t* out, size_t& outLen, bool& keyMismatch) {
    keyMismatch = false;
    if (n < 9 || m[0] != 0x10) return false;
    if (m[7] != key[0]) { keyMismatch = true; return false; }
    size_t len = n - 8;
    if (len > VIC_MAX_DATA) len = VIC_MAX_DATA;
    uint8_t ctr[16] = {0};
    ctr[0] = m[5];
    ctr[1] = m[6];
    for (size_t off = 0; off < len; off += 16) {
        uint8_t ks[16];
        if (!aes(key, ctr, ks)) return false;
        for (size_t i = 0; i < 16 && off + i < len; i++) out[off + i] = m[8 + off + i] ^ ks[i];
        for (int i = 0; i < 16 && ++ctr[i] == 0; i++) {}   // little endian increment
    }
    outLen = len;
    return true;
}

// Decodes a decrypted record of a known type. false = type not supported.
static inline bool vicParse(uint8_t rec, uint16_t model, const uint8_t* d, size_t n, VicReading& r) {
    vicClear(r);
    VicBits b = {d, n, 0};
    r.rec = rec;
    r.model = model;
    if (rec == VIC_REC_SOLAR) {
        r.state = (uint8_t)b.u(8);
        r.error = (uint8_t)b.u(8);
        int32_t v = b.s(16), a = b.s(16);
        uint32_t y = b.u(16), p = b.u(16), l = b.u(9);
        if (v != 0x7FFF) r.voltage = v / 100.0f;
        if (a != 0x7FFF) r.current = a / 10.0f;
        if (y != 0xFFFF) r.yield_wh = y * 10.0f;
        if (p != 0xFFFF) r.power = (float)p;
        if (l != 0x1FF) r.load_a = l / 10.0f;
        return true;
    }
    if (rec == VIC_REC_BATTMON) {
        uint32_t rem = b.u(16);
        int32_t v = b.s(16);
        uint32_t alarm = b.u(16), aux = b.u(16), auxMode = b.u(2);
        int32_t cur = b.s(22);
        uint32_t used = b.u(20), soc = b.u(10);
        if (rem != 0xFFFF) r.remaining_min = (float)rem;
        if (v != 0x7FFF) r.voltage = v / 100.0f;
        r.alarm = (uint16_t)alarm;
        if (cur != 0x1FFFFF && cur != -1) r.current = cur / 1000.0f;   // 0x3FFFFF = not available
        if (used != 0xFFFFF) r.consumed_ah = -(float)used / 10.0f;
        if (soc != 0x3FF) r.soc = soc / 10.0f;
        if (auxMode == 0) r.starter_v = (int16_t)aux / 100.0f;
        else if (auxMode == 1) r.mid_v = aux / 100.0f;
        else if (auxMode == 2) r.temperature = aux / 100.0f - 273.15f;
        return true;
    }
    if (rec == VIC_REC_DCDC) {
        r.state = (uint8_t)b.u(8);
        r.error = (uint8_t)b.u(8);
        uint32_t in = b.u(16);
        int32_t out = b.s(16);
        r.off_reason = b.u(32);
        if (in != 0xFFFF) r.input_v = in / 100.0f;
        if (out != 0x7FFF) r.voltage = out / 100.0f;
        return true;
    }
    return false;
}

// German name of the operation mode
static inline const char* vicStateName(uint8_t s) {
    switch (s) {
        case 0:   return "Aus";
        case 1:   return "Sparmodus";
        case 2:   return "Fehler";
        case 3:   return "Bulk";
        case 4:   return "Absorption";
        case 5:   return "Erhaltung";
        case 6:   return "Lagerung";
        case 7:   return "Ausgleich";
        case 9:   return "Wechselrichter";
        case 11:  return "Netzteil";
        case 245: return "Startet";
        case 246: return "Absorption (wiederholt)";
        case 247: return "Auffrischen";
        case 248: return "BatterySafe";
        case 249: return "Aktiv";
        case 252: return "Externe Steuerung";
        default:  return nullptr;
    }
}

// German text of the DC-DC "off reason" (most important bits only)
static inline const char* vicOffReasonName(uint32_t r) {
    if (r == 0) return "";
    if (r & 0x80) return "Motor aus";
    if (r & 0x01) return "Keine Eingangsspannung";
    if (r & 0x02) return "Am Schalter aus";
    if (r & 0x04) return "Per Einstellung aus";
    if (r & 0x08) return "Fernsteuereingang";
    if (r & 0x10) return "Schutz aktiv";
    if (r & 0x40) return "BMS";
    if (r & 0x100) return "Prüft Eingangsspannung";
    return "Aus";
}

#endif // VICTRON_DECODE_H
