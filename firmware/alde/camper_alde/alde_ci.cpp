// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Based on daviddootson/alde_3030_plus (MIT), see alde_ci.h.

#include "alde_ci.h"
#include <math.h>
#include <string.h>

void aldeCiCommandClear(AldeCiCommand& c) {
    c.hasTarget = false;
    c.target = 0;
    c.gas = -1;
    c.el_level = -1;
    c.water = -1;
}

bool aldeCiCommandEmpty(const AldeCiCommand& c) {
    return !c.hasTarget && c.gas < 0 && c.el_level < 0 && c.water < 0;
}

uint8_t aldeCiChecksum(uint8_t id, const uint8_t* d, int n) {
    uint16_t total = id;
    for (int i = 0; i < n; i++) total += d[i];
    while (total > 0xFF) total = (total & 0xFF) + (total >> 8);
    return (uint8_t)(~total & 0xFF);
}

float aldeCiTemp(uint8_t raw) {
    if (raw >= 0xFB) return NAN;   // below/above range, no sensor, zone unused, invalid
    return raw * 0.5f - 42.0f;
}

void aldeCiDiagPayload(uint16_t functionId, uint8_t out[8]) {
    out[0] = 0x10;                       // NAD of the panel
    out[1] = 0x06;                       // single frame, 6 bytes
    out[2] = 0xB2;                       // ReadByIdentifier
    out[3] = 0x00;                       // identifier 0 = product info
    out[4] = 0xDE;                       // supplier 0x41DE (Alde), little-endian
    out[5] = 0x41;
    out[6] = (uint8_t)(functionId & 0xFF);
    out[7] = (uint8_t)(functionId >> 8);
}

bool aldeCiParseInfo(const uint8_t* buf, int n, AldeCiState* out) {
    for (int i = 0; i + 2 + 9 <= n; i++) {
        if (buf[i] != 0x55 || buf[i + 1] != ALDE_CI_ID_INFO) continue;
        const uint8_t* d = buf + i + 2;
        if (aldeCiChecksum(ALDE_CI_ID_INFO, d, 8) != d[8]) continue;

        AldeCiState s;
        memcpy(s.raw, d, 8);
        s.room = aldeCiTemp(d[0]);
        s.outdoor = aldeCiTemp(d[2]);
        uint8_t sp = d[3] & 0x3F;
        s.setpoint = (sp <= 50) ? sp * 0.5f + 5.0f : NAN;
        s.gas = (d[3] >> 6) & 1;
        s.valve = (d[3] >> 7) & 1;
        s.el_level = (d[4] >> 6) & 3;
        uint8_t wm = (d[5] >> 3) & 3;
        s.water = (wm == 3) ? 1 : wm;
        s.panel_on = d[5] & 1;
        s.busy = (d[5] >> 1) & 1;
        s.error = (d[5] >> 2) & 1;
        s.ac_input = (d[5] >> 5) & 1;
        s.pump = (d[5] >> 7) & 1;
        s.valid = true;
        *out = s;
        return true;
    }
    return false;
}

void aldeCiBuildControl(const AldeCiState& s, const AldeCiCommand& c, uint8_t out[8]) {
    float sp = c.hasTarget ? c.target : (isnan(s.setpoint) ? 20.0f : s.setpoint);
    if (sp < 5.0f) sp = 5.0f;
    if (sp > 30.0f) sp = 30.0f;
    uint8_t spRaw = (uint8_t)lroundf((sp - 5.0f) * 2.0f);
    uint8_t gas = (c.gas >= 0) ? (uint8_t)(c.gas ? 1 : 0) : s.gas;
    uint8_t el = (c.el_level >= 0 && c.el_level <= 3) ? (uint8_t)c.el_level : s.el_level;

    out[0] = 0x00;
    out[1] = 0x00;
    out[2] = 0x00;
    out[3] = (uint8_t)(spRaw | (gas << 6) | (s.valve << 7));    // valve bit copied as reported
    out[4] = (uint8_t)((s.raw[4] & 0x3F) | (el << 6));          // zone 2 unchanged
    uint8_t b5 = s.raw[5];
    if (c.water >= 0 && c.water <= 2) b5 = (uint8_t)((b5 & 0xE7) | (c.water << 3));
    out[5] = b5;
    out[6] = 0xFF;
    out[7] = 0xFF;
}
