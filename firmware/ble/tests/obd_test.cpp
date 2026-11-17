// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Host test of the ELM327 answer parsing (plugin "obd", no ESP32 needed): ./run.sh
#include <cmath>
#include <cstdio>
#include "../ble_node/src/plugins/obd_decode.h"

static int fails = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) fails++;
}
static bool near(float a, float b) { return std::fabs(a - b) < 0.01f; }

static bool value(const char* resp, uint8_t pid, uint8_t formula, float& v, bool* mil = nullptr) {
    uint8_t d[16];
    size_t n = 0;
    return elmMode01(resp, pid, d, sizeof(d), n) && obdDecode(formula, d, n, v, mil);
}

int main() {
    float v = 0;
    bool mil = false;

    // Mode 01 answers, without and with spaces, with "SEARCHING..." and the prompt
    check(value("410C1AF8\r\r>", 0x0C, OBD_F_RPM, v) && near(v, 1726.0f), "RPM 41 0C 1A F8 = 1726");
    check(value("41 0C 1A F8 \r\r>", 0x0C, OBD_F_RPM, v) && near(v, 1726.0f), "RPM with spaces");
    check(value("SEARCHING...\r410D32\r\r>", 0x0D, OBD_F_A, v) && near(v, 50.0f), "speed after SEARCHING");
    check(value("41057B\r>", 0x05, OBD_F_A_M40, v) && near(v, 83.0f), "coolant 0x7B = 83 °C");
    check(value("412FFF\r>", 0x2F, OBD_F_PCT, v) && near(v, 100.0f), "fuel level 100 %");
    check(value("41422EE0\r>", 0x42, OBD_F_AB_D1000, v) && near(v, 12.0f), "module voltage 12.000 V");
    check(value("415E0190\r>", 0x5E, OBD_F_AB_D20, v) && near(v, 20.0f), "fuel rate 20 l/h");
    check(value("41A6000F4240\r>", 0xA6, OBD_F_ABCD_D10, v) && near(v, 100000.0f), "odometer 100000 km");
    check(value("41018307E100\r>", 0x01, OBD_F_DTC_COUNT, v, &mil) && near(v, 3.0f) && mil, "PID 01: 3 codes, lamp on");
    check(value("410C1AF8\r410C1B00\r>", 0x0C, OBD_F_RPM, v) && near(v, 1726.0f), "two control units: first wins");
    check(!value("NO DATA\r\r>", 0x0C, OBD_F_RPM, v), "NO DATA gives no value");
    check(!value("410D\r>", 0x0D, OBD_F_A, v), "too short");
    check(!value("410C1AF8\r>", 0x0D, OBD_F_A, v), "other PID ignored");
    check(!value("010C\r410C1A\r>", 0x0C, OBD_F_RPM, v), "echo ignored, one byte too short");
    check(!value("415CFF\r>", 0x5C, OBD_F_A_M40, v), "oil temperature FF (not available) dropped");

    // AdBlue and fuel mass (values as seen in a Fiat Ducato)
    check(value("00C\r0:418507000000\r1:00E2000000AAAA\r\r>", 0x85, OBD_F_REAGENT_LVL, v) && near(v, 88.63f),
          "0185 multi frame: AdBlue E2 = 88.6 %");
    check(!value("00C\r0:418503000000\r1:00E2000000AAAA\r>", 0x85, OBD_F_REAGENT_LVL, v),
          "0185 without tank level bit");
    check(!value("00C\r0:418507000000\r0:418507000000\r1:00E2000000AAAA\r1:00E2000000AAAA\r>", 0x85,
                 OBD_F_REAGENT_LVL, v),
          "0185 frames of two control units mixed: dropped");
    check(value("419B0E802BE1\r>", 0x9B, OBD_F_DEF_CONC, v) && near(v, 32.0f), "019B concentration 32 %");
    check(value("419B0E802BE1\r>", 0x9B, OBD_F_DEF_TEMP, v) && near(v, 3.0f), "019B AdBlue 3 °C");
    check(value("419D000B0000\r>", 0x9D, OBD_F_FUEL_GS, v) && std::fabs(v - 0.948f) < 0.005f,
          "019D 0.22 g/s = 0.95 l/h diesel");

    // Supported PIDs: two control units are combined
    uint32_t mask[2] = {0, 0};
    check(elmSupported("4100BE1FA813\r410098180001\r>", 0x00, mask[0]) && mask[0] == (0xBE1FA813u | 0x98180001u),
          "0100 from two control units");
    check(obdPidInMask(mask, 2, 0x0C) && obdPidInMask(mask, 2, 0x05) && !obdPidInMask(mask, 2, 0x02),
          "PID 0C and 05 supported, 02 not");
    check(obdPidInMask(mask, 2, 0x20), "bit 0 = next range (0x20) available");
    check(!obdPidInMask(mask, 2, 0x2F) && !obdPidInMask(mask, 2, 0x50), "outside the known masks");
    check(elmSupported("412000020001\r>", 0x20, mask[1]) && obdPidInMask(mask, 2, 0x2F) &&
              !obdPidInMask(mask, 2, 0x2E) && obdPidInMask(mask, 2, 0x40),
          "0120: fuel level 2F and next range 40 supported");

    // Vehicle scan: answers with headers, one mask per control unit
    {
        char ids[4][9] = {};
        uint32_t m[4] = {};
        int cnt = 0;
        auto add = [&](const char* id, uint32_t mk) {
            if (cnt < 4) { strcpy(ids[cnt], id); m[cnt++] = mk; }
        };
        elmSupportedPerEcu("7E8064100BE1FA813\r7E906410098180001\r\r>", false, 0x00, add);
        check(cnt == 2 && !strcmp(ids[0], "7E8") && m[0] == 0xBE1FA813u && !strcmp(ids[1], "7E9") &&
                  m[1] == 0x98180001u,
              "scan 11 bit: two control units 7E8 / 7E9");
        cnt = 0;
        elmSupportedPerEcu("SEARCHING...\r18DAF110064100BE1FA813\r18DAF10106410098180001\r>", true, 0x00, add);
        check(cnt == 2 && !strcmp(ids[0], "18DAF110") && !strcmp(ids[1], "18DAF101") && m[1] == 0x98180001u,
              "scan 29 bit: 18DAF110 / 18DAF101");
        cnt = 0;
        elmSupportedPerEcu("7E8100C41850700\r7E82100E2000000AAAA\r>", false, 0x00, add);
        check(cnt == 0, "scan: multi frame lines are no support answer");
        cnt = 0;
        elmSupportedPerEcu("NO DATA\r>", false, 0x00, add);
        check(cnt == 0, "scan: NO DATA");
    }

    // Status texts
    check(elmStatus("NO DATA\r\r>") == ELM_NO_DATA, "status NO DATA");
    check(elmStatus("SEARCHING...\rUNABLE TO CONNECT\r\r>") == ELM_NO_CONNECT, "status UNABLE TO CONNECT");
    check(elmStatus("BUS INIT: ...ERROR\r>") == ELM_NO_CONNECT, "status BUS INIT ERROR");
    check(elmStatus("?\r\r>") == ELM_ERROR, "status ?");
    check(elmStatus("OK\r\r>") == ELM_OK, "status OK");

    // Voltage and protocol
    check(near(elmVoltage("12.6V\r\r>"), 12.6f), "ATRV 12.6V");
    check(near(elmVoltage("\r14.1\r>"), 14.1f), "ATRV without V");
    check(std::isnan(elmVoltage("?\r>")), "ATRV error");
    check(elmProtocol("A6\r\r>") == '6' && elmProtocolIsCan('6'), "ATDPN A6 = CAN");
    check(elmProtocol("3\r>") == '3' && !elmProtocolIsCan('3'), "ATDPN 3 = ISO 9141");
    check(elmProtocol("A\r>") == 'A', "ATDPN A = J1939");

    // Trouble codes
    char codes[8][6];
    size_t total = 0;
    size_t n = obdParseDtcs("43020133023400\r\r>", true, codes, 8, &total);
    check(n == 2 && !strcmp(codes[0], "P0133") && !strcmp(codes[1], "P0234"), "CAN single frame: P0133 P0234");
    n = obdParseDtcs("4300\r>", true, codes, 8);
    check(n == 0, "CAN: no codes");
    n = obdParseDtcs("00A\r0:43040133023441\r1:23C1050000\r\r>", true, codes, 8, &total);
    check(n == 4 && !strcmp(codes[2], "C0123") && !strcmp(codes[3], "U0105") && total == 4,
          "CAN multi frame: 4 codes");
    n = obdParseDtcs("43013302340000\r>", false, codes, 8);
    check(n == 2 && !strcmp(codes[0], "P0133") && !strcmp(codes[1], "P0234"), "ISO 9141: P0133 P0234");
    n = obdParseDtcs("430201330234\r430201330234\r>", true, codes, 8);
    check(n == 2, "same codes from two control units counted once");
    n = obdParseDtcs("NO DATA\r>", true, codes, 8);
    check(n == 0, "NO DATA: no codes");
    n = obdParseDtcs("430301330234C123\r>", true, codes, 2, &total);
    check(n == 2 && total == 3, "more codes than space: total still counted");

    if (fails) {
        printf("%d test(s) failed\n", fails);
        return 1;
    }
    printf("all OBD tests passed\n");
    return 0;
}
