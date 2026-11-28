// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Message format of the Truma iNet X panel and the Alde values on its bus, see inetx_proto.h.
#include "inetx_proto.h"
#include <string.h>
#include <stdio.h>

// ============================================================
// CBOR reader
// ============================================================

void cborInBegin(CborIn& r, const uint8_t* buf, size_t len) {
    r.p = buf;
    r.end = buf + len;
    r.ok = true;
}

static bool take(CborIn& r, size_t n) {
    if (!r.ok || (size_t)(r.end - r.p) < n) { r.ok = false; return false; }
    r.p += n;
    return true;
}

bool cborHead(CborIn& r, uint8_t& major, uint64_t& arg, uint8_t& info) {
    if (!r.ok || r.p >= r.end) { r.ok = false; return false; }
    uint8_t b = *r.p++;
    major = b >> 5;
    info = b & 0x1F;
    if (info < 24) { arg = info; return true; }
    int n = info == 24 ? 1 : info == 25 ? 2 : info == 26 ? 4 : info == 27 ? 8 : 0;
    if (n == 0) {
        // 31 = indefinite length: the panel does not use it; 28..30 are reserved
        r.ok = false;
        return false;
    }
    const uint8_t* q = r.p;
    if (!take(r, n)) return false;
    arg = 0;
    for (int k = 0; k < n; k++) arg = (arg << 8) | q[k];
    return true;
}

static bool skipDepth(CborIn& r, int depth) {
    if (depth > 16) { r.ok = false; return false; }
    uint8_t major, info;
    uint64_t arg;
    if (!cborHead(r, major, arg, info)) return false;
    switch (major) {
        case 0: case 1: case 7: return true;          // ints, simple values, floats (argument read)
        case 2: case 3: return arg <= 65535 && take(r, (size_t)arg);
        case 4:
            if (arg > 4096) { r.ok = false; return false; }
            for (uint64_t k = 0; k < arg; k++) if (!skipDepth(r, depth + 1)) return false;
            return true;
        case 5:
            if (arg > 4096) { r.ok = false; return false; }
            for (uint64_t k = 0; k < 2 * arg; k++) if (!skipDepth(r, depth + 1)) return false;
            return true;
        case 6: return skipDepth(r, depth + 1);        // tag: skip the tagged item
    }
    r.ok = false;
    return false;
}

bool cborSkip(CborIn& r) {
    return skipDepth(r, 0);
}

bool cborReadText(CborIn& r, char* out, size_t size) {
    uint8_t major, info;
    uint64_t arg;
    const uint8_t* start = r.p;
    if (!cborHead(r, major, arg, info)) return false;
    if (major != 3) {
        r.p = start;                                   // leave it for cborSkip()
        if (size) out[0] = 0;
        return false;
    }
    const uint8_t* s = r.p;
    if (arg > 65535 || !take(r, (size_t)arg)) return false;
    size_t n = (size_t)arg < size - 1 ? (size_t)arg : size - 1;
    memcpy(out, s, n);
    out[n] = 0;
    return true;
}

static double halfToDouble(uint16_t h) {
    int e = (h >> 10) & 0x1F, m = h & 0x3FF;
    double v = e == 0 ? ldexp(m, -24) : e == 31 ? (m ? NAN : INFINITY) : ldexp(m + 1024, e - 25);
    return (h & 0x8000) ? -v : v;
}

bool cborReadValue(CborIn& r, CborValue& v) {
    memset(&v, 0, sizeof(v));
    const uint8_t* start = r.p;
    uint8_t major, info;
    uint64_t arg;
    if (!cborHead(r, major, arg, info)) return false;
    switch (major) {
        case 0: v.kind = CBK_NUM; v.num = (double)arg; return true;
        case 1: v.kind = CBK_NUM; v.num = -1.0 - (double)arg; return true;
        case 3:
            v.kind = CBK_TEXT;
            v.text = r.p;
            v.count = (uint32_t)arg;
            return arg <= 65535 && take(r, (size_t)arg);
        case 4: case 5:
            v.kind = major == 4 ? CBK_ARRAY : CBK_MAP;
            v.count = (uint32_t)arg;
            r.p = start;
            return cborSkip(r);
        case 7:
            if (info == 20 || info == 21) { v.kind = CBK_NUM; v.num = info == 21 ? 1 : 0; return true; }
            if (info == 22 || info == 23) { v.kind = CBK_NULL; return true; }
            if (info == 25) { v.kind = CBK_NUM; v.num = halfToDouble((uint16_t)arg); return true; }
            if (info == 26) {
                uint32_t b = (uint32_t)arg;
                float f;
                memcpy(&f, &b, 4);
                v.kind = CBK_NUM;
                v.num = f;
                return true;
            }
            if (info == 27) {
                double d;
                memcpy(&d, &arg, 8);
                v.kind = CBK_NUM;
                v.num = d;
                return true;
            }
            v.kind = CBK_NONE;
            return true;
        default:                                       // bytes, tags: not used for values here
            r.p = start;
            v.kind = CBK_NONE;
            return cborSkip(r);
    }
}

