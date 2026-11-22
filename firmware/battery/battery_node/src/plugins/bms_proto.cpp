// SPDX-License-Identifier: GPL-3.0-only AND Apache-2.0
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// Portions from aiobmsble by Patrick Loschmidt (Apache-2.0) - see firmware/battery/LICENSE-Apache-2.0.
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// BMS protocol decoding. Protocol knowledge ported from aiobmsble (Apache-2.0),
// https://github.com/patman15/aiobmsble - see bms_proto.h and firmware/battery/LICENSE-Apache-2.0.
#include "bms_proto.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

const uint8_t ECTIVE_DEV_IDS[4] = {0x22, 0x0B, 0x08, 0x38};

static uint32_t be(const uint8_t* d, size_t n) {
    uint32_t v = 0;
    for (size_t i = 0; i < n; i++) v = (v << 8) | d[i];
    return v;
}

static uint32_t le(const uint8_t* d, size_t n) {
    uint32_t v = 0;
    for (size_t i = n; i > 0; i--) v = (v << 8) | d[i - 1];
    return v;
}

void bmsClear(BmsSample& s) {
    memset(&s, 0, sizeof(s));
    s.voltage = s.current = s.power = s.soc = s.charge_ah = s.energy_wh = NAN;
    s.temperature = s.delta_v = s.runtime_s = NAN;
    s.cycles = -1;
    s.charge_fet = s.discharge_fet = -1;
    s.bat_state = BAT_STATE_UNKNOWN;
}

uint16_t bmsCrcModbus(const uint8_t* d, size_t n) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
    }
    return crc;
}

size_t bmsModbusRead(uint8_t dev, uint16_t addr, uint16_t count, uint8_t out[8]) {
    out[0] = dev;
    out[1] = 0x03;
    out[2] = addr >> 8;
    out[3] = addr & 0xFF;
    out[4] = count >> 8;
    out[5] = count & 0xFF;
    uint16_t crc = bmsCrcModbus(out, 6);
    out[6] = crc & 0xFF;
    out[7] = crc >> 8;
    return 8;
}

int bmsModbusExpectedLen(const uint8_t* d, size_t n, uint8_t head) {
    if (n == 0) return 0;
    if (d[0] != head) return -1;
    if (n < 2) return 0;
    if (d[1] != 0x03) return -1;
    if (n < 3) return 0;
    return d[2] + 5;   // head, function, length, data, 2 bytes CRC
}

bool bmsModbusCrcOk(const uint8_t* d, size_t n) {
    if (n < 5 || (size_t)(d[2] + 5) != n) return false;
    uint16_t crc = bmsCrcModbus(d, n - 2);
    return d[n - 2] == (crc & 0xFF) && d[n - 1] == (crc >> 8);
}

// ============================================================
// Daly
// ============================================================

uint8_t dalyRequestCount(DalyProto p) { return p == DALY_X81 ? 2 : 1; }

void dalyRequest(DalyProto p, uint8_t idx, uint16_t& addr, uint16_t& count) {
    if (p == DALY_X81) {
        addr = idx == 0 ? 0x00 : 0x41;
        count = idx == 0 ? 64 : 62;
    } else {
        addr = 0x00;
        count = 62;
    }
}

uint8_t dalyResponseHead(DalyProto p) { return p == DALY_X81 ? 0x51 : 0xD2; }

// Data positions are relative to the first data byte (after head, function and length)
static void dalyCells(const uint8_t* data, size_t len, uint8_t count, BmsSample& s) {
    s.cell_count = 0;
    for (uint8_t i = 0; i < count && s.cell_count < BMS_MAX_CELLS; i++) {
        size_t p = i * 2;
        if (p + 2 > len) break;
        uint16_t mv = be(data + p, 2);
        if (mv == 0) continue;   // unused cell
        s.cell_v[s.cell_count++] = mv / 1000.0f;
    }
}

static void dalyTemps(const uint8_t* data, size_t len, size_t start, uint8_t count, BmsSample& s) {
    s.temp_count = 0;
    for (uint8_t i = 0; i < count && s.temp_count < BMS_MAX_TEMPS; i++) {
        size_t p = start + i * 2;
        if (p + 2 > len) break;
        uint16_t raw = be(data + p, 2);
        if (raw == 0 || raw >= 0xFF) continue;   // sensor missing
        s.temp_c[s.temp_count++] = (int)raw - 40;
    }
}

