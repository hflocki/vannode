// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// Portions: advertisement layout and status codes from github.com/ksanislo/senso4s_ble
// (Apache-2.0, Copyright 2026 Ken Sanislo) - see firmware/ble/LICENSE-Apache-2.0 and NOTICE.md.
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#ifndef SENSO4S_DECODE_H
#define SENSO4S_DECODE_H

// Pure decoding of the Senso4s gas bottle scale (Basic / Plus) advertisement. No Arduino types,
// tested on a PC (firmware/ble/tests/senso4s_test.cpp).
//
// Manufacturer data, company 0x09CC (Senso4s) or 0x0059 (Nordic, older firmware), 12 bytes:
//   [0] high nibble: 8 = Basic, below 8 = Plus with warning bits (1 temperature, 2 incline,
//       4 motion); low nibble: usage mode 1..5
//   [1] gas level 0..100 %, else a status code (0xFF not ready, 0xFC setup error,
//       0xFE batteries empty, 0xF1..0xF7 measurement disturbed)
//   [4] battery in %
//   [6..11] MAC address of the scale (either byte order)

#include <stddef.h>
#include <stdint.h>

#define S4S_COMPANY        0x09CC
#define S4S_COMPANY_NORDIC 0x0059

struct S4sReading {
    bool plus;          // Plus model
    int8_t level;       // 0..100 %, -1 = no reading (see status)
    uint8_t battery;    // %
    uint8_t status;     // raw level byte when level < 0
    uint8_t warnings;   // Plus: 1 temperature, 2 incline, 4 motion
};

// mac: the advertising address as 6 bytes (most significant first), used only for the
// Nordic company id, where other sensors (e.g. Mopeka) use the same id
static inline bool s4sDecode(uint16_t company, const uint8_t* d, size_t n, const uint8_t mac[6], S4sReading& r) {
    if (company != S4S_COMPANY && company != S4S_COMPANY_NORDIC) return false;
    if (n < 11) return false;
    uint8_t lvl = d[1];
    bool validLevel = lvl <= 100 || (lvl >= 0xF1 && lvl <= 0xF7) || lvl == 0xFC || lvl == 0xFE || lvl == 0xFF;
    if (!validLevel || d[4] > 100) return false;
    if (company == S4S_COMPANY_NORDIC) {
        // structural check: usage mode 1..5 and the scale's own MAC in bytes 6..11
        if (n != 12 || (d[0] & 0x0F) < 1 || (d[0] & 0x0F) > 5) return false;
        bool fwd = true, rev = true;
        for (int i = 0; i < 6; i++) {
            fwd = fwd && d[6 + i] == mac[i];
            rev = rev && d[6 + i] == mac[5 - i];
        }
        if (!fwd && !rev) return false;
    }
    uint8_t model = d[0] >> 4;
    r.plus = model < 0x8;
    r.warnings = r.plus ? (model & 0x07) : 0;
    r.battery = d[4];
    r.level = lvl <= 100 ? (int8_t)lvl : -1;
    r.status = lvl <= 100 ? 0 : lvl;
    if (lvl >= 0xF1 && lvl <= 0xF7) r.warnings |= (uint8_t)(lvl - 0xF0);
    return true;
}

// German text of a status code / warnings, "" = all fine
static inline const char* s4sStatusText(const S4sReading& r) {
    if (r.status == 0xFF) return "Nicht bereit (Waage nullen / Flasche aufsetzen)";
    if (r.status == 0xFC) return "Einrichtungsfehler (Gewicht oder Flaschengröße)";
    if (r.status == 0xFE) return "Batterien leer";
    if (r.warnings & 4) return "Bewegung erkannt";
    if (r.warnings & 2) return "Schräglage";
    if (r.warnings & 1) return "Temperatur außerhalb";
    return "";
}

#endif // SENSO4S_DECODE_H
