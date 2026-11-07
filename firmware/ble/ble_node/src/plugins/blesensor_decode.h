#ifndef CT_BLESENSOR_DECODE_H
#define CT_BLESENSOR_DECODE_H

// Pure decoding helpers of the plugin "blesensor" (no Arduino types, so they can be
// tested on a PC: tools/test/blesensor_test.cpp).

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Reads len bytes (1, 2 or 4) at offset, little or big endian, optionally as signed
// (two's complement) integer, and converts it: out = value * factor + add.
// Returns false if the bytes are not inside the payload.
static inline bool bsDecode(const uint8_t* d, size_t n, uint32_t offset, uint8_t len, bool isSigned,
                            bool bigEndian, float factor, float add, float& out) {
    if (!(len == 1 || len == 2 || len == 4) || offset + len > n) return false;
    uint32_t v = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = d[offset + i];
        if (bigEndian) v = (v << 8) | b;
        else v |= (uint32_t)b << (8 * i);
    }
    double x;
    if (isSigned) {
        if (len == 1) x = (int8_t)v;
        else if (len == 2) x = (int16_t)v;
        else x = (int32_t)v;
    } else {
        x = v;
    }
    out = (float)(x * factor + add);
    return true;
}

// Hex view with positions for the web UI: "0: 01 AB FF 00  4: 12 34"
static inline void bsHexWithPositions(const uint8_t* d, size_t n, char* out, size_t size) {
    size_t o = 0;
    if (size) out[0] = '\0';
    for (size_t i = 0; i < n && o + 8 < size; i++) {
        int w;
        if (i % 4 == 0) w = snprintf(out + o, size - o, "%s%u: %02X", i ? "  " : "", (unsigned)i, d[i]);
        else w = snprintf(out + o, size - o, " %02X", d[i]);
        if (w < 0) break;
        o += (size_t)w;
    }
}

// "FFFF", "0x004c", "" (= any) -> company id, -1 = any, -2 = invalid
static inline int32_t bsParseCompany(const char* s) {
    if (!s || !s[0]) return -1;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    int32_t v = 0;
    int digits = 0;
    for (; *s; s++) {
        char c = *s;
        int h = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (h < 0 || ++digits > 4) return -2;
        v = (v << 4) | h;
    }
    return digits ? v : -1;
}

#endif // CT_BLESENSOR_DECODE_H
