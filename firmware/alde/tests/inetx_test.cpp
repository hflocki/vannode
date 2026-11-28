// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test for the Truma iNet X message format (inetx_proto.cpp): CBOR, frames, the Alde
// values and the writes a command turns into.
#include "inetx_proto.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static bool feq(float a, float b) { return fabsf(a - b) < 0.001f; }

// Frame from the panel with a body built by fill()
template <typename F>
static size_t frameFrom(uint8_t* out, uint16_t src, uint8_t ctrl, uint8_t sub, F fill) {
    uint8_t body[1024];
    CborOut o;
    cborBegin(o, body, sizeof(body));
    fill(o);
    if (!o.ok) return 0;
    return inetxFrame(out, 1200, 0x0502, src, ctrl, sub, 0, body, o.len);
}

int main() {
    uint8_t f[1200];

    // 1. Frame layout: write AirHeating.TgtTemp = 215 from 0x0502 to 0x0404
    {
        size_t n = inetxWriteIntFrame(f, sizeof(f), 0x0502, 0x0404, "AirHeating", "TgtTemp", 215);
        CHECK(n > 18, "write frame built (%zu)", n);
        CHECK(f[0] == 0x04 && f[1] == 0x04 && f[2] == 0x02 && f[3] == 0x05, "addresses little endian");
        CHECK((size_t)(f[4] | (f[5] << 8)) == n - 7, "size = body + 11 (%u vs %zu)", f[4] | (f[5] << 8), n - 7);
        CHECK(f[6] == 0x03 && f[16] == 0x01 && f[17] == 0, "control MBP, sub write");
        for (int k = 7; k < 16; k++) CHECK(f[k] == 0, "segmentation header zero");
        // {"tn": "AirHeating", "pn": "TgtTemp", "v": 215, "id": 0}
        static const uint8_t expect[] = {0xA4, 0x62, 't', 'n', 0x6A, 'A', 'i', 'r', 'H', 'e', 'a', 't', 'i', 'n', 'g',
                                         0x62, 'p', 'n', 0x67, 'T', 'g', 't', 'T', 'e', 'm', 'p',
                                         0x61, 'v', 0x18, 0xD7, 0x62, 'i', 'd', 0x00};
        CHECK(n - 18 == sizeof(expect) && !memcmp(f + 18, expect, sizeof(expect)), "CBOR body as expected");
    }

    // 2. Registration frame: broadcast, ctrl 1, sub 1, corr 0x42, {"pv": [5, 1]}
    {
        size_t n = inetxRegisterFrame(f, sizeof(f), INETX_APP);
        static const uint8_t body[] = {0xA1, 0x62, 'p', 'v', 0x82, 0x05, 0x01};
        CHECK(f[0] == 0xFF && f[1] == 0xFF && f[2] == 0x00 && f[3] == 0x05, "register addresses");
        CHECK(f[6] == 0x01 && f[16] == 0x01 && f[17] == 0x42, "register header");
        CHECK(n == 18 + sizeof(body) && !memcmp(f + 18, body, sizeof(body)), "register body");
    }

    // 3. Registration answer sets our address
    InetxModel m;
    inetxModelClear(m);
    {
        size_t n = frameFrom(f, 0x0101, 0x01, 0x02, [](CborOut& o) { cborMap(o, 1); cborTextOut(o, "addr"); cborInt(o, 0x0502); });
        inetxModelFeed(m, f, n);
        CHECK(m.assigned == 0x0502, "assigned 0x%04X", m.assigned);
    }

    // 4. Discovery answer from the Alde: parameters with extra fields, "tn" after "parameters"
    {
        size_t n = frameFrom(f, 0x0404, 0x03, 0x84, [](CborOut& o) {
            cborMap(o, 2);
            cborTextOut(o, "x"); cborArray(o, 2); cborInt(o, 1); cborTextOut(o, "skip");
            cborTextOut(o, "topics");
            cborArray(o, 3);
            // AirHeating: parameters first, then tn
            cborMap(o, 2);
            cborTextOut(o, "parameters");
            cborArray(o, 4);
            cborMap(o, 4); cborTextOut(o, "pn"); cborTextOut(o, "TgtTemp"); cborTextOut(o, "v"); cborInt(o, 205);
                           cborTextOut(o, "min"); cborInt(o, 50); cborTextOut(o, "max"); cborInt(o, 300);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "Temp"); cborTextOut(o, "v"); cborInt(o, 156);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "ExtTemp"); cborTextOut(o, "v"); cborInt(o, -35);
            cborMap(o, 3); cborTextOut(o, "pn"); cborTextOut(o, "Active");
                           cborTextOut(o, "values"); cborMap(o, 1); cborInt(o, 1); cborTextOut(o, "Active");
                           cborTextOut(o, "v"); cborInt(o, 2);
            cborTextOut(o, "tn"); cborTextOut(o, "AirHeating");
            // EnergySrc
            cborMap(o, 2);
            cborTextOut(o, "tn"); cborTextOut(o, "EnergySrc");
            cborTextOut(o, "parameters");
            cborArray(o, 2);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "GasLevel"); cborTextOut(o, "v"); cborInt(o, 1);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "ElectricLevel"); cborTextOut(o, "v"); cborInt(o, 2);
            // WaterHeating + error list
            cborMap(o, 2);
            cborTextOut(o, "tn"); cborTextOut(o, "WaterHeating");
            cborTextOut(o, "parameters");
            cborArray(o, 2);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "Active"); cborTextOut(o, "v"); cborInt(o, 1);
            cborMap(o, 2); cborTextOut(o, "pn"); cborTextOut(o, "BoostMode"); cborTextOut(o, "v"); cborInt(o, 0);
        });
        CHECK(n > 0, "discovery frame built");
        int ch = inetxModelFeed(m, f, n);
        CHECK(ch == 8, "8 values changed (%d)", ch);
        CHECK(feq(m.target, 20.5f) && feq(m.room, 15.6f) && feq(m.outdoor, -3.5f), "temperatures %.1f %.1f %.1f", m.target, m.room, m.outdoor);
        CHECK(m.heating == 0, "Active 2 (idle) is not heating");
        CHECK(m.gas == 1 && m.el_level == 2, "energy %.0f %.0f", m.gas, m.el_level);
        CHECK(inetxWaterMode(m) == 1, "water on, no boost");
        CHECK(m.heaterAddr == 0x0404 && m.energyAddr == 0x0404 && m.waterAddr == 0x0404, "publisher addresses");
        CHECK(inetxModelFeed(m, f, n) == 0, "same answer again changes nothing");
    }

    // 5. Info messages: boost on (bool true), half float temperature, error list, panel room mode
    {
        size_t n = frameFrom(f, 0x0404, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "tn"); cborTextOut(o, "WaterHeating");
            cborTextOut(o, "pn"); cborTextOut(o, "BoostMode"); cborTextOut(o, "v");
            o.buf[o.len++] = 0xF5;   // true
        });
        CHECK(inetxModelFeed(m, f, n) == 1 && inetxWaterMode(m) == 2, "boost");
        n = frameFrom(f, 0x0404, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "v");
            o.buf[o.len++] = 0xF9; o.buf[o.len++] = 0x59; o.buf[o.len++] = 0x14;   // half float 162.5
            cborTextOut(o, "tn"); cborTextOut(o, "AirHeating"); cborTextOut(o, "pn"); cborTextOut(o, "Temp");
        });
        inetxModelFeed(m, f, n);
        CHECK(feq(m.room, 16.25f), "half float room %.2f", m.room);
        n = frameFrom(f, 0x0404, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "tn"); cborTextOut(o, "ErrorReset"); cborTextOut(o, "pn"); cborTextOut(o, "ErrCode");
            cborTextOut(o, "v"); cborArray(o, 1); cborInt(o, 12);
        });
        inetxModelFeed(m, f, n);
        CHECK(m.error == 1, "error list not empty");
        n = frameFrom(f, 0x0101, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "tn"); cborTextOut(o, "ErrorReset"); cborTextOut(o, "pn"); cborTextOut(o, "ErrCode");
            cborTextOut(o, "v"); cborArray(o, 0);
        });
        inetxModelFeed(m, f, n);
        CHECK(m.error == 1, "panel's empty list does not clear the heater's fault");
        n = frameFrom(f, 0x0101, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "tn"); cborTextOut(o, "RoomClimate"); cborTextOut(o, "pn"); cborTextOut(o, "Mode");
            cborTextOut(o, "v"); cborInt(o, 3);
        });
        inetxModelFeed(m, f, n);
        CHECK(m.room_mode == 3 && m.panelAddr == 0x0101, "room mode heat");
        // Panel's outdoor copy must not override the heater's sensor
        n = frameFrom(f, 0x0101, 0x03, 0x00, [](CborOut& o) {
            cborMap(o, 3); cborTextOut(o, "tn"); cborTextOut(o, "Temperature"); cborTextOut(o, "pn"); cborTextOut(o, "External");
            cborTextOut(o, "v"); cborInt(o, 99);
        });
        inetxModelFeed(m, f, n);
        CHECK(feq(m.outdoor, -3.5f), "outdoor stays the heater's (%.1f)", m.outdoor);
    }

    // 6. Broken input never crashes and changes nothing
    {
        uint8_t junk[64];
        srand(7);
        for (int round = 0; round < 2000; round++) {
            size_t n = 16 + rand() % 48;
            for (size_t k = 0; k < n; k++) junk[k] = (uint8_t)rand();
            junk[6] = 0x03; junk[16] = (rand() & 1) ? 0x00 : 0x84;
            InetxModel x;
            inetxModelClear(x);
            inetxModelFeed(x, junk, n);
        }
        size_t n = inetxWriteIntFrame(f, sizeof(f), 1, 2, "AirHeating", "TgtTemp", 100);
        InetxModel before = m;
        CHECK(inetxModelFeed(m, f, n - 3) == 0 && feq(m.target, before.target), "truncated frame ignored");
    }

    // 7. Commands: only what differs, sent to the publisher
    {
        InetxWrite w[6];
        InetxCommand c;
        inetxCommandClear(c);
        c.hasTarget = true; c.target = 20.5f;           // already set
        c.gas = 1;                                       // already on
        c.el_level = 3;
        CHECK(inetxCommandWrites(m, c, w, 6) == 1 && w[0].dst == 0x0404 && !strcmp(w[0].param, "ElectricLevel") && w[0].value == 3,
              "only the heating element step");
        inetxCommandClear(c);
        c.hasTarget = true; c.target = 22.0f;
        int k = inetxCommandWrites(m, c, w, 6);
        CHECK(k == 1 && !strcmp(w[0].topic, "AirHeating") && w[0].value == 220, "setpoint in tenths");
        inetxCommandClear(c);
        c.water = 1;                                     // boost -> on: only BoostMode 0
        k = inetxCommandWrites(m, c, w, 6);
        CHECK(k == 1 && !strcmp(w[0].param, "BoostMode") && w[0].value == 0, "boost off (%d)", k);
        c.water = 0;                                     // boost -> off: Active 0 and BoostMode 0
        k = inetxCommandWrites(m, c, w, 6);
        CHECK(k == 2 && !strcmp(w[0].param, "Active") && w[0].value == 0, "water off (%d)", k);
        inetxCommandClear(c);
        c.power = 0;
        k = inetxCommandWrites(m, c, w, 6);
        CHECK(k == 1 && w[0].dst == INETX_PANEL && !strcmp(w[0].topic, "RoomClimate") && w[0].value == 0, "heating off on the panel");
        c.power = 1;
        CHECK(inetxCommandWrites(m, c, w, 6) == 0, "already heating");
        InetxModel empty;
        inetxModelClear(empty);
        inetxCommandClear(c);
        c.gas = 0;
        k = inetxCommandWrites(empty, c, w, 6);
        CHECK(k == 1 && w[0].dst == INETX_HEATER, "unknown publisher -> default heater address");
    }

    if (fails) { printf("%d INETX TEST(S) FAILED\n", fails); return 1; }
    printf("ALL INETX TESTS PASSED\n");
    return 0;
}