bool dalyDecode(DalyProto p, const uint8_t* f1, size_t n1, const uint8_t* f2, size_t n2, BmsSample& s) {
    bmsClear(s);
    const uint8_t head = dalyResponseHead(p);
    if (!f1 || n1 < 5 || f1[0] != head || !bmsModbusCrcOk(f1, n1)) return false;
    const uint8_t* d1 = f1 + 3;
    const size_t l1 = n1 - 5;

    if (p == DALY_D2) {
        if (l1 < 124) return false;                      // 62 registers
        s.voltage   = be(d1 + 80, 2) / 10.0f;
        s.current   = ((int)be(d1 + 82, 2) - 30000) / 10.0f;
        s.soc       = be(d1 + 84, 2) / 10.0f;
        s.charge_ah = be(d1 + 96, 2) / 10.0f;
        uint8_t cells = be(d1 + 98, 2) > 32 ? 32 : be(d1 + 98, 2);
        uint8_t temps = be(d1 + 100, 2) > 8 ? 8 : be(d1 + 100, 2);
        s.cycles    = be(d1 + 102, 2);
        s.charge_fet    = be(d1 + 106, 2) ? 1 : 0;
        s.discharge_fet = be(d1 + 108, 2) ? 1 : 0;
        s.problem_code = 0;
        for (int i = 0; i < 8; i++) s.problem_code = (s.problem_code << 8) | d1[116 + i];
        s.problem_fmt = 1;
        dalyCells(d1, l1, cells, s);
        dalyTemps(d1, l1, 32 * 2, temps, s);
    } else {
        if (l1 < 128 || !f2 || n2 < 5 || f2[0] != head || !bmsModbusCrcOk(f2, n2)) return false;
        const uint8_t* d2 = f2 + 3;
        const size_t l2 = n2 - 5;
        if (l2 < 124) return false;
        s.voltage   = be(d1 + 112, 2) / 10.0f;
        s.current   = ((int)be(d1 + 114, 2) - 30000) / 10.0f;
        s.soc       = be(d1 + 116, 2) / 10.0f;
        uint8_t cells = be(d1 + 120, 2) > 48 ? 48 : be(d1 + 120, 2);
        uint8_t temps = be(d1 + 122, 2) > 8 ? 8 : be(d1 + 122, 2);
        s.charge_ah = be(d2 + 20, 2) / 10.0f;
        s.cycles    = be(d2 + 22, 2);
        s.charge_fet    = be(d2 + 34, 2) ? 1 : 0;
        s.discharge_fet = be(d2 + 36, 2) ? 1 : 0;
        s.problem_code  = be(d2 + 88, 4);
        dalyCells(d1, l1, cells, s);
        dalyTemps(d1, l1, 48 * 2, temps, s);
    }
    s.problem = s.problem_code != 0;
    bmsFinish(s);
    return true;
}

// ============================================================
// Ective BM X / LiPower (registers 0x0400 ... 0x0407)
// Positions below are in the whole frame (head, function, length come first).
// ============================================================

bool ectiveDecode(const uint8_t* f, size_t n, BmsSample& s) {
    bmsClear(s);
    if (n < 17 || f[1] != 0x03 || !bmsModbusCrcOk(f, n)) return false;
    bool known = false;
    for (uint8_t id : ECTIVE_DEV_IDS) known |= (f[0] == id);
    if (!known) return false;

    s.charge_ah = be(f + 3, 2);                            // Ah
    s.soc       = be(f + 5, 2);                            // %
    uint32_t rt = be(f + 7, 4);                            // hours (high word), minutes (low word)
    float runtime = (rt >> 16) * 3600.0f + (rt & 0xFFFF) * 60.0f;
    // Byte 12 = direction (1 = discharging), bytes 13-14 = current in 10 mA
    float amps = be(f + 13, 2) / 100.0f;
    s.current   = (f[12] & 0x01) ? -amps : amps;
    s.voltage   = be(f + 15, 2) / 10.0f;
    bmsFinish(s);
    // The monitor reports its own runtime estimate; use it while discharging
    if (s.current < 0 && runtime > 0) s.runtime_s = runtime;
    return true;
}

