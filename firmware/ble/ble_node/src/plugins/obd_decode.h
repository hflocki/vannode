// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Parsing of ELM327 / STN answers for the plugin "obd" (header only, no Arduino code, so the
// host test firmware/ble/tests/obd_test.cpp can use it).
//
// An ELM327 compatible adapter answers every command with text lines and ends with the
// prompt '>'. With echo, spaces and headers switched off (ATE0, ATS0, ATH0) a mode 01 answer
// looks like "410C1AF8": 0x41 = answer to mode 01, 0x0C = PID, then the data bytes A, B ...
// CAN answers longer than one frame come as "008" (byte count), "0:4302...", "1:...".
// Several control units may answer one request, one line each.

#ifndef CT_OBD_DECODE_H
#define CT_OBD_DECODE_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Formulas of the standard PIDs (SAE J1979), A B C D = data bytes
enum ObdFormula : uint8_t {
    OBD_F_A,            // A
    OBD_F_A_M40,        // A - 40           (temperatures)
    OBD_F_PCT,          // A * 100 / 255    (percent)
    OBD_F_RPM,          // (256A + B) / 4
    OBD_F_AB,           // 256A + B
    OBD_F_AB_D20,       // (256A + B) / 20  (fuel rate l/h)
    OBD_F_AB_D100,      // (256A + B) / 100 (air mass g/s)
    OBD_F_AB_D1000,     // (256A + B) / 1000 (control module voltage)
    OBD_F_ABCD_D10,     // (A<<24 | B<<16 | C<<8 | D) / 10 (odometer km)
    OBD_F_DTC_COUNT,    // PID 01: A & 0x7F = number of stored codes, bit 7 = warning lamp
};

enum ElmStatus : uint8_t {
    ELM_OK,          // Data or "OK"
    ELM_NO_DATA,     // Request understood, no control unit answered
    ELM_NO_CONNECT,  // No bus found / bus error (ignition off, wrong protocol)
    ELM_ERROR,       // "?" / ERROR / STOPPED / BUFFER FULL
    ELM_UNKNOWN,
};

static inline uint8_t obdFormulaBytes(uint8_t f) {
    switch (f) {
    case OBD_F_RPM: case OBD_F_AB: case OBD_F_AB_D20: case OBD_F_AB_D100: case OBD_F_AB_D1000: return 2;
    case OBD_F_ABCD_D10: return 4;
    default: return 1;
    }
}

// Value of a mode 01 answer. mil (optional) gets the warning lamp for OBD_F_DTC_COUNT.
static inline bool obdDecode(uint8_t formula, const uint8_t* d, size_t n, float& v, bool* mil = nullptr) {
    if (n < obdFormulaBytes(formula)) return false;
    uint32_t ab = n >= 2 ? ((uint32_t)d[0] << 8 | d[1]) : 0;
    switch (formula) {
    case OBD_F_A:         v = d[0]; break;
    case OBD_F_A_M40:     v = (float)d[0] - 40.0f; break;
    case OBD_F_PCT:       v = d[0] * 100.0f / 255.0f; break;
    case OBD_F_RPM:       v = ab / 4.0f; break;
    case OBD_F_AB:        v = (float)ab; break;
    case OBD_F_AB_D20:    v = ab / 20.0f; break;
    case OBD_F_AB_D100:   v = ab / 100.0f; break;
    case OBD_F_AB_D1000:  v = ab / 1000.0f; break;
    case OBD_F_ABCD_D10:
        v = (float)(((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) | ((uint32_t)d[2] << 8) | d[3]) / 10.0f;
        break;
    case OBD_F_DTC_COUNT:
        v = (float)(d[0] & 0x7F);
        if (mil) *mil = (d[0] & 0x80) != 0;
        break;
    default: return false;
    }
    return true;
}

static inline int elmHexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Hex text ("41 0C 1A F8" or "410C1AF8") to bytes. -1 = not a pure hex line (text such as
// "SEARCHING...", odd number of digits such as the CAN byte count "008").
static inline int elmHexBytes(const char* s, size_t len, uint8_t* out, size_t max) {
    int n = 0, hi = -1;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == ' ') continue;
        int v = elmHexNibble(c);
        if (v < 0) return -1;
        if (hi < 0) {
            hi = v;
        } else {
            if ((size_t)n >= max) return -1;
            out[n++] = (uint8_t)(hi << 4 | v);
            hi = -1;
        }
    }
    return hi >= 0 ? -1 : n;
}