// ============================================================
// CBOR writer
// ============================================================

void cborBegin(CborOut& o, uint8_t* buf, size_t cap) {
    o.buf = buf;
    o.cap = cap;
    o.len = 0;
    o.ok = true;
}

static void put(CborOut& o, uint8_t b) {
    if (o.len >= o.cap) { o.ok = false; return; }
    o.buf[o.len++] = b;
}

static void head(CborOut& o, uint8_t major, uint64_t arg) {
    uint8_t m = (uint8_t)(major << 5);
    if (arg < 24) { put(o, m | (uint8_t)arg); return; }
    int n = arg <= 0xFF ? 1 : arg <= 0xFFFF ? 2 : arg <= 0xFFFFFFFFULL ? 4 : 8;
    put(o, m | (uint8_t)(n == 1 ? 24 : n == 2 ? 25 : n == 4 ? 26 : 27));
    for (int k = n - 1; k >= 0; k--) put(o, (uint8_t)(arg >> (8 * k)));
}

void cborMap(CborOut& o, uint32_t entries) { head(o, 5, entries); }
void cborArray(CborOut& o, uint32_t entries) { head(o, 4, entries); }

void cborTextOut(CborOut& o, const char* s) {
    size_t n = strlen(s);
    head(o, 3, n);
    for (size_t k = 0; k < n; k++) put(o, (uint8_t)s[k]);
}

void cborInt(CborOut& o, int64_t v) {
    if (v >= 0) head(o, 0, (uint64_t)v);
    else head(o, 1, (uint64_t)(-1 - v));
}

// ============================================================
// Frames
// ============================================================

size_t inetxFrame(uint8_t* out, size_t cap, uint16_t dst, uint16_t src, uint8_t ctrl, uint8_t sub,
                  uint8_t corr, const uint8_t* body, size_t bodyLen) {
    size_t total = INETX_HDR + bodyLen;
    if (total > cap) return 0;
    uint16_t size = (uint16_t)(bodyLen + 2 + 9);       // sub type + correlation id + body, plus segmentation header
    out[0] = dst & 0xFF; out[1] = dst >> 8;
    out[2] = src & 0xFF; out[3] = src >> 8;
    out[4] = size & 0xFF; out[5] = size >> 8;
    out[6] = ctrl;
    memset(out + 7, 0, 9);                             // not segmented
    out[16] = sub;
    out[17] = corr;
    if (bodyLen) memcpy(out + INETX_HDR, body, bodyLen);
    return total;
}

size_t inetxRegisterFrame(uint8_t* out, size_t cap, uint16_t src) {
    uint8_t b[16];
    CborOut o;
    cborBegin(o, b, sizeof(b));
    cborMap(o, 1);
    cborTextOut(o, "pv");                              // protocol version 5.1
    cborArray(o, 2);
    cborInt(o, 5);
    cborInt(o, 1);
    return o.ok ? inetxFrame(out, cap, INETX_BROADCAST, src, INETX_CTRL_REG, 0x01, 0x42, b, o.len) : 0;
}

size_t inetxSubscribeFrame(uint8_t* out, size_t cap, uint16_t src, const char* const* topics, uint8_t count) {
    uint8_t b[256];
    CborOut o;
    cborBegin(o, b, sizeof(b));
    cborMap(o, 1);
    cborTextOut(o, "tn");
    cborArray(o, count);
    for (uint8_t k = 0; k < count; k++) cborTextOut(o, topics[k]);
    return o.ok ? inetxFrame(out, cap, INETX_BROKER, src, INETX_CTRL_MBP, INETX_MBP_SUB, 0, b, o.len) : 0;
}

size_t inetxDiscoverFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst) {
    return inetxFrame(out, cap, dst, src, INETX_CTRL_MBP, INETX_MBP_DISC, 0, nullptr, 0);
}