// ============================================================
// BM2
// ============================================================

const uint8_t BM2_AES_KEY[16] = {0x6C, 0x65, 0x61, 0x67, 0x65, 0x6E, 0x64, 0xFF,
                                 0xFE, 0x31, 0x38, 0x38, 0x32, 0x34, 0x36, 0x36};  // "leagend" FF FE "1882466"

bool bm2Decode(const uint8_t p[16], BmsSample& s) {
    bmsClear(s);
    if (p[0] != 0xF5) return false;
    uint16_t raw = ((uint16_t)p[1] << 4) | (p[2] >> 4);   // 12 bit, 10 mV steps
    if (raw == 0 || raw > 2000) return false;             // 0 ... 20 V plausible
    s.voltage = raw / 100.0f;
    s.soc = p[3] > 100 ? 100 : p[3];
    s.bat_state = s.voltage >= BM2_V_CHARGING ? BAT_STATE_CHARGING
                : s.voltage >= BM2_V_FLOATING ? BAT_STATE_FLOATING
                : s.voltage <= BM2_V_CRITICAL ? BAT_STATE_CRITICAL
                : s.voltage <= BM2_V_LOW      ? BAT_STATE_LOW
                :                               BAT_STATE_NORMAL;
    bmsFinish(s);
    return true;
}

// ============================================================
// JBD / Xiaoxiang (aiobmsble jbd_bms.py)
// ============================================================

size_t jbdRequest(uint8_t cmd, uint8_t out[7]) {
    uint16_t crc = (uint16_t)(0x10000 - (cmd + 0));   // over command and length byte
    out[0] = 0xDD;
    out[1] = 0xA5;
    out[2] = cmd;
    out[3] = 0x00;
    out[4] = crc >> 8;
    out[5] = crc & 0xFF;
    out[6] = 0x77;
    return 7;
}

int jbdExpectedLen(const uint8_t* d, size_t n) {
    if (n == 0) return 0;
    if (d[0] != 0xDD) return -1;
    if (n < 2) return 0;
    if (d[1] != JBD_CMD_BASIC && d[1] != JBD_CMD_CELLS && d[1] != 0x05) return -1;
    if (n < 4) return 0;
    return 7 + d[3];
}

bool jbdFrameOk(const uint8_t* d, size_t n, uint8_t cmd) {
    if (n < 7 || jbdExpectedLen(d, n) != (int)n) return false;
    if (d[n - 1] != 0x77 || d[1] != cmd || (d[2] & 0x80)) return false;
    uint32_t sum = 0;
    for (size_t i = 2; i < n - 3; i++) sum += d[i];
    uint16_t crc = (uint16_t)(0x10000 - sum);
    return d[n - 3] == (crc >> 8) && d[n - 2] == (crc & 0xFF);
}

bool jbdDecodeBasic(const uint8_t* d, size_t n, BmsSample& s) {
    bmsClear(s);
    if (!jbdFrameOk(d, n, JBD_CMD_BASIC) || n < 27 + 3) return false;
    uint8_t temps = d[26];
    size_t ctrl = 27 + 2 * (size_t)temps;
    bool d10 = ctrl < n - 3 && (d[ctrl] & 0x80);        // 100 mA / 100 mAh units
    s.voltage = be(d + 4, 2) / 100.0f;
    s.current = (int16_t)be(d + 6, 2) / (d10 ? 10.0f : 100.0f);
    s.charge_ah = be(d + 8, 2) / (d10 ? 10.0f : 100.0f);
    s.cycles = be(d + 12, 2);
    s.problem_code = be(d + 20, 2);
    s.problem_fmt = 2;
    s.problem = s.problem_code != 0;
    s.soc = d[23];
    s.charge_fet = (d[24] & 0x01) ? 1 : 0;
    s.discharge_fet = (d[24] & 0x02) ? 1 : 0;
    s.temp_count = 0;
    for (uint8_t i = 0; i < temps && s.temp_count < BMS_MAX_TEMPS; i++) {
        size_t p = 27 + 2 * (size_t)i;
        if (p + 2 > n - 3) break;
        uint16_t raw = be(d + p, 2);
        if (raw == 0) continue;                          // sensor missing
        s.temp_c[s.temp_count++] = ((int)raw - 2731) / 10.0f;
    }
    s.cell_count = 0;                                    // cells come with command 0x04
    bmsFinish(s);
    return true;
}

