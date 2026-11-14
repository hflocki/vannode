// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Decoding based on Mellowgroup-dk/alde-3020he-lin (MIT), see alde_lin.h.

#include "alde_lin.h"
#include <math.h>
#include <string.h>

// ---- Encodings (docs/PROTOCOL.md of alde-3020he-lin) ----------------------
//
// Temperature: 16-bit LE decikelvin, T = (raw - 2730) / 10.
//   0xFFFF and 0x7FFF are "not available" and are dropped (value stays as it is).
// Current/voltage: 16-bit LE with bit 15 as type marker, x = (raw & 0x7FFF) / 100.
//   NO sentinel filter here - 0x7FFF is a legal reading after masking. 0xFFFF
//   (-> 327.67) is caught by the plausibility gate at the call site.
// The two encodings share a bit pattern and must not share a filter.

static bool u16(const uint8_t* d, int n, int off, uint16_t* out) {
    if (off + 1 >= n) return false;
    *out = (uint16_t)(d[off] | (d[off + 1] << 8));
    return true;
}

static bool dk(const uint8_t* d, int n, int off, float* out) {
    uint16_t raw;
    if (!u16(d, n, off, &raw)) return false;
    if (raw == 0xFFFF || raw == 0x7FFF) return false;
    *out = ((int)raw - 2730) / 10.0f;
    return true;
}

static bool b15(const uint8_t* d, int n, int off, float* out) {
    uint16_t raw;
    if (!u16(d, n, off, &raw)) return false;
    *out = (raw & 0x7FFF) / 100.0f;
    return true;
}

uint8_t aldeLinChecksum(uint8_t pid, const uint8_t* d, int n, bool classic) {
    uint16_t sum = classic ? 0 : pid;
    for (int k = 0; k < n; k++) {
        sum += d[k];
        if (sum > 0xFF) sum = (sum & 0xFF) + 1;
    }
    return (uint8_t)~sum;
}

AldeLin::AldeLin() : onSignalChange(nullptr), len_(0), changed_(false) {
    float* f = (float*)&r_;
    for (size_t i = 0; i < sizeof(r_) / sizeof(float); i++) f[i] = NAN;
    memset(&s_, 0, sizeof(s_));
    memset(last_, 0, sizeof(last_));
    memset(lastLen_, 0, sizeof(lastLen_));
}

void AldeLin::set(float& field, float v) {
    // Exact compare on purpose: the bus repeats identical raw values every 388 ms
    if (isnan(field) || field != v) {
        field = v;
        changed_ = true;
    }
}

bool AldeLin::takeChanged() {
    bool c = changed_;
    changed_ = false;
    return c;
}

bool AldeLin::busOk(uint32_t now_ms) const {
    return s_.last_frame_ms != 0 && now_ms - s_.last_frame_ms < BUS_TIMEOUT_MS;
}

void AldeLin::tick(uint32_t now_ms) {
    // The AC unit answers 0x08 only while it is enabled (~7.7 % of the slots,
    // about every 5 s). While dormant the slot stays silent indefinitely.
    // Only meaningful while the bus runs at all.
    if (!busOk(now_ms)) return;
    bool awake = s_.last_ac_ms != 0 && now_ms - s_.last_ac_ms < AC_TIMEOUT_MS;
    set(r_.ac_active, awake ? 1.0f : 0.0f);
}

void AldeLin::feed(const uint8_t* data, size_t n, uint32_t now_ms) {
    while (n > 0) {
        int room = BUF_MAX - len_;
        if (room == 0) {
            // Emergency brake: keep the newer half. Normally parse() drains the buffer.
            memmove(buf_, buf_ + BUF_MAX / 2, BUF_MAX / 2);
            len_ = BUF_MAX / 2;
            room = BUF_MAX - len_;
        }
        int take = (int)n < room ? (int)n : room;
        memcpy(buf_ + len_, data, take);
        len_ += take;
        data += take;
        n -= take;
        parse(now_ms);
    }
}

