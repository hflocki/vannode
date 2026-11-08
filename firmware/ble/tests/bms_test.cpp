// Host test of the battery protocols (plugins daly, ective, bm2; bms_proto.cpp): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../ble_node/src/plugins/bms_proto.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
static void put16(uint8_t* p, unsigned v) { p[0] = v >> 8; p[1] = v & 0xFF; }
// head, 0x03, length, data, CRC (little endian)
static size_t frame(uint8_t head, const uint8_t* data, uint8_t n, uint8_t* out) {
    out[0] = head;
    out[1] = 0x03;
    out[2] = n;
    memcpy(out + 3, data, n);
    uint16_t crc = bmsCrcModbus(out, n + 3);
    out[n + 3] = crc & 0xFF;
    out[n + 4] = crc >> 8;
    return n + 5;
}

int main() {
    // MODBUS read request, as sent to a Daly (protocol D2: 62 registers from 0)
    uint8_t req[8];
    bmsModbusRead(0xD2, 0x0000, 62, req);
    uint16_t crc = bmsCrcModbus(req, 6);
    check(req[0] == 0xD2 && req[1] == 0x03 && req[5] == 62 && req[6] == (crc & 0xFF) && req[7] == (crc >> 8),
          "MODBUS read request");

    // Daly D2: 4 cells, 2 temperatures, 13.2 V, +5.2 A, 87.5 %, 100 Ah, 12 cycles
    uint8_t d[124] = {0};
    put16(d + 0, 3300); put16(d + 2, 3310); put16(d + 4, 3305); put16(d + 6, 3290);
    put16(d + 64, 65); put16(d + 66, 66);
    put16(d + 80, 132); put16(d + 82, 30052); put16(d + 84, 875);
    put16(d + 96, 1000); put16(d + 98, 4); put16(d + 100, 2); put16(d + 102, 12);
    put16(d + 106, 1); put16(d + 108, 1);
    uint8_t f[200];
    size_t n = frame(0xD2, d, sizeof(d), f);
    check(bmsModbusExpectedLen(f, 3, 0xD2) == (int)n && bmsModbusCrcOk(f, n), "Daly frame length and CRC");
    BmsSample s;
    check(dalyDecode(DALY_D2, f, n, nullptr, 0, s), "Daly D2 decoded");
    check(near(s.voltage, 13.2f) && near(s.current, 5.2f) && near(s.soc, 87.5f) && near(s.charge_ah, 100.0f),
          "Daly: voltage, current, SoC, capacity");
    check(s.cell_count == 4 && near(s.delta_v, 0.020f) && near(s.temperature, 25.5f) && s.cycles == 12,
          "Daly: cells, delta, temperature, cycles");
    check(near(s.power, 68.64f, 0.01f) && std::isnan(s.runtime_s) && !s.problem, "Daly: power, no runtime while charging");

    // Discharging: runtime from capacity / current
    put16(d + 82, 30000 - 100);   // -10 A
    n = frame(0xD2, d, sizeof(d), f);
    check(dalyDecode(DALY_D2, f, n, nullptr, 0, s) && near(s.runtime_s, 36000.0f, 1), "Daly: runtime 10 h at -10 A");

    // Fault register 0x3A: low byte = voltages, high byte = temperatures (swapped against UART)
    d[117] = 0x02;
    n = frame(0xD2, d, sizeof(d), f);
    char pt[160];
    check(dalyDecode(DALY_D2, f, n, nullptr, 0, s) && s.problem && bmsProblemText(s, pt, sizeof(pt)) > 0 &&
              !strcmp(pt, "Zellspannung zu hoch (Stufe 2)"), "Daly: problem text, voltage byte");
    d[117] = 0x00;
    d[116] = 0x04;
    n = frame(0xD2, d, sizeof(d), f);
    check(dalyDecode(DALY_D2, f, n, nullptr, 0, s) && bmsProblemText(s, pt, sizeof(pt)) > 0 &&
              !strcmp(pt, "Ladetemperatur zu niedrig (Stufe 1)"), "Daly: problem text, temperature byte");
    d[116] = 0x00;
    d[119] = 0x40;   // register 0x3B low byte: SoC low (level 1)
    n = frame(0xD2, d, sizeof(d), f);
    check(dalyDecode(DALY_D2, f, n, nullptr, 0, s) && bmsProblemText(s, pt, sizeof(pt)) > 0 &&
              !strcmp(pt, "Ladezustand zu niedrig (Stufe 1)"), "Daly: problem text, current/SoC byte");

    // Broken CRC is rejected
    f[10] ^= 0xFF;
    check(!dalyDecode(DALY_D2, f, n, nullptr, 0, s), "Daly: bad CRC rejected");

    // Ective BM X: 100 Ah, 80 %, 5 h 30 min, discharging 5.00 A, 13.2 V (device id 0x22)
    uint8_t e[16] = {0};
    put16(e + 0, 100); put16(e + 2, 80);
    put16(e + 4, 5); put16(e + 6, 30);
    e[9] = 0x01;                     // frame byte 12: direction (discharging)
    put16(e + 10, 500); put16(e + 12, 132);
    n = frame(0x22, e, sizeof(e), f);
    check(ectiveDecode(f, n, s) && near(s.charge_ah, 100) && near(s.soc, 80) && near(s.current, -5.0f) &&
              near(s.voltage, 13.2f) && near(s.runtime_s, 19800.0f, 1), "Ective: values and runtime 5 h 30 min");
    n = frame(0x99, e, sizeof(e), f);
    check(!ectiveDecode(f, n, s), "Ective: unknown device id rejected");

    // BM2 (decrypted block): 0xF5, 12 bit voltage 1275 (12.75 V), 76 %
    uint8_t b[16] = {0xF5, 0x4F, 0xB0, 76};
    check(bm2Decode(b, s) && near(s.voltage, 12.75f) && near(s.soc, 76) && s.bat_state == BAT_STATE_NORMAL,
          "BM2 notification: 12.75 V, 76 %, normal");
    uint8_t a[16] = {0};
    a[6] = 0x05; a[7] = 0x6E; a[8] = 100;   // 1390 -> 13.90 V
    check(bm2DecodeAdv(a, s) && near(s.voltage, 13.9f) && s.bat_state == BAT_STATE_CHARGING, "BM2 broadcast: charging");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