bool jbdDecodeCells(const uint8_t* d, size_t n, BmsSample& s) {
    if (!jbdFrameOk(d, n, JBD_CMD_CELLS)) return false;
    size_t cells = d[3] / 2;
    s.cell_count = 0;
    for (size_t i = 0; i < cells && s.cell_count < BMS_MAX_CELLS; i++) {
        uint16_t mv = be(d + 4 + 2 * i, 2);
        if (mv == 0) continue;
        s.cell_v[s.cell_count++] = mv / 1000.0f;
    }
    bmsFinish(s);
    return true;
}

// ============================================================
// JK-BMS JK02 (aiobmsble jikong_bms.py)
// ============================================================

size_t jkCommand(uint8_t cmd, uint8_t out[20]) {
    memset(out, 0, 20);
    out[0] = 0xAA;
    out[1] = 0x55;
    out[2] = 0x90;
    out[3] = 0xEB;
    out[4] = cmd;
    out[5] = 0x00;                                       // no value
    uint8_t sum = 0;
    for (int i = 0; i < 19; i++) sum += out[i];
    out[19] = sum;
    return 20;
}

bool jkFrameOk(const uint8_t* d, size_t n) {
    if (n != JK_FRAME_LEN) return false;
    if (d[0] != 0x55 || d[1] != 0xAA || d[2] != 0xEB || d[3] != 0x90) return false;
    uint8_t sum = 0;
    for (size_t i = 0; i < n - 1; i++) sum += d[i];
    return sum == d[n - 1];
}

int jkSwVersion(const uint8_t* d, size_t n) {
    if (!jkFrameOk(d, n) || d[4] != 0x03) return -1;
    // bytes 30..37: text such as "11.XW" or "V19.05"; leading digits = major version
    int v = -1;
    for (size_t i = 30; i < 38; i++) {
        char c = (char)d[i];
        if (c >= '0' && c <= '9') v = (v < 0 ? 0 : v * 10) + (c - '0');
        else if (v >= 0) break;
    }
    return v;
}

bool jkDecode(const uint8_t* d, size_t n, int sw, BmsSample& s) {
    bmsClear(s);
    if (!jkFrameOk(d, n) || d[4] != 0x02) return false;
    const int off = (sw >= 0 && sw < 11) ? -32 : 0;      // older 24S layout
    const int h = off / 2;                               // cell area moves by half
    s.voltage = le(d + 150 + off, 4) / 1000.0f;
    s.current = (int32_t)le(d + 158 + off, 4) / 1000.0f;
    uint32_t prb = le(d + 166 + off, 4);
    if (off) prb >>= 16;
    s.problem_code = prb;
    s.problem_fmt = 3;
    s.soc = d[173 + off];
    s.charge_ah = le(d + 174 + off, 4) / 1000.0f;
    s.cycles = le(d + 182 + off, 4);
    s.charge_fet = d[198 + off] ? 1 : 0;
    s.discharge_fet = d[199 + off] ? 1 : 0;

    uint32_t mask = le(d + 70 + h, 4);
    uint8_t cells = 0;
    for (int b = 0; b < 32; b++) cells += (mask >> b) & 1;
    s.cell_count = 0;
    for (uint8_t i = 0; i < cells && s.cell_count < BMS_MAX_CELLS; i++) {
        uint16_t mv = le(d + 6 + 2 * i, 2);
        if (mv == 0) continue;
        s.cell_v[s.cell_count++] = mv / 1000.0f;
    }

    // Temperature sensors: positions depend on the firmware, bit mask at 214
    uint8_t tmask = d[214 + off];
    static const uint16_t POS_V11[6] = {144, 162, 164, 254, 256, 258};
    static const uint16_t POS_OLD[3] = {130, 132, 134};
    const uint16_t* pos = sw >= 11 ? POS_V11 : POS_OLD;
    uint8_t npos = sw >= 14 ? 6 : sw >= 11 ? 4 : 3;
    s.temp_count = 0;
    for (uint8_t i = 0; i < npos && s.temp_count < BMS_MAX_TEMPS; i++) {
        if (!(tmask & (1 << i))) continue;
        int16_t raw = (int16_t)le(d + pos[i], 2);
        if (raw == -2000) continue;                      // not fitted
        s.temp_c[s.temp_count++] = raw / 10.0f;
    }
    // Bit 4 = "fully charged" is only information, not a problem
    s.problem = (s.problem_code & ~(uint64_t)(1u << 4)) != 0;
    bmsFinish(s);
    return true;
}

