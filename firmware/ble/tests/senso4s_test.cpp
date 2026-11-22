// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of senso4s_decode.h
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/senso4s_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}

int main() {
    const uint8_t mac[6] = {0xC1, 0x22, 0x33, 0x44, 0x55, 0x66};
    S4sReading r;
    // Plus, caravanning mode (3), 64 %, battery 90 %, MAC forward
    uint8_t a[12] = {0x03, 64, 0, 0, 90, 0, 0xC1, 0x22, 0x33, 0x44, 0x55, 0x66};
    check(s4sDecode(S4S_COMPANY, a, 12, mac, r) && r.plus && r.level == 64 && r.battery == 90 && !r.warnings,
          "Plus, 64 %");
    // Basic (high nibble 8), not ready
    uint8_t b[12] = {0x81, 0xFF, 0, 0, 77, 0, 0, 0, 0, 0, 0, 0};
    check(s4sDecode(S4S_COMPANY, b, 12, mac, r) && !r.plus && r.level == -1 && r.status == 0xFF &&
          !strncmp(s4sStatusText(r), "Nicht bereit", 12), "Basic, not ready");
    // Plus with motion warning in the high nibble
    uint8_t c[12] = {0x43, 50, 0, 0, 80, 0, 0, 0, 0, 0, 0, 0};
    check(s4sDecode(S4S_COMPANY, c, 12, mac, r) && r.level == 50 && (r.warnings & 4), "motion warning");
    // Nordic id: accepted only with matching MAC (either order) and a valid usage mode
    check(s4sDecode(S4S_COMPANY_NORDIC, a, 12, mac, r), "Nordic id, MAC forward");
    uint8_t rv[12] = {0x03, 64, 0, 0, 90, 0, 0x66, 0x55, 0x44, 0x33, 0x22, 0xC1};
    check(s4sDecode(S4S_COMPANY_NORDIC, rv, 12, mac, r), "Nordic id, MAC reversed");
    uint8_t other[12] = {0x03, 64, 0, 0, 90, 0, 1, 2, 3, 4, 5, 6};
    check(!s4sDecode(S4S_COMPANY_NORDIC, other, 12, mac, r), "Nordic id, other MAC rejected");
    // Mopeka Pro advertisement (also company 0x0059, 10 bytes) is not taken for a scale
    uint8_t mopeka[10] = {0x03, 0x60, 0x3C, 0x10, 0x40, 0x22, 0x33, 0x44, 0, 0};
    check(!s4sDecode(S4S_COMPANY_NORDIC, mopeka, 10, mac, r), "Mopeka advertisement rejected");
    uint8_t bad[12] = {0x03, 150, 0, 0, 90, 0, 0, 0, 0, 0, 0, 0};
    check(!s4sDecode(S4S_COMPANY, bad, 12, mac, r), "invalid level rejected");
    if (fails) { printf("%d test(s) failed\n", fails); return 1; }
    printf("all tests passed\n");
    return 0;
}
