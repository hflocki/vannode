// Host test of the BTHome v2 decoding (plugin "bthome", no ESP32 needed): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/bthome_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }

int main() {
    BthValue v[12];
    size_t n;
    // pvvx (ATC firmware) in BTHome mode: packet id, battery 100 %, 22.50 °C, 50.55 %, 2.854 V
    const uint8_t pvvx[] = {0x40, 0x00, 0xA5, 0x01, 0x64, 0x02, 0xCA, 0x08, 0x03, 0xBF, 0x13, 0x0C, 0x26, 0x0B};
    check(bthParse(pvvx, sizeof(pvvx), v, 12, n) == BTH_OK && n == 4, "pvvx: 4 values (packet id skipped)");
    check(v[0].id == 0x01 && near(v[0].v, 100), "battery 100 %");
    check(v[1].id == 0x02 && near(v[1].v, 22.5f), "temperature 22.50 °C");
    check(v[2].id == 0x03 && near(v[2].v, 50.55f), "humidity 50.55 %");
    check(v[3].id == 0x0C && near(v[3].v, 2.854f), "voltage 2.854 V");

    // Below zero (sint16): 0xFF38 = -200 -> -2.00 °C; 0x45 sint16 * 0.1: 0xFFF6 -> -1.0
    const uint8_t neg[] = {0x40, 0x02, 0x38, 0xFF, 0x45, 0xF6, 0xFF};
    check(bthParse(neg, sizeof(neg), v, 12, n) == BTH_OK && n == 2 && near(v[0].v, -2.0f) && near(v[1].v, -1.0f),
          "negative temperatures");

    // Shelly BLU Button (trigger based): battery, button press
    const uint8_t btn[] = {0x44, 0x00, 0x12, 0x01, 0x5A, 0x3A, 0x01};
    check(bthParse(btn, sizeof(btn), v, 12, n) == BTH_OK && n == 2 && v[1].id == 0x3A &&
              !strcmp(bthEventText(v[1].id, v[1].v), "press"), "button press");
    check(!strcmp(bthEventText(0x3A, 0x04), "long_press") && !strcmp(bthEventText(0x3A, 0), ""), "event texts");

    // Shelly BLU Door/Window: battery, illuminance (uint24 * 0.01), window open, rotation
    const uint8_t door[] = {0x44, 0x00, 0x01, 0x01, 0x64, 0x05, 0x10, 0x27, 0x00, 0x2D, 0x01, 0x3F, 0x84, 0x03};
    check(bthParse(door, sizeof(door), v, 12, n) == BTH_OK && n == 4 && near(v[1].v, 100.0f) && v[2].id == 0x2D &&
              near(v[2].v, 1) && near(v[3].v, 90.0f), "door/window: lux, open, rotation 90°");

    // Two temperatures in one packet
    const uint8_t two[] = {0x40, 0x02, 0xCA, 0x08, 0x02, 0x10, 0x0E};
    check(bthParse(two, sizeof(two), v, 12, n) == BTH_OK && n == 2 && near(v[1].v, 36.0f), "two temperatures");

    // Text object with length byte is skipped, parsing goes on
    const uint8_t txt[] = {0x40, 0x53, 0x03, 'a', 'b', 'c', 0x01, 0x32};
    check(bthParse(txt, sizeof(txt), v, 12, n) == BTH_OK && n == 1 && near(v[0].v, 50), "text skipped");

    // Errors
    const uint8_t enc[] = {0x41, 0x01, 0x64};
    check(bthParse(enc, sizeof(enc), v, 12, n) == BTH_ENCRYPTED, "encrypted rejected");
    const uint8_t v1[] = {0x02, 0x01, 0x64};
    check(bthParse(v1, sizeof(v1), v, 12, n) == BTH_WRONG_VERSION, "not version 2");
    const uint8_t unk[] = {0x40, 0x02, 0xCA, 0x08, 0xEE, 0x01};
    check(bthParse(unk, sizeof(unk), v, 12, n) == BTH_UNKNOWN_OBJECT && n == 1 && near(v[0].v, 22.5f),
          "unknown object: values before it kept");
    const uint8_t cut[] = {0x40, 0x02, 0xCA};
    check(bthParse(cut, sizeof(cut), v, 12, n) == BTH_TOO_SHORT && n == 0, "cut packet");
    check(bthParse(pvvx, sizeof(pvvx), v, 2, n) == BTH_OK && n == 2, "max respected");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