// ============================================================
// LiTime / Redodo / PowerQueen (aiobmsble redodo_bms.py)
// ============================================================

const uint8_t LITIME_REQUEST[8] = {0x00, 0x00, 0x04, 0x01, 0x13, 0x55, 0xAA, 0x17};

int litimeExpectedLen(const uint8_t* d, size_t n) {
    if (n == 0) return 0;
    if (d[0] != 0x00) return -1;
    if (n < 2) return 0;
    if (d[1] != 0x00) return -1;
    if (n < 3) return 0;
    return d[2] + 4;
}

bool litimeDecode(const uint8_t* d, size_t n, BmsSample& s) {
    bmsClear(s);
    if (n < 100 || litimeExpectedLen(d, n) != (int)n) return false;
    uint8_t sum = 0;
    for (size_t i = 0; i < n - 1; i++) sum += d[i];
    if (sum != d[n - 1]) return false;
    s.voltage = le(d + 12, 2) / 1000.0f;
    s.current = (int32_t)le(d + 48, 4) / 1000.0f;
    s.soc = le(d + 90, 2);
    s.charge_ah = le(d + 62, 2) / 100.0f;
    s.cycles = le(d + 96, 4);
    uint64_t prb = 0;
    for (int i = 7; i >= 0; i--) prb = (prb << 8) | d[76 + i];
    s.problem_code = prb;
    s.problem = prb != 0;
    s.cell_count = 0;
    for (uint8_t i = 0; i < 16 && s.cell_count < BMS_MAX_CELLS; i++) {
        uint16_t mv = le(d + 16 + 2 * i, 2);
        if (mv == 0) continue;
        s.cell_v[s.cell_count++] = mv / 1000.0f;
    }
    // Up to 5 sensors; the first always counts, further ones once they report a value
    int16_t t[5];
    uint8_t used = 1;
    for (uint8_t i = 0; i < 5; i++) {
        t[i] = (int16_t)le(d + 52 + 2 * i, 2);
        if (i > 0 && t[i] != 0) used = i + 1;
    }
    s.temp_count = 0;
    for (uint8_t i = 0; i < used && s.temp_count < BMS_MAX_TEMPS; i++) s.temp_c[s.temp_count++] = t[i];
    bmsFinish(s);
    return true;
}

// ============================================================
// Derived values
// ============================================================

void bmsFinish(BmsSample& s) {
    if (!isnan(s.voltage) && !isnan(s.current)) s.power = s.voltage * s.current;
    if (!isnan(s.voltage) && !isnan(s.charge_ah)) s.energy_wh = s.voltage * s.charge_ah;

    if (s.cell_count > 1) {
        float lo = s.cell_v[0], hi = s.cell_v[0];
        for (uint8_t i = 1; i < s.cell_count; i++) {
            if (s.cell_v[i] < lo) lo = s.cell_v[i];
            if (s.cell_v[i] > hi) hi = s.cell_v[i];
        }
        s.delta_v = hi - lo;
    }
    if (s.temp_count > 0) {
        float sum = 0;
        for (uint8_t i = 0; i < s.temp_count; i++) sum += s.temp_c[i];
        s.temperature = sum / s.temp_count;
    }
    // Runtime from remaining capacity while discharging
    if (!isnan(s.current) && s.current < -0.01f && !isnan(s.charge_ah) && s.charge_ah > 0) {
        s.runtime_s = s.charge_ah / -s.current * 3600.0f;
    }
}

static void bm2State(BmsSample& s) {
    s.bat_state = s.voltage >= BM2_V_CHARGING ? BAT_STATE_CHARGING
                : s.voltage >= BM2_V_FLOATING ? BAT_STATE_FLOATING
                : s.voltage <= BM2_V_CRITICAL ? BAT_STATE_CRITICAL
                : s.voltage <= BM2_V_LOW      ? BAT_STATE_LOW
                :                               BAT_STATE_NORMAL;
}

