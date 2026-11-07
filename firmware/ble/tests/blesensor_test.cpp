// Host test of the value decoding of the plugin "blesensor" (no ESP32 needed): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/blesensor_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

int main() {
    // DIMES like payload (manufacturer data without company id): level 90 %, battery 100 %
    const uint8_t dimes[] = {0x01, 0x02, 0x5A, 0x64};
    float v;
    check(bsDecode(dimes, sizeof(dimes), 2, 1, false, false, 1, 0, v) && near(v, 90), "1 byte unsigned");
    check(bsDecode(dimes, sizeof(dimes), 3, 1, false, false, 1, 0, v) && near(v, 100), "battery byte");

    // Xiaomi ATC / pvvx style service data 181A: temp int16 LE * 0.01, humidity uint16 LE * 0.01
    const uint8_t atc[] = {0x38, 0xC1, 0xA4, 0x12, 0x34, 0x56, 0x66, 0xF8, 0x10, 0x1A};
    //                                                         ^ 0xF866 = -1946 -> -19.46
    check(bsDecode(atc, sizeof(atc), 6, 2, true, false, 0.01f, 0, v) && near(v, -19.46f), "2 byte signed little endian");
    check(bsDecode(atc, sizeof(atc), 6, 2, false, false, 0.01f, 0, v) && near(v, 635.90f), "same bytes unsigned");
    check(bsDecode(atc, sizeof(atc), 8, 2, false, false, 0.01f, 0, v) && near(v, 66.72f), "humidity 0x1A10");

    // Big endian and offset
    const uint8_t be[] = {0x00, 0x01, 0x2C, 0xFF, 0xFF, 0xFF, 0xFE};
    check(bsDecode(be, sizeof(be), 1, 2, false, true, 0.1f, -10, v) && near(v, 20.0f), "2 byte big endian * 0.1 - 10");
    check(bsDecode(be, sizeof(be), 3, 4, true, true, 1, 0, v) && near(v, -2), "4 byte signed big endian");

    // Out of range and invalid length
    check(!bsDecode(be, sizeof(be), 6, 2, false, false, 1, 0, v), "position past the end");
    check(!bsDecode(be, sizeof(be), 0, 3, false, false, 1, 0, v), "length 3 rejected");

    // Company ids
    check(bsParseCompany("") == -1, "company empty = any");
    check(bsParseCompany("FFFF") == 0xFFFF, "company FFFF");
    check(bsParseCompany("0x004c") == 0x4C, "company 0x004c");
    check(bsParseCompany("12345") == -2 && bsParseCompany("zz") == -2, "company invalid");

    // Hex view with positions
    char out[96];
    bsHexWithPositions(atc, 6, out, sizeof(out));
    check(!strcmp(out, "0: 38 C1 A4 12  4: 34 56"), out);
    bsHexWithPositions(atc, 6, out, 12);   // small buffer: cut, never overflows
    check(strlen(out) < 12, "hex view respects the buffer size");

    printf(fails ? "%d test(s) failed\n" : "all tests passed\n", fails);
    return fails ? 1 : 0;
}
