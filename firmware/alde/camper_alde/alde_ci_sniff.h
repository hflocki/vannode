// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Passive listener for the external panel bus (ci_mode 3, diagnosis). The ESP does not
// transmit; it only collects which LIN frames other devices put on the bus. Without any
// other device attached, every header seen here comes from the Alde panel - that answers
// whether the panel is LIN master on this bus.
//
// Framing without break detection: the byte stream is split at "55 <valid PID>". The
// response length (1-8) is the shortest one whose last byte is a valid enhanced (LIN 2.x)
// or classic (LIN 1.x) checksum and that is followed by a new frame or the end of the
// burst. A header with no matching response counts as "no answer".
#ifndef ALDE_CI_SNIFF_H
#define ALDE_CI_SNIFF_H

#include <stdint.h>

struct AldeSniffId {
    uint32_t headers;      // headers seen with this id
    uint32_t responses;    // headers followed by a response with a valid checksum
    uint32_t bad;          // headers followed by bytes without a valid checksum
    uint8_t len;           // data length of the last valid response
    uint8_t data[8];       // last valid response
    bool enhanced;         // last valid response used the enhanced checksum
    unsigned long last_ms; // time of the last header
};

struct AldeSniffer {
    AldeSniffId ids[64];
    uint32_t bytes;        // bytes received
    uint32_t frames;       // headers recognised
    uint32_t junk;         // bytes that did not belong to any frame
    uint8_t buf[192];
    int n;
    unsigned long lastByteMs;
    // Optional hooks (may be null): first header of an id, and every parsed frame
    void (*onNewId)(uint8_t id);
    void (*onFrame)(uint8_t id, const uint8_t* d, int len, bool ok);
};

// LIN protected id (id + parity bits) for a 6-bit id
uint8_t aldeLinPid(uint8_t id);

void aldeSnifferReset(AldeSniffer& s);
// Add received bytes; parses at once when the buffer is nearly full
void aldeSnifferFeed(AldeSniffer& s, const uint8_t* b, int n, unsigned long now);
// Parse what is buffered once the bus has been idle for idleMs (call often)
void aldeSnifferTick(AldeSniffer& s, unsigned long now, unsigned long idleMs = 3);

#endif // ALDE_CI_SNIFF_H