bool bm2DecodeAdv(const uint8_t p[16], BmsSample& s) {
    bmsClear(s);
    uint16_t raw = ((uint16_t)p[6] << 8) | p[7];
    if (raw < 500 || raw > 2000) return false;             // 5 ... 20 V plausible
    if (p[8] > 100) return false;
    s.voltage = raw / 100.0f;
    s.soc = p[8];
    bm2State(s);
    bmsFinish(s);
    return true;
}

const char* batStateName(int8_t st) {
    switch (st) {
        case BAT_STATE_CRITICAL: return "Kritisch";
        case BAT_STATE_LOW:      return "Schwach";
        case BAT_STATE_NORMAL:   return "Normal";
        case BAT_STATE_CHARGING: return "Lädt";
        case BAT_STATE_FLOATING: return "Erhaltungsladung";
        default:                 return nullptr;
    }
}

const char* bmsTypeName(uint8_t t) {
    switch (t) {
        case BMS_DALY:   return "Daly";
        case BMS_ECTIVE: return "Ective BM X";
        case BMS_BM2:    return "BM2";
        case BMS_JBD:    return "JBD";
        case BMS_JK:     return "JK-BMS";
        case BMS_LITIME: return "LiTime";
        default:         return "-";
    }
}

// ============================================================
// Problem text
// ============================================================

// Daly fault bytes, indexed like the 0x98 "failure status" answer of the Daly UART protocol.
// Level 1 = warning, level 2 = protection (the BMS switches the MOSFETs off).
// In the MODBUS registers 0x3A ... 0x3D the two bytes of each register are swapped against
// the UART order: register 0x3A = temperatures (high byte), voltages (low byte), and so on
// (same as syssi/esphome-daly-bms). See bmsProblemText().
static const char* const DALY_FAULTS[7][8] = {
    {"Zellspannung zu hoch (Stufe 1)", "Zellspannung zu hoch (Stufe 2)", "Zellspannung zu niedrig (Stufe 1)", "Zellspannung zu niedrig (Stufe 2)",
     "Gesamtspannung zu hoch (Stufe 1)", "Gesamtspannung zu hoch (Stufe 2)", "Gesamtspannung zu niedrig (Stufe 1)", "Gesamtspannung zu niedrig (Stufe 2)"},
    {"Ladetemperatur zu hoch (Stufe 1)", "Ladetemperatur zu hoch (Stufe 2)", "Ladetemperatur zu niedrig (Stufe 1)", "Ladetemperatur zu niedrig (Stufe 2)",
     "Entladetemperatur zu hoch (Stufe 1)", "Entladetemperatur zu hoch (Stufe 2)", "Entladetemperatur zu niedrig (Stufe 1)", "Entladetemperatur zu niedrig (Stufe 2)"},
    {"Ladestrom zu hoch (Stufe 1)", "Ladestrom zu hoch (Stufe 2)", "Entladestrom zu hoch (Stufe 1)", "Entladestrom zu hoch (Stufe 2)",
     "Ladezustand zu hoch (Stufe 1)", "Ladezustand zu hoch (Stufe 2)", "Ladezustand zu niedrig (Stufe 1)", "Ladezustand zu niedrig (Stufe 2)"},
    {"Zellen ungleich (Stufe 1)", "Zellen ungleich (Stufe 2)", "Temperaturen ungleich (Stufe 1)", "Temperaturen ungleich (Stufe 2)",
     nullptr, nullptr, nullptr, nullptr},
    {"Lade-MOSFET zu heiß", "Entlade-MOSFET zu heiß", "Temperaturfühler Lade-MOSFET defekt", "Temperaturfühler Entlade-MOSFET defekt",
     "Lade-MOSFET klebt", "Entlade-MOSFET klebt", "Lade-MOSFET unterbrochen", "Entlade-MOSFET unterbrochen"},
    {"Messchip (AFE) defekt", "Zellspannungsmessung ausgefallen", "Zelltemperaturfühler defekt", "EEPROM-Fehler",
     "Uhr (RTC) defekt", "Vorladung fehlgeschlagen", "Kommunikationsfehler", "interner Kommunikationsfehler"},
    {"Strommessung defekt", "Gesamtspannungsmessung defekt", "Kurzschlussschutz ausgelöst", "Laden gesperrt (Spannung zu niedrig)",
     nullptr, nullptr, nullptr, nullptr},
};