static size_t writeFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst, const char* topic,
                         const char* param, const int64_t* iv, const char* sv) {
    uint8_t b[160];
    CborOut o;
    cborBegin(o, b, sizeof(b));
    cborMap(o, 4);
    cborTextOut(o, "tn"); cborTextOut(o, topic);
    cborTextOut(o, "pn"); cborTextOut(o, param);
    cborTextOut(o, "v");
    if (iv) cborInt(o, *iv); else cborTextOut(o, sv);
    cborTextOut(o, "id"); cborInt(o, 0);
    return o.ok ? inetxFrame(out, cap, dst, src, INETX_CTRL_MBP, INETX_MBP_WRITE, 0, b, o.len) : 0;
}

size_t inetxWriteIntFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst, const char* topic, const char* param, int64_t v) {
    return writeFrame(out, cap, src, dst, topic, param, &v, nullptr);
}

size_t inetxWriteTextFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst, const char* topic, const char* param, const char* v) {
    return writeFrame(out, cap, src, dst, topic, param, nullptr, v);
}

size_t inetxLastMessageFrame(uint8_t* out, size_t cap, uint16_t src) {
    uint8_t b[24];
    CborOut o;
    cborBegin(o, b, sizeof(b));
    cborMap(o, 1);
    cborTextOut(o, "LastMessage");                     // end of the identity sequence
    cborInt(o, 1);
    return o.ok ? inetxFrame(out, cap, INETX_PANEL, src, INETX_CTRL_MBP, INETX_MBP_WRITE, 0, b, o.len) : 0;
}

bool inetxParseHeader(const uint8_t* d, size_t len, InetxHeader& h) {
    if (len < 16) return false;
    h.dst = d[0] | (d[1] << 8);
    h.src = d[2] | (d[3] << 8);
    h.size = d[4] | (d[5] << 8);
    h.ctrl = d[6];
    h.sub = len > 16 ? d[16] : 0;
    h.corr = len > 17 ? d[17] : 0;
    h.body = len > INETX_HDR ? d + INETX_HDR : nullptr;
    h.bodyLen = len > INETX_HDR ? len - INETX_HDR : 0;
    return true;
}

// ============================================================
// Alde values
// ============================================================

void inetxModelClear(InetxModel& m) {
    memset(&m, 0, sizeof(m));
    m.target = m.room = m.outdoor = m.gas = m.el_level = NAN;
    m.water_on = m.water_boost = m.heating = m.room_mode = m.error = NAN;
}

// Returns 1 if *dst changed
static int setVal(float& dst, double v) {
    if (isnan(v)) return 0;
    float f = (float)v;
    if (!isnan(dst) && dst == f) return 0;
    dst = f;
    return 1;
}

static double num(const CborValue& v) {
    return v.kind == CBK_NUM ? v.num : NAN;
}

// One topic.parameter = value from device src
static int takeParam(InetxModel& m, uint16_t src, const char* tn, const char* pn, const CborValue& v) {
    int ch = 0;
    double n = num(v);
    if (!strcmp(tn, "AirHeating")) {
        if (!strcmp(pn, "TgtTemp")) { ch += setVal(m.target, n / 10.0); if (!isnan(n)) m.heaterAddr = src; }
        else if (!strcmp(pn, "Temp")) ch += setVal(m.room, n / 10.0);
        else if (!strcmp(pn, "ExtTemp")) { ch += setVal(m.outdoor, n / 10.0); if (!isnan(n)) m.outdoorFromHeater = true; }
        else if (!strcmp(pn, "Active")) ch += setVal(m.heating, isnan(n) ? NAN : (n == 1 ? 1.0 : 0.0));
    } else if (!strcmp(tn, "Temperature")) {
        // The panel's copy of the outdoor sensor: only if the heater has not sent its own
        if (!strcmp(pn, "External") && !m.outdoorFromHeater) ch += setVal(m.outdoor, n / 10.0);
    } else if (!strcmp(tn, "EnergySrc")) {
        if (!strcmp(pn, "GasLevel")) { ch += setVal(m.gas, isnan(n) ? NAN : (n != 0 ? 1.0 : 0.0)); if (!isnan(n)) m.energyAddr = src; }
        else if (!strcmp(pn, "ElectricLevel")) { ch += setVal(m.el_level, n); if (!isnan(n)) m.energyAddr = src; }
    } else if (!strcmp(tn, "WaterHeating")) {
        if (!strcmp(pn, "Active")) { ch += setVal(m.water_on, isnan(n) ? NAN : (n != 0 ? 1.0 : 0.0)); if (!isnan(n)) m.waterAddr = src; }
        else if (!strcmp(pn, "BoostMode")) { ch += setVal(m.water_boost, isnan(n) ? NAN : (n != 0 ? 1.0 : 0.0)); if (!isnan(n)) m.waterAddr = src; }
    } else if (!strcmp(tn, "RoomClimate")) {
        if (!strcmp(pn, "Mode")) { ch += setVal(m.room_mode, n); if (!isnan(n)) m.panelAddr = src; }
    } else if (!strcmp(tn, "ErrorReset")) {
        // List of the codes currently raised; empty = no fault. Only the heater's list counts
        // once the heater is known, so a panel fault does not hide a healthy heater.
        if (!strcmp(pn, "ErrCode") && v.kind == CBK_ARRAY && (m.heaterAddr == 0 || src == m.heaterAddr))
            ch += setVal(m.error, v.count ? 1.0 : 0.0);
    } else if (!strcmp(tn, "Identify")) {
        if (!strcmp(pn, "Supplier") && v.kind == CBK_TEXT && src != INETX_PANEL) {
            size_t k = v.count < sizeof(m.supplier) - 1 ? v.count : sizeof(m.supplier) - 1;
            memcpy(m.supplier, v.text, k);
            m.supplier[k] = 0;
        }
    }
    m.updates += ch;
    return ch;
}

