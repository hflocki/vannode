// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the battery protocols (plugins daly, ective, bm2, jbd, jk, litime; bms_proto.cpp): ./run.sh
#include <cmath>
#include <cstdio>
#include <cstring>
#include "../battery_node/src/plugins/bms_proto.h"
#include "bms_vectors.h"

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


    // ---- JBD (reference frames from aiobmsble) --------------------------------------
    {
        uint8_t rq[7];
        jbdRequest(JBD_CMD_BASIC, rq);
        const uint8_t exp[7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
        check(memcmp(rq, exp, 7) == 0, "JBD request 0x03");
        jbdRequest(JBD_CMD_CELLS, rq);
        check(rq[4] == 0xFF && rq[5] == 0xFC, "JBD request 0x04");
        check(jbdExpectedLen(JBD_BASIC, 4) == (int)sizeof(JBD_BASIC), "JBD expected length");
        BmsSample s;
        // aiobmsble: 15.6 V, -2.87 A, 100 %, 4.98 Ah, 42 cycles, 22.4/22.3/21.7 C
        bool ok = jbdDecodeBasic(JBD_BASIC, sizeof(JBD_BASIC), s);
        check(ok && near(s.voltage, 15.6f) && near(s.current, -2.87f) && near(s.soc, 100) &&
              near(s.charge_ah, 4.98f) && s.cycles == 42, "JBD basic info");
        check(s.temp_count == 3 && near(s.temp_c[0], 22.4f, 0.01f) && near(s.temp_c[2], 21.7f, 0.01f) &&
              !s.problem && s.charge_fet == 1 && s.discharge_fet == 1, "JBD temperatures, FETs");
        // cells: 3.43 3.425 3.432 3.417, delta 0.015
        ok = jbdDecodeCells(JBD_CELLS, sizeof(JBD_CELLS), s);
        check(ok && s.cell_count == 4 && near(s.cell_v[1], 3.425f) && near(s.delta_v, 0.015f), "JBD cells");
        // 100 mA units (control byte 0x88): -5.8 A, 471.9 Ah, 13.28 V, 72 %, 37 cycles
        ok = jbdDecodeBasic(JBD_BASIC_D10, sizeof(JBD_BASIC_D10), s);
        check(ok && near(s.current, -5.8f) && near(s.charge_ah, 471.9f, 0.01f) && near(s.voltage, 13.28f) &&
              near(s.soc, 72) && s.cycles == 37, "JBD 100 mA units");
        uint8_t bad[sizeof(JBD_BASIC)];
        memcpy(bad, JBD_BASIC, sizeof(bad));
        bad[10] ^= 1;
        check(!jbdDecodeBasic(bad, sizeof(bad), s), "JBD bad checksum rejected");
        BmsSample p;
        bmsClear(p);
        p.problem = true;
        p.problem_fmt = 2;
        p.problem_code = (1 << 0) | (1 << 10);
        char t[160];
        bmsProblemText(p, t, sizeof(t));
        check(!strcmp(t, "Zellspannung zu hoch, Kurzschluss"), "JBD problem text");
    }

    // ---- JK-BMS JK02 (reference frames from aiobmsble) ------------------------------
    {
        uint8_t cmd[20];
        jkCommand(JK_CMD_CELLS, cmd);
        uint8_t sum = 0;
        for (int i = 0; i < 19; i++) sum += cmd[i];
        check(cmd[0] == 0xAA && cmd[3] == 0xEB && cmd[4] == 0x96 && cmd[19] == sum, "JK command 0x96");
        struct JkCase {
            const char* name;
            const uint8_t* dev;
            const uint8_t* cell;
            int sw;
            float v, a, soc, ah, delta, t0;
            int cycles, cells, temps;
        };
        // values as decoded by aiobmsble (temperatures: first sensor, count capped at BMS_MAX_TEMPS)
        const JkCase cases[] = {
            {"JK02 24S (fw 10)", JK_JK02_24S_DEV, JK_JK02_24S_CELL, 10, 52.971f, 2.329f, 56, 113.245f, 0.005f, 18.1f, 60, 16, 3},
            {"JK02 32S (fw 11)", JK_JK02_32S_DEV, JK_JK02_32S_CELL, 11, 26.509f, -7.063f, 68, 142.464f, 0.003f, 31.0f, 21, 8, 4},
            {"JK02 32S (fw 15)", JK_JK02_32S_v15_DEV, JK_JK02_32S_v15_CELL, 15, 53.224f, 31.881f, 25, 49.286f, 0.016f, 12.9f, 9, 16, 4},
            {"JK02 32S (fw 19.05)", JK_JK02_32S_v19_05_DEV, JK_JK02_32S_v19_05_CELL, 19, 53.224f, 31.881f, 25, 49.286f, 0.016f, 12.9f, 9, 16, 4},
            {"JK02 32S (fw 19.27)", JK_JK02_32S_v19_27_DEV, JK_JK02_32S_v19_27_CELL, 19, 26.481f, -12.684f, 78, 244.296f, 0.005f, 26.2f, 15, 8, 4},
        };
        for (const JkCase& c : cases) {
            char what[80];
            int sw = jkSwVersion(c.dev, JK_FRAME_LEN);
            snprintf(what, sizeof(what), "%s: firmware version %d", c.name, sw);
            check(sw == c.sw, what);
            BmsSample s;
            bool ok = jkDecode(c.cell, JK_FRAME_LEN, sw, s);
            snprintf(what, sizeof(what), "%s: values", c.name);
            check(ok && near(s.voltage, c.v) && near(s.current, c.a) && near(s.soc, c.soc) &&
                  near(s.charge_ah, c.ah) && s.cycles == c.cycles && !s.problem, what);
            snprintf(what, sizeof(what), "%s: %u cells, delta, %u temperatures", c.name, s.cell_count, s.temp_count);
            check(s.cell_count == c.cells && near(s.delta_v, c.delta) && s.temp_count == c.temps &&
                  near(s.temp_c[0], c.t0, 0.01f), what);
        }
        BmsSample s;
        check(!jkDecode(JK_JK02_32S_DEV, JK_FRAME_LEN, 11, s), "JK device info is not cell info");
        uint8_t bad[JK_FRAME_LEN];
        memcpy(bad, JK_JK02_32S_CELL, JK_FRAME_LEN);
        bad[100] ^= 1;
        check(!jkDecode(bad, JK_FRAME_LEN, 11, s), "JK bad checksum rejected");
    }

    // ---- LiTime / Redodo (reference frame from aiobmsble) ---------------------------
    {
        check(litimeExpectedLen(LITIME_RESP, 3) == (int)sizeof(LITIME_RESP), "LiTime expected length");
        BmsSample s;
        // aiobmsble: 26.556 V, -1.435 A, 65 %, 68.89 Ah, 3 cycles, 8 cells, 23/22/-2 C
        bool ok = litimeDecode(LITIME_RESP, sizeof(LITIME_RESP), s);
        check(ok && near(s.voltage, 26.556f) && near(s.current, -1.435f) && near(s.soc, 65) &&
              near(s.charge_ah, 68.89f) && s.cycles == 3 && !s.problem, "LiTime values");
        check(s.cell_count == 8 && near(s.cell_v[0], 3.317f) && near(s.delta_v, 0.010f) &&
              s.temp_count == 3 && near(s.temp_c[2], -2), "LiTime cells, temperatures");
        uint8_t bad[sizeof(LITIME_RESP)];
        memcpy(bad, LITIME_RESP, sizeof(bad));
        bad[sizeof(bad) - 1] ^= 0xFF;
        check(!litimeDecode(bad, sizeof(bad), s), "LiTime bad checksum rejected");
    }

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all tests passed\n");
    return 0;
}
