// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Host test for alde_lin.cpp: synthetic bus traffic with the raw values that
// alde-3020he-lin verified against the panel's service menu (docs/PROTOCOL.md).
#include "alde_lin.h"
#include <cmath>
#include <cstdio>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool near(float a, float b) { return !std::isnan(a) && std::fabs(a - b) < 0.011f; }

// One frame as a plain 8N2 UART sees it: 00 55 PID data... checksum
static void frame(std::vector<uint8_t>& out, uint8_t pid, std::vector<uint8_t> d, bool badCs = false) {
    out.push_back(0x00); out.push_back(0x55); out.push_back(pid);
    bool classic = ((pid & 0x3F) == 0x3C || (pid & 0x3F) == 0x3D);
    for (uint8_t b : d) out.push_back(b);
    uint8_t cs = aldeLinChecksum(pid, d.data(), (int)d.size(), classic);
    out.push_back(badCs ? (uint8_t)(cs ^ 0x01) : cs);
}
static void header(std::vector<uint8_t>& out, uint8_t pid) { out.push_back(0x00); out.push_back(0x55); out.push_back(pid); }

// The 11-slot schedule 03 C4 85 06 47 08 D3 55 D6 97 5B with values from the docs
static void cycle(std::vector<uint8_t>& out, bool acAwake) {
    frame(out, 0x03, {0x90, 0x0B, 0, 0, 0, 0, 0, 0});              // 23.0 C
    frame(out, 0xC4, {0x9E, 0x0C, 0, 0, 0, 0, 0, 0});              // hot water 50.0 = flow
    frame(out, 0x85, {0x04, 0, 0, 0, 0, 0, 0, 0});                 // gas on
    frame(out, 0x06, {0xD0, 0x07, 0, 0, 0, 0, 0, 0});              // 2000 W permitted
    frame(out, 0x47, {0x86, 0x0B, 0, 0, 0, 0x04, 0, 0});           // room 22.0 C
    if (acAwake) frame(out, 0x08, {0x8A, 0x0B, 0, 0, 0, 0, 0, 0}); else header(out, 0x08);
    frame(out, 0xD3, {0x30, 0x0C, 0, 0, 0, 0, 0, 0});              // PCB 39.0
    frame(out, 0x55, {0xE8, 0x03, 0, 0, 0, 0, 0, 0});              // 1000 W applied
    frame(out, 0xD6, {0, 0, 0, 0, 0xA8, 0x0C, 0x1D, 0x85});        // water 51.0, battery 13.09
    frame(out, 0x97, {0xFF, 0xFF, 0x8A, 0x0B, 0, 0, 40, 0});       // AC light 40 %
    frame(out, 0x5B, {0xC6, 0x0C, 0xD1, 0x80, 0, 0, 100, 0});      // glycol 54.0, 2.09 A, fan 3200
}

int main() {
    // 1. Full cycle, fed byte by byte (worst case for the framing)
    {
        AldeLin lin;
        std::vector<uint8_t> bus;
        cycle(bus, false);
        cycle(bus, false);   // second cycle: the last frame of the first one is delimited by the next header
        for (size_t i = 0; i < bus.size(); i++) lin.feed(&bus[i], 1, 1000);
        const AldeReadings& r = lin.readings();
        CHECK(near(r.setpoint, 23.0f), "setpoint %.2f", r.setpoint);
        CHECK(near(r.room, 22.0f), "room %.2f", r.room);
        CHECK(near(r.glycol, 54.0f), "glycol %.2f", r.glycol);
        CHECK(near(r.water, 51.0f), "water %.2f", r.water);
        CHECK(near(r.pcb, 39.0f), "pcb %.2f", r.pcb);
        CHECK(near(r.hotwater_target, 50.0f), "hotwater %.2f", r.hotwater_target);
        CHECK(near(r.mains_current, 2.09f), "current %.2f", r.mains_current);
        CHECK(near(r.battery, 13.09f), "battery %.2f", r.battery);
        CHECK(near(r.el_allowed, 2000.0f), "el_allowed %.0f", r.el_allowed);
        CHECK(near(r.el_power, 1000.0f), "el_power %.0f", r.el_power);
        CHECK(near(r.burner_fan, 3200.0f), "fan %.0f", r.burner_fan);
        CHECK(near(r.gas, 1.0f), "gas %.0f", r.gas);
        CHECK(near(r.ac_light, 40.0f), "ac_light %.0f", r.ac_light);
        CHECK(lin.stats().frames_bad == 0, "bad frames %u", lin.stats().frames_bad);
        CHECK(lin.stats().headers_empty == 2, "empty headers %u", lin.stats().headers_empty);
        CHECK(lin.busOk(1000), "bus not ok");
        lin.tick(1000);
        CHECK(near(lin.readings().ac_active, 0.0f), "ac_active %.0f", lin.readings().ac_active);
        CHECK(lin.takeChanged() && !lin.takeChanged(), "changed flag");
    }
    // 2. Sentinels, unknown gas byte, bad checksum, hot water off, AC awake, bus timeout
    {
        AldeLin lin;
        std::vector<uint8_t> bus;
        cycle(bus, true);
        frame(bus, 0x47, {0xFF, 0xFF, 0, 0, 0, 0, 0, 0});         // sentinel: room keeps 22.0
        frame(bus, 0x85, {0x07, 0, 0, 0, 0, 0, 0, 0});            // unknown: gas stays on
        frame(bus, 0x03, {0x59, 0x0B, 0, 0, 0, 0, 0, 0}, true);   // 17.5 C but bad checksum: ignored
        frame(bus, 0xC4, {0xAA, 0x0A, 0, 0, 0, 0, 0, 0});         // 0.0 = hot water off, must pass
        frame(bus, 0x5B, {0xFF, 0xFF, 0xFF, 0xFF, 0, 0, 0, 0});   // sentinels; fan 0 = burner off
        header(bus, 0x03);                                        // delimiter
        lin.feed(bus.data(), bus.size(), 2000);
        const AldeReadings& r = lin.readings();
        CHECK(near(r.room, 22.0f), "room after sentinel %.2f", r.room);
        CHECK(near(r.gas, 1.0f), "gas after unknown byte %.0f", r.gas);
        CHECK(near(r.setpoint, 23.0f), "setpoint after bad checksum %.2f", r.setpoint);
        CHECK(lin.stats().frames_bad == 1, "bad frames %u", lin.stats().frames_bad);
        CHECK(near(r.hotwater_target, 0.0f), "hot water off %.2f", r.hotwater_target);
        CHECK(near(r.glycol, 54.0f), "glycol after sentinel %.2f", r.glycol);
        CHECK(near(r.mains_current, 2.09f), "current after 0xFFFF %.2f", r.mains_current);
        CHECK(near(r.burner_fan, 0.0f), "fan off %.0f", r.burner_fan);
        lin.tick(2000);
        CHECK(near(lin.readings().ac_active, 1.0f), "ac_active %.0f", lin.readings().ac_active);
        CHECK(!lin.busOk(2000 + AldeLin::BUS_TIMEOUT_MS + 1), "bus still ok after timeout");
    }
    // 3. Garbage without headers must not grow the buffer or crash
    {
        AldeLin lin;
        std::vector<uint8_t> junk(5000, 0xA5);
        lin.feed(junk.data(), junk.size(), 10);
        CHECK(lin.stats().frames_ok == 0 && !lin.busOk(10), "junk produced frames");
    }
    if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
    printf("ALL ALDE TESTS PASSED\n");
    return 0;
}
