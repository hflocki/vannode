// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Passive listener for the external panel bus, see alde_ci_sniff.h.
#include "alde_ci_sniff.h"
#include <string.h>

uint8_t aldeLinPid(uint8_t id) {
    id &= 0x3F;
    uint8_t b0 = id & 1, b1 = (id >> 1) & 1, b2 = (id >> 2) & 1;
    uint8_t b3 = (id >> 3) & 1, b4 = (id >> 4) & 1, b5 = (id >> 5) & 1;
    uint8_t p0 = b0 ^ b1 ^ b2 ^ b4;
    uint8_t p1 = (uint8_t)(~(b1 ^ b3 ^ b4 ^ b5) & 1);
    return (uint8_t)(id | (p0 << 6) | (p1 << 7));
}

static bool validPid(uint8_t pid) {
    return aldeLinPid(pid & 0x3F) == pid;
}

static uint8_t checksum(uint16_t start, const uint8_t* d, int n) {
    uint16_t total = start;
    for (int i = 0; i < n; i++) {
        total += d[i];
        if (total > 0xFF) total = (total & 0xFF) + 1;
    }
    return (uint8_t)(~total & 0xFF);
}

// Start of a new frame at buf[i]: sync byte + valid PID, optionally after a break byte
// (0x00). A lone 0x00 at the very end of the burst also counts (header still arriving).
static bool frameStart(const uint8_t* b, int i, int n) {
    if (b[i] == 0x00) {
        if (i + 1 >= n) return true;
        i++;
    }
    return b[i] == 0x55 && i + 1 < n && validPid(b[i + 1]);
}

void aldeSnifferReset(AldeSniffer& s) {
    void (*newId)(uint8_t) = s.onNewId;
    void (*frame)(uint8_t, const uint8_t*, int, bool) = s.onFrame;
    memset(&s, 0, sizeof(s));
    s.onNewId = newId;
    s.onFrame = frame;
}

static void parse(AldeSniffer& s, unsigned long now) {
    const uint8_t* b = s.buf;
    const int n = s.n;
    int i = 0;
    while (i < n) {
        if (b[i] == 0x00) { i++; continue; }                 // break as seen by the UART
        if (b[i] != 0x55 || i + 1 >= n || !validPid(b[i + 1])) { s.junk++; i++; continue; }

        const uint8_t pid = b[i + 1];
        const uint8_t id = pid & 0x3F;
        const uint8_t* d = b + i + 2;
        const int avail = n - (i + 2);
        int len = 0;
        bool enh = false;
        // Shortest length whose checksum matches and after which a new frame (or the end of
        // the buffer) follows. Not the longest: data + its checksum always sums up so that
        // the following break byte (0x00) looks like a valid checksum one byte later.
        for (int L = 1; L <= 8 && L + 1 <= avail; L++) {
            const int next = i + 2 + L + 1;
            if (next < n && !frameStart(b, next, n)) continue;
            if (d[L] == checksum(pid, d, L)) { len = L; enh = true; break; }
            if (d[L] == checksum(0, d, L)) { len = L; enh = false; break; }
        }

        AldeSniffId& e = s.ids[id];
        if (e.headers == 0 && s.onNewId) s.onNewId(id);
        e.headers++;
        e.last_ms = now;
        s.frames++;

        if (len > 0) {
            e.responses++;
            e.len = (uint8_t)len;
            e.enhanced = enh;
            memcpy(e.data, d, len);
            if (s.onFrame) s.onFrame(id, d, len, true);
            i += 2 + len + 1;
        } else {
            // Header only, or bytes that do not form a valid response: skip to the next frame
            int j = i + 2;
            while (j < n && !frameStart(b, j, n)) j++;
            if (j > i + 2) e.bad++;
            if (s.onFrame) s.onFrame(id, d, j - (i + 2), false);
            i = j;
        }
    }
    s.n = 0;
}

void aldeSnifferFeed(AldeSniffer& s, const uint8_t* b, int n, unsigned long now) {
    for (int k = 0; k < n; k++) {
        if (s.n >= (int)sizeof(s.buf)) parse(s, now);
        s.buf[s.n++] = b[k];
    }
    s.bytes += (uint32_t)n;
    if (n > 0) s.lastByteMs = now;
}

void aldeSnifferTick(AldeSniffer& s, unsigned long now, unsigned long idleMs) {
    if (s.n > 0 && now - s.lastByteMs >= idleMs) parse(s, now);
}