// One line of an answer: frame index (-1 = none, 0.. = "n:" prefix of a multi-frame answer)
// and its bytes. Returns the number of bytes, -1 if the line is no data line.
static inline int elmLine(const char* s, size_t len, int& frame, uint8_t* out, size_t max) {
    frame = -1;
    const char* colon = (const char*)memchr(s, ':', len);
    if (colon) {
        if (colon == s) return -1;
        int f = elmHexNibble(colon[-1]);
        if (f < 0) return -1;
        frame = f;
        len -= (size_t)(colon + 1 - s);
        s = colon + 1;
    }
    return elmHexBytes(s, len, out, max);
}

// Calls f(frame, bytes, n) for every data line of an answer
template <typename F>
static inline void elmForEachLine(const char* resp, F f) {
    const char* p = resp;
    uint8_t buf[64];
    while (*p) {
        size_t len = strcspn(p, "\r\n>");
        if (len) {
            int frame;
            int n = elmLine(p, len, frame, buf, sizeof(buf));
            if (n > 0) f(frame, buf, (size_t)n);
        }
        p += len;
        if (*p) p++;
    }
}

// Data bytes of the first answer "41 <pid> ..." to a mode 01 request
static inline bool elmMode01(const char* resp, uint8_t pid, uint8_t* data, size_t max, size_t& n) {
    bool found = false;
    elmForEachLine(resp, [&](int, const uint8_t* b, size_t len) {
        if (found || len < 3 || b[0] != 0x41 || b[1] != pid) return;
        n = len - 2 < max ? len - 2 : max;
        memcpy(data, b + 2, n);
        found = true;
    });
    return found;
}

// Supported PIDs from the answer to 01 00 / 01 20 / ... (bit 31 = base + 1). The answers
// of all control units are combined.
static inline bool elmSupported(const char* resp, uint8_t base, uint32_t& mask) {
    bool found = false;
    elmForEachLine(resp, [&](int, const uint8_t* b, size_t len) {
        if (len < 6 || b[0] != 0x41 || b[1] != base) return;
        mask |= ((uint32_t)b[2] << 24) | ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 8) | b[5];
        found = true;
    });
    return found;
}

// Is pid in the bitmasks (mask[0] = PIDs 01..20, mask[1] = 21..40, ...)?
static inline bool obdPidInMask(const uint32_t* mask, size_t words, uint8_t pid) {
    if (pid == 0) return false;
    size_t w = (size_t)(pid - 1) / 32;
    if (w >= words) return false;
    return (mask[w] >> (31 - (pid - 1) % 32)) & 1;
}

static inline ElmStatus elmStatus(const char* resp) {
    if (strstr(resp, "NO DATA")) return ELM_NO_DATA;
    if (strstr(resp, "UNABLE TO CONNECT") || strstr(resp, "BUS INIT") || strstr(resp, "CAN ERROR") ||
        strstr(resp, "BUS ERROR") || strstr(resp, "FB ERROR") || strstr(resp, "LV RESET"))
        return ELM_NO_CONNECT;
    if (strchr(resp, '?') || strstr(resp, "ERROR") || strstr(resp, "STOPPED") || strstr(resp, "BUFFER FULL"))
        return ELM_ERROR;
    if (strstr(resp, "OK")) return ELM_OK;
    return ELM_UNKNOWN;
}