// Map with "tn"/"pn"/"v" (info message) or "pn"/"v" inside a topic (discovery answer)
static int readParamMap(CborIn& r, InetxModel& m, uint16_t src, uint32_t entries, const char* topicIn) {
    char tn[24], pn[32], key[16];
    strncpy(tn, topicIn ? topicIn : "", sizeof(tn) - 1);
    tn[sizeof(tn) - 1] = 0;
    pn[0] = 0;
    CborValue val;
    val.kind = CBK_NONE;
    for (uint32_t k = 0; k < entries && r.ok; k++) {
        if (!cborReadText(r, key, sizeof(key))) { if (!cborSkip(r)) return 0; cborSkip(r); continue; }
        if (!strcmp(key, "tn") && !topicIn) { if (!cborReadText(r, tn, sizeof(tn))) cborSkip(r); }
        else if (!strcmp(key, "pn")) { if (!cborReadText(r, pn, sizeof(pn))) cborSkip(r); }
        else if (!strcmp(key, "v")) cborReadValue(r, val);
        else cborSkip(r);
    }
    if (!r.ok || !tn[0] || !pn[0] || val.kind == CBK_NONE || val.kind == CBK_NULL) return 0;
    return takeParam(m, src, tn, pn, val);
}

// Discovery answer: {"topics": [{"tn": ..., "parameters": [{"pn": ..., "v": ...}, ...]}, ...]}
static int readTopics(CborIn& r, InetxModel& m, uint16_t src) {
    uint8_t major, info;
    uint64_t n;
    if (!cborHead(r, major, n, info) || major != 4) return 0;
    int ch = 0;
    for (uint64_t t = 0; t < n && r.ok; t++) {
        uint64_t entries;
        if (!cborHead(r, major, entries, info)) return ch;
        if (major != 5) { r.ok = false; return ch; }
        char tn[24] = "";
        // "tn" may come after "parameters": remember where the parameters start and read them second
        const uint8_t* paramsAt = nullptr;
        for (uint64_t k = 0; k < entries && r.ok; k++) {
            char key[16];
            if (!cborReadText(r, key, sizeof(key))) { cborSkip(r); cborSkip(r); continue; }
            if (!strcmp(key, "tn")) { if (!cborReadText(r, tn, sizeof(tn))) cborSkip(r); }
            else if (!strcmp(key, "parameters")) { paramsAt = r.p; cborSkip(r); }
            else cborSkip(r);
        }
        if (!r.ok || !paramsAt || !tn[0]) continue;
        CborIn pr = {paramsAt, r.end, true};
        uint64_t pc;
        if (!cborHead(pr, major, pc, info) || major != 4) continue;
        for (uint64_t p = 0; p < pc && pr.ok; p++) {
            uint64_t pe;
            const uint8_t* at = pr.p;
            if (!cborHead(pr, major, pe, info)) break;
            if (major != 5) { pr.p = at; cborSkip(pr); continue; }
            ch += readParamMap(pr, m, src, (uint32_t)pe, tn);
        }
    }
    return ch;
}

