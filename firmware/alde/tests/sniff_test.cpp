// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Host test for alde_ci_sniff.cpp (passive listener on the external panel bus).
#include "alde_ci.h"
#include "alde_ci_sniff.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int newIds = 0;
static void onNew(uint8_t) { newIds++; }

static void frame(std::vector<uint8_t>& v, uint8_t id, const uint8_t* d, int n, bool classic = false, bool badCs = false) {
    uint8_t pid = aldeLinPid(id);
    v.push_back(0x00);                 // break as the UART sees it
    v.push_back(0x55);
    v.push_back(pid);
    if (n == 0) return;
    for (int i = 0; i < n; i++) v.push_back(d[i]);
    uint8_t cs = aldeCiChecksum(classic ? 0 : pid, d, n);
    v.push_back(badCs ? (uint8_t)(cs ^ 0x5A) : cs);
}

int main() {
    // 1. Parity: the ids used by the reference implementation
    CHECK(aldeLinPid(0x1B) == 0x5B, "pid 1B = %02X", aldeLinPid(0x1B));
    CHECK(aldeLinPid(0x1A) == 0x1A, "pid 1A = %02X", aldeLinPid(0x1A));
    CHECK(aldeLinPid(0x3C) == 0x3C, "pid 3C = %02X", aldeLinPid(0x3C));
    CHECK(aldeLinPid(0x3D) == 0x7D, "pid 3D = %02X", aldeLinPid(0x3D));

    AldeSniffer s;
    memset(&s, 0, sizeof(s));
    s.onNewId = onNew;
    aldeSnifferReset(s);

    // 2. Mixed traffic in one burst: info frame with 8 bytes (enhanced), a 2-byte classic
    //    frame, a header nobody answers, and a response with a broken checksum
    const uint8_t info[8] = {126, 0xFE, 94, 0xDE, 0x9E, 0x29, 0x00, 0x00};
    const uint8_t two[2] = {0x55, 0x00};          // data that looks like sync + break
    const uint8_t diag[8] = {0x10, 0x06, 0xB2, 0x00, 0xDE, 0x41, 0x01, 0x00};
    std::vector<uint8_t> v;
    frame(v, 0x1B, info, 8);
    frame(v, 0x21, two, 2, true);
    frame(v, 0x3D, nullptr, 0);
    frame(v, 0x3C, diag, 8, false, true);
    frame(v, 0x1B, info, 8);
    aldeSnifferFeed(s, v.data(), (int)v.size(), 100);
    aldeSnifferTick(s, 101);
    CHECK(s.ids[0x1B].headers == 0, "parsed before the bus was idle");
    aldeSnifferTick(s, 104);

    CHECK(s.frames == 5, "frames %u", (unsigned)s.frames);
    CHECK(s.ids[0x1B].headers == 2 && s.ids[0x1B].responses == 2, "1B %u/%u", (unsigned)s.ids[0x1B].headers, (unsigned)s.ids[0x1B].responses);
    CHECK(s.ids[0x1B].len == 8 && s.ids[0x1B].enhanced && memcmp(s.ids[0x1B].data, info, 8) == 0, "1B data");
    CHECK(s.ids[0x21].responses == 1 && s.ids[0x21].len == 2 && !s.ids[0x21].enhanced, "21 classic len %u", s.ids[0x21].len);
    CHECK(s.ids[0x3D].headers == 1 && s.ids[0x3D].responses == 0 && s.ids[0x3D].bad == 0, "3D header only");
    CHECK(s.ids[0x3C].headers == 1 && s.ids[0x3C].responses == 0 && s.ids[0x3C].bad == 1, "3C bad checksum");
    CHECK(newIds == 4, "new ids %d", newIds);
    CHECK(s.junk == 0, "junk %u", (unsigned)s.junk);

    // 3. Split across two reads: nothing lost
    {
        AldeSniffer t;
        memset(&t, 0, sizeof(t));
        std::vector<uint8_t> w;
        frame(w, 0x1B, info, 8);
        aldeSnifferFeed(t, w.data(), 5, 10);
        aldeSnifferTick(t, 11);
        aldeSnifferFeed(t, w.data() + 5, (int)w.size() - 5, 12);
        aldeSnifferTick(t, 20);
        CHECK(t.ids[0x1B].responses == 1, "split frame");
    }
    // 4. Line noise is counted as junk, not as frames
    {
        AldeSniffer t;
        memset(&t, 0, sizeof(t));
        const uint8_t noise[6] = {0x12, 0x34, 0x55, 0xFF, 0x99, 0x01};
        aldeSnifferFeed(t, noise, 6, 0);
        aldeSnifferTick(t, 10);
        CHECK(t.frames == 0 && t.junk == 6, "noise frames %u junk %u", (unsigned)t.frames, (unsigned)t.junk);
    }

    if (failures) { printf("%d FAILURE(S)\n", failures); return 1; }
    printf("ALL SNIFF TESTS PASSED\n");
    return 0;
}