// "12.6V" -> 12.6, NAN if no plausible voltage
static inline float elmVoltage(const char* resp) {
    const char* p = resp;
    while (*p && !(*p >= '0' && *p <= '9')) p++;
    if (!*p) return NAN;
    float v = strtof(p, nullptr);
    return (v > 0.0f && v < 40.0f) ? v : NAN;
}

// Protocol number from the answer to ATDPN ("A6" = automatic, protocol 6). '\0' = unknown.
static inline char elmProtocol(const char* resp) {
    const char* p = resp;
    while (*p == '\r' || *p == '\n' || *p == ' ') p++;
    if (*p == 'A' && p[1] && elmHexNibble(p[1]) >= 0) p++;
    char c = *p;
    if ((c >= '1' && c <= '9') || (c >= 'A' && c <= 'C')) return c;
    return '\0';
}

static inline bool elmProtocolIsCan(char p) {
    return (p >= '6' && p <= '9') || (p >= 'A' && p <= 'C');
}

static inline const char* elmProtocolName(char p) {
    switch (p) {
    case '1': return "SAE J1850 PWM";
    case '2': return "SAE J1850 VPW";
    case '3': return "ISO 9141-2";
    case '4': return "ISO 14230 KWP (5 Baud)";
    case '5': return "ISO 14230 KWP (schnell)";
    case '6': return "CAN 11 Bit 500 kBit/s";
    case '7': return "CAN 29 Bit 500 kBit/s";
    case '8': return "CAN 11 Bit 250 kBit/s";
    case '9': return "CAN 29 Bit 250 kBit/s";
    case 'A': return "SAE J1939 CAN";
    case 'B': case 'C': return "CAN (eigene Einstellung)";
    default: return "unbekannt";
    }
}

// "P0133" from the two bytes of a trouble code
static inline void obdDtcText(uint8_t a, uint8_t b, char* out) {
    static const char LETTER[] = "PCBU";
    static const char HEX[] = "0123456789ABCDEF";
    out[0] = LETTER[a >> 6];
    out[1] = HEX[(a >> 4) & 0x03];
    out[2] = HEX[a & 0x0F];
    out[3] = HEX[b >> 4];
    out[4] = HEX[b & 0x0F];
    out[5] = '\0';
}

// Stored trouble codes from the answer to mode 03. can = CAN protocol (the answer then carries
// the number of codes after 0x43). Returns the number of codes written to codes (max entries),
// total (optional) gets all codes found. Duplicates (several control units) are skipped.
static inline size_t obdParseDtcs(const char* resp, bool can, char (*codes)[6], size_t max, size_t* total = nullptr) {
    uint8_t msg[128];
    size_t len = 0;
    size_t out = 0, all = 0;
    auto flush = [&]() {
        if (len < 1 || msg[0] != 0x43) { len = 0; return; }
        size_t start = 1, count = (len - 1) / 2;
        if (can) {
            if (len < 2) { len = 0; return; }
            start = 2;
            size_t avail = (len - 2) / 2;
            count = msg[1] < avail ? msg[1] : avail;
        }
        for (size_t i = 0; i < count; i++) {
            uint8_t a = msg[start + 2 * i], b = msg[start + 2 * i + 1];
            if (!a && !b) continue;
            char t[6];
            obdDtcText(a, b, t);
            bool dup = false;
            for (size_t k = 0; k < out; k++) if (!strcmp(codes[k], t)) dup = true;
            if (dup) continue;
            all++;
            if (out < max) memcpy(codes[out++], t, 6);
        }
        len = 0;
    };
    elmForEachLine(resp, [&](int frame, const uint8_t* b, size_t n) {
        if (frame <= 0) flush();   // New message (single frame or first frame "0:")
        for (size_t i = 0; i < n && len < sizeof(msg); i++) msg[len++] = b[i];
    });
    flush();
    if (total) *total = all;
    return out;
}

#endif // CT_OBD_DECODE_H