// JBD protection status (register 0x03, bytes 20-21), bits 14 and 15 undocumented
static const char* const JBD_FAULTS[16] = {
    "Zellspannung zu hoch", "Zellspannung zu niedrig", "Gesamtspannung zu hoch", "Gesamtspannung zu niedrig",
    "Ladetemperatur zu hoch", "Ladetemperatur zu niedrig", "Entladetemperatur zu hoch", "Entladetemperatur zu niedrig",
    "Ladestrom zu hoch", "Entladestrom zu hoch", "Kurzschluss", "Messchip (AFE) defekt",
    "MOSFET per Software gesperrt", "Ladezeit überschritten", nullptr, nullptr,
};
// JK02 alarm bits; bit 4 ("fully charged") is skipped in bmsProblemText()
static const char* const JK_FAULTS[32] = {
    "Leitungswiderstand zu hoch", "MOSFET zu heiß", "Zellenzahl passt nicht zur Einstellung", nullptr,
    nullptr, "Gesamtspannung zu hoch", "Ladestrom zu hoch", "Kurzschluss beim Laden",
    "Ladetemperatur zu hoch", "Ladetemperatur zu niedrig", "Kommunikationsfehler", "Zellspannung zu niedrig",
    "Gesamtspannung zu niedrig", "Entladestrom zu hoch", "Kurzschluss beim Entladen", "Entladetemperatur zu hoch",
    "Lade-MOSFET defekt", "Entlade-MOSFET defekt", "GPS getrennt", "Passwort ändern",
    "Entladen einschalten fehlgeschlagen", "Batterie zu heiß", "Temperaturfühler defekt", "Parallelmodul gestört",
    "Kurzschlussschutz lässt sich nicht lösen", "Entladestrom zu hoch (Stufe 2)", "Entladestrom zu hoch (Stufe 3)", "Entladetemperatur zu niedrig",
    "Per GPS gesperrt", nullptr, nullptr, nullptr,
};

size_t bmsProblemText(const BmsSample& s, char* out, size_t n) {
    if (!n) return 0;
    out[0] = 0;
    if (!s.problem) return 0;
    size_t len = 0;
    uint64_t unknown = 0;
    bool full = false;
    // whole entries only (no cut UTF-8 characters); "..." once nothing more fits
    auto add = [&](const char* t) {
        if (full) return;
        size_t need = strlen(t) + (len ? 2 : 0);
        if (len + need + 1 > n) {
            if (len + 4 < n) { memcpy(out + len, " ...", 4); len += 4; out[len] = 0; }
            full = true;
            return;
        }
        if (len) { out[len++] = ','; out[len++] = ' '; }
        while (*t) out[len++] = *t++;
        out[len] = 0;
    };
    if (s.problem_fmt == 1) {
        for (int byte = 0; byte < 8; byte++) {         // byte 0 = first byte of register 0x3A
            uint8_t v = (s.problem_code >> (8 * (7 - byte))) & 0xFF;
            int uart = byte ^ 1;                         // swapped bytes per register, see DALY_FAULTS
            for (int bit = 0; bit < 8; bit++) {
                if (!(v & (1 << bit))) continue;
                const char* t = uart < 7 ? DALY_FAULTS[uart][bit] : nullptr;
                if (t) add(t);
                else unknown |= (uint64_t)1 << (8 * (7 - byte) + bit);
            }
        }
    } else if (s.problem_fmt == 2 || s.problem_fmt == 3) {
        const char* const* tab = s.problem_fmt == 2 ? JBD_FAULTS : JK_FAULTS;
        int bits = s.problem_fmt == 2 ? 16 : 32;
        for (int bit = 0; bit < bits; bit++) {
            if (!(s.problem_code & ((uint64_t)1 << bit))) continue;
            if (s.problem_fmt == 3 && bit == 4) continue;   // "fully charged": no problem
            if (tab[bit]) add(tab[bit]);
            else unknown |= (uint64_t)1 << bit;
        }
    } else {
        unknown = s.problem_code;
    }
    if (unknown || !len) {
        char hex[32];
        snprintf(hex, sizeof(hex), "Fehlercode 0x%llX", (unsigned long long)(unknown ? unknown : s.problem_code));
        add(hex);
    }
    return len;
}