// A LIN break plus sync byte arrives at a plain 8N2 UART as 0x00 0x55, followed by
// the protected ID. A frame is delimited by the FOLLOWING header, so the last frame
// stays in the buffer until the next header arrives.
void AldeLin::parse(uint32_t now_ms) {
    int i = 0;
    while (true) {
        int h = -1;
        for (int k = i; k + 2 < len_; k++)
            if (buf_[k] == 0x00 && buf_[k + 1] == 0x55) { h = k; break; }
        if (h < 0) break;

        int nxt = -1;
        for (int k = h + 3; k + 2 < len_; k++)
            if (buf_[k] == 0x00 && buf_[k + 1] == 0x55) { nxt = k; break; }
        if (nxt < 0) { i = h; break; }

        uint8_t pid = buf_[h + 2];
        int raw = nxt - (h + 3);
        if (raw == 0) {
            s_.headers_empty++;
        } else {
            int dlen = raw - 1;
            const uint8_t* d = &buf_[h + 3];
            uint8_t cs = buf_[nxt - 1];
            uint8_t fid = pid & 0x3F;
            bool classic = (fid == 0x3C || fid == 0x3D);
            if (dlen < 1 || dlen > 8 || aldeLinChecksum(pid, d, dlen, classic) != cs) {
                s_.frames_bad++;
            } else {
                s_.frames_ok++;
                s_.last_frame_ms = now_ms ? now_ms : 1;
                handleFrame(pid, d, dlen, now_ms);
            }
        }
        i = nxt;
    }
    // Drop everything before the first unfinished header
    if (i > 0) {
        memmove(buf_, buf_ + i, len_ - i);
        len_ -= i;
    } else if (len_ > 2 && !(buf_[0] == 0x00 && buf_[1] == 0x55)) {
        // No header at the start: keep only the last byte (could be a 0x00 of the next header)
        buf_[0] = buf_[len_ - 1];
        len_ = 1;
    }
}

void AldeLin::handleFrame(uint8_t pid, const uint8_t* d, int n, uint32_t now_ms) {
    uint8_t fid = pid & 0x3F;
    if (fid == 0x3C || fid == 0x3D) return;   // diagnostics: not needed for the readings

    // Change log for the serial monitor ("sig") - only on change, the bus repeats every 388 ms
    if (onSignalChange) {
        bool diff = lastLen_[pid] != n;
        for (int k = 0; !diff && k < n; k++) diff = last_[pid][k] != d[k];
        if (diff) {
            onSignalChange(pid, d, n);
            lastLen_[pid] = (uint8_t)n;
            memcpy(last_[pid], d, n);
        }
    }

    float t;
    switch (pid) {
    case 0x03:   // heat command; the setpoint only while the AC unit is dormant (not gated on purpose)
        if (dk(d, n, 0, &t) && t > 4.0f && t < 40.0f) set(r_.setpoint, t);
        break;
    case 0x47:   // room temperature
        if (dk(d, n, 0, &t) && t > -20.0f && t < 60.0f) set(r_.room, t);
        break;
    case 0x5B:   // glycol (decikelvin), mains current (bit 15), burner fan (byte * 32)
        if (dk(d, n, 0, &t) && t > -30.0f && t < 130.0f) set(r_.glycol, t);
        if (b15(d, n, 2, &t) && t >= 0.0f && t < 100.0f) set(r_.mains_current, t);
        if (n > 6) set(r_.burner_fan, d[6] * 32.0f);   // 0 = burner off, a valid reading
        break;
    case 0xD6:   // water temperature (decikelvin), battery (bit 15)
        if (dk(d, n, 4, &t) && t > -30.0f && t < 130.0f) set(r_.water, t);
        if (b15(d, n, 6, &t) && t > 0.0f && t < 40.0f) set(r_.battery, t);
        break;
    case 0xD3:   // PCB temperature
        if (dk(d, n, 0, &t) && t > -30.0f && t < 150.0f) set(r_.pcb, t);
        break;
    case 0x55: { // applied electric power (W), raw
        uint16_t w;
        if (u16(d, n, 0, &w) && w <= 5000) set(r_.el_power, (float)w);
        break;
    }
    case 0x06: { // permitted electric power (W), raw - a state, not the user setting
        uint16_t w;
        if (u16(d, n, 0, &w) && w <= 5000) set(r_.el_allowed, (float)w);
        break;
    }
    case 0xC4:   // hot water target; 0.0 = off, so the gate lets zero through
        if (dk(d, n, 0, &t) && t >= 0.0f && t < 100.0f) set(r_.hotwater_target, t);
        break;
    case 0x85:   // gas: only the two observed values, an unknown byte must not become "off"
        if (n >= 1 && (d[0] == 0x02 || d[0] == 0x04)) set(r_.gas, d[0] == 0x04 ? 1.0f : 0.0f);
        break;
    case 0x97:   // AC unit: @6 light level in percent
        if (n > 6 && d[6] <= 100) set(r_.ac_light, (float)d[6]);
        break;
    case 0x08:   // AC unit heartbeat: its presence says "awake", the content is not used
        s_.last_ac_ms = now_ms ? now_ms : 1;
        break;
    default:
        break;
    }
}