int inetxModelFeed(InetxModel& m, const uint8_t* frame, size_t len, uint16_t* sender) {
    InetxHeader h;
    if (!inetxParseHeader(frame, len, h)) return 0;
    if (sender) *sender = h.src;
    if (!h.body) return 0;
    CborIn r;
    cborInBegin(r, h.body, h.bodyLen);
    uint8_t major, info;
    uint64_t entries;
    if (!cborHead(r, major, entries, info) || major != 5) return 0;

    if (h.ctrl == INETX_CTRL_REG && h.sub == INETX_REG_RESP) {
        for (uint64_t k = 0; k < entries && r.ok; k++) {
            char key[8];
            if (!cborReadText(r, key, sizeof(key))) { cborSkip(r); cborSkip(r); continue; }
            CborValue v;
            if (!cborReadValue(r, v)) return 0;
            if (!strcmp(key, "addr") && v.kind == CBK_NUM && v.num > 0 && v.num < 0xFFFF) m.assigned = (uint16_t)v.num;
        }
        return 0;
    }
    if (h.ctrl != INETX_CTRL_MBP) return 0;
    if (h.sub == INETX_MBP_INFO) return readParamMap(r, m, h.src, (uint32_t)entries, nullptr);
    if (h.sub == INETX_MBP_DISC_RESP) {
        int ch = 0;
        for (uint64_t k = 0; k < entries && r.ok; k++) {
            char key[16];
            if (!cborReadText(r, key, sizeof(key))) { cborSkip(r); cborSkip(r); continue; }
            if (!strcmp(key, "topics")) ch += readTopics(r, m, h.src);
            else cborSkip(r);
        }
        return ch;
    }
    return 0;
}

int inetxWaterMode(const InetxModel& m) {
    if (isnan(m.water_on)) return -1;
    if (m.water_on == 0) return 0;
    return (!isnan(m.water_boost) && m.water_boost != 0) ? 2 : 1;
}

void inetxCommandClear(InetxCommand& c) {
    c.hasTarget = false;
    c.target = NAN;
    c.gas = c.el_level = c.water = c.power = -1;
}

int inetxCommandWrites(const InetxModel& m, const InetxCommand& c, InetxWrite* out, int max) {
    int n = 0;
    auto add = [&](uint16_t dst, const char* t, const char* p, int32_t v) {
        if (n < max) out[n++] = {dst, t, p, v};
    };
    uint16_t heater = m.heaterAddr ? m.heaterAddr : INETX_HEATER;
    uint16_t energy = m.energyAddr ? m.energyAddr : heater;
    uint16_t water = m.waterAddr ? m.waterAddr : heater;
    if (c.power >= 0) {
        // The room mode belongs to the panel, which relays it to the heater
        int want = c.power ? 3 : 0;
        bool isOn = !isnan(m.room_mode) && m.room_mode != 0;
        if (isnan(m.room_mode) || isOn != (c.power != 0)) add(INETX_PANEL, "RoomClimate", "Mode", want);
    }
    if (c.hasTarget && !isnan(c.target)) {
        int32_t wire = (int32_t)lroundf(c.target * 10.0f);
        if (isnan(m.target) || (int32_t)lroundf(m.target * 10.0f) != wire) add(heater, "AirHeating", "TgtTemp", wire);
    }
    if (c.gas >= 0 && (isnan(m.gas) || (int)m.gas != (c.gas ? 1 : 0))) add(energy, "EnergySrc", "GasLevel", c.gas ? 1 : 0);
    if (c.el_level >= 0 && (isnan(m.el_level) || (int)m.el_level != c.el_level)) add(energy, "EnergySrc", "ElectricLevel", c.el_level);
    if (c.water >= 0) {
        int cur = inetxWaterMode(m);
        if (cur != c.water) {
            bool on = c.water != 0, boost = c.water == 2;
            if (cur < 0 || (cur != 0) != on) add(water, "WaterHeating", "Active", on ? 1 : 0);
            bool curBoost = !isnan(m.water_boost) && m.water_boost != 0;
            if (on && (isnan(m.water_boost) || curBoost != boost)) add(water, "WaterHeating", "BoostMode", boost ? 1 : 0);
            if (!on && curBoost) add(water, "WaterHeating", "BoostMode", 0);
        }
    }
    return n;
}
