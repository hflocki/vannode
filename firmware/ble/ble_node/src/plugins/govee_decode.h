#ifndef CT_GOVEE_DECODE_H
#define CT_GOVEE_DECODE_H

// Pure decoding of Govee thermo-hygrometer advertisements (no Arduino types, so it can be
// tested on a PC: firmware/ble/tests/govee_test.cpp).
//
// All formats are manufacturer data; d / n = payload WITHOUT the 2 company id bytes.
//   0xEC88, 6 bytes  H5072, H5075        [1..3] packed value, [4] battery
//   0xEC88, 7 bytes  H5074               [1..2] temp int16 LE / 100, [3..4] hum / 100, [5] battery
//   0x0001, 6 bytes  H5100, H5101, H5102, H5104, H5174, H5177
//                                        [2..4] packed value, [5] battery
//   0x8801, 9 bytes  H5179               [4..5] temp int16 LE / 100, [6..7] hum / 100, [8] battery
// Packed value (24 bit big endian): bit 23 = below zero, rest = temp * 10000 + hum * 10,
// i.e. temp = (v / 1000) / 10 °C, hum = (v % 1000) / 10 %.
// Formats as in the MIT licensed govee-ble library used by Home Assistant
// (https://github.com/Bluetooth-Devices/govee-ble); own implementation.

#include <stddef.h>
#include <stdint.h>

#define GOVEE_CID_H5075 0xEC88
#define GOVEE_CID_H5101 0x0001
#define GOVEE_CID_H5179 0x8801

struct GoveeReading {
    float temperature;   // °C
    float humidity;      // %
    int battery;         // %
    const char* model;   // "H5075" ...
};

static inline void goveePacked(const uint8_t* p, float& t, float& h) {
    uint32_t v = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
    bool neg = v & 0x800000;
    v &= 0x7FFFFF;
    t = (float)(v / 1000) / 10.0f;
    if (neg) t = -t;
    h = (float)(v % 1000) / 10.0f;
}

static inline float goveeLe16(const uint8_t* p, bool isSigned) {
    uint16_t v = (uint16_t)(p[0] | (p[1] << 8));
    return isSigned ? (float)(int16_t)v : (float)v;
}

// Returns false for unknown company ids / lengths or implausible values
static inline bool goveeDecode(int32_t company, const uint8_t* d, size_t n, GoveeReading& r) {
    if (company == GOVEE_CID_H5075 && n == 6) {
        goveePacked(d + 1, r.temperature, r.humidity);
        r.battery = d[4] & 0x7F;
        r.model = "H5072/H5075";
    } else if (company == GOVEE_CID_H5075 && n == 7) {
        r.temperature = goveeLe16(d + 1, true) / 100.0f;
        r.humidity = goveeLe16(d + 3, false) / 100.0f;
        r.battery = d[5] & 0x7F;
        r.model = "H5074";
    } else if (company == GOVEE_CID_H5101 && n == 6) {
        goveePacked(d + 2, r.temperature, r.humidity);
        r.battery = d[5] & 0x7F;
        r.model = "H5101/H5102/H5174/H5177";
    } else if (company == GOVEE_CID_H5179 && n == 9) {
        r.temperature = goveeLe16(d + 4, true) / 100.0f;
        r.humidity = goveeLe16(d + 6, false) / 100.0f;
        r.battery = d[8] & 0x7F;
        r.model = "H5179";
    } else {
        return false;
    }
    // Sensor range is -40 .. 100 °C; anything else is another packet with the same id
    if (r.temperature < -45.0f || r.temperature > 105.0f || r.humidity < 0.0f || r.humidity > 100.0f) return false;
    if (r.battery > 100) r.battery = 100;
    return true;
}

static inline bool goveeCompany(int32_t company) {
    return company == GOVEE_CID_H5075 || company == GOVEE_CID_H5101 || company == GOVEE_CID_H5179;
}

#endif // CT_GOVEE_DECODE_H
