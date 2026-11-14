// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Host test for alde_ci.cpp (external panel bus). Reference values: the example frame in
// PROTOCOL.md of daviddootson/alde_3030_plus.
#include "alde_ci.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static std::vector<uint8_t> infoBytes(const uint8_t d[8], bool echo = true, bool badCs = false) {
    std::vector<uint8_t> v;
    if (echo) { v.push_back(0x00); }                    // echo of the break byte
    v.push_back(0x55); v.push_back(ALDE_CI_ID_INFO);    // echo of our header
    for (int i = 0; i < 8; i++) v.push_back(d[i]);
    uint8_t cs = aldeCiChecksum(ALDE_CI_ID_INFO, d, 8);
    v.push_back(badCs ? (uint8_t)(cs + 1) : cs);
    return v;
}

int main() {
    // 1. Registration payload for the Compact 3020 (function id 1)
    {
        uint8_t d[8];
        aldeCiDiagPayload(1, d);
        const uint8_t exp[8] = {0x10, 0x06, 0xB2, 0x00, 0xDE, 0x41, 0x01, 0x00};
        CHECK(memcmp(d, exp, 8) == 0, "diag payload");
    }
    // 2. Info frame: 21.0 C room, zone 2 unused, 5.0 C outside, 20 C / gas on / valve open,
    //    2 kW, panel on + water on + 230 V + pump
    const uint8_t info[8] = {126, 0xFE, 94, 0xDE, 0x9E, 0x01 | 0x08 | 0x20 | 0x80, 0x00, 0x00};
    AldeCiState s;
    {
        std::vector<uint8_t> b = infoBytes(info);
        CHECK(aldeCiParseInfo(b.data(), (int)b.size(), &s), "parse failed");
        CHECK(s.valid, "not valid");
        CHECK(std::fabs(s.room - 21.0f) < 0.01f, "room %.2f", s.room);
        CHECK(std::fabs(s.outdoor - 5.0f) < 0.01f, "outdoor %.2f", s.outdoor);
        CHECK(std::fabs(s.setpoint - 20.0f) < 0.01f, "setpoint %.2f", s.setpoint);
        CHECK(s.gas == 1 && s.valve == 1 && s.el_level == 2 && s.water == 1, "flags gas=%u valve=%u el=%u water=%u", s.gas, s.valve, s.el_level, s.water);
        CHECK(s.panel_on == 1 && s.ac_input == 1 && s.pump == 1 && s.error == 0, "status bits");
    }
    // 3. No command = the panel's own state again; equals the documented example frame
    {
        AldeCiCommand c; aldeCiCommandClear(c);
        uint8_t d[8];
        aldeCiBuildControl(s, c, d);
        const uint8_t exp[8] = {0x00, 0x00, 0x00, 0xDE, 0x9E, 0xA9, 0xFF, 0xFF};
        CHECK(memcmp(d, exp, 8) == 0, "control %02X %02X %02X", d[3], d[4], d[5]);
    }
    // 4. Change only the setpoint: 22.5 C -> raw 35; gas, valve, 2 kW, status bits unchanged
    {
        AldeCiCommand c; aldeCiCommandClear(c);
        c.hasTarget = true; c.target = 22.5f;
        uint8_t d[8];
        aldeCiBuildControl(s, c, d);
        CHECK(d[3] == (35 | 0x40 | 0x80) && d[4] == 0x9E && d[5] == 0xA9, "target %02X %02X %02X", d[3], d[4], d[5]);
    }
    // 5. Gas off, heating element 0, water boost; zone 2 bits (0x1E) and other b5 bits stay
    {
        AldeCiCommand c; aldeCiCommandClear(c);
        c.gas = 0; c.el_level = 0; c.water = 2;
        uint8_t d[8];
        aldeCiBuildControl(s, c, d);
        CHECK(d[3] == (0x1E | 0x80), "gas off b3=%02X", d[3]);
        CHECK(d[4] == 0x1E, "el off b4=%02X", d[4]);
        CHECK(d[5] == ((0xA9 & 0xE7) | (2 << 3)), "boost b5=%02X", d[5]);
    }
    // 6. Setpoint is clamped to 5..30
    {
        AldeCiCommand c; aldeCiCommandClear(c);
        c.hasTarget = true; c.target = 40.0f;
        uint8_t d[8];
        aldeCiBuildControl(s, c, d);
        CHECK((d[3] & 0x3F) == 50, "clamp %u", d[3] & 0x3F);
    }
    // 7. Bad checksum, missing echo, special temperatures
    {
        AldeCiState t;
        std::vector<uint8_t> b = infoBytes(info, true, true);
        CHECK(!aldeCiParseInfo(b.data(), (int)b.size(), &t), "bad checksum accepted");
        std::vector<uint8_t> e = infoBytes(info, false);
        CHECK(aldeCiParseInfo(e.data(), (int)e.size(), &t), "without break echo");
        const uint8_t noSensor[8] = {0xFD, 0xFE, 0xFF, 0x1E, 0x00, 0x00, 0, 0};
        std::vector<uint8_t> n = infoBytes(noSensor);
        CHECK(aldeCiParseInfo(n.data(), (int)n.size(), &t) && std::isnan(t.room) && std::isnan(t.outdoor), "special values");
        CHECK(!aldeCiParseInfo(n.data(), 5, &t), "short buffer accepted");
    }
    if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
    printf("ALL CI TESTS PASSED\n");
    return 0;
}
