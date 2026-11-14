// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Passive LIN bus decoder for the Alde Compact 3020 HE.
//
// Frame layout, field offsets, encodings and plausibility gates are taken from
// Mellowgroup-dk/alde-3020he-lin (code MIT, Copyright (c) 2026 Mellowgroup-dk,
// documentation CC BY 4.0), see LICENSE-alde-MIT and NOTICE.md.
//
// RECEIVE-ONLY BY CONSTRUCTION: this module never writes to the bus. It only
// consumes bytes that the UART received. Keep it that way - the Alde panel is the
// LIN master and control of the heater is NOT decoded (see README).
//
// Plain C++ without Arduino dependencies, so it can be tested on the host
// (tests/run.sh).
#ifndef ALDE_LIN_H
#define ALDE_LIN_H

#include <stdint.h>
#include <stddef.h>

struct AldeReadings {
    // NAN = not seen yet. Values keep their last valid reading (sentinels are dropped).
    float setpoint;        // 0x03 @0-1  heat command - only the setpoint while the AC unit is dormant
    float room;            // 0x47 @0-1  room temperature
    float glycol;          // 0x5B @0-1  glycol temperature
    float water;           // 0xD6 @4-5  water temperature
    float pcb;             // 0xD3 @0-1  PCB temperature
    float hotwater_target; // 0xC4 @0-1  0 = off, 50 = flow, 65 = full
    float mains_current;   // 0x5B @2-3  total mains current of the vehicle (A)
    float battery;         // 0xD6 @6-7  12 V battery (V)
    float el_allowed;      // 0x06 @0-1  permitted electric power (W) - a state, drops on load shedding
    float el_power;        // 0x55 @0-1  applied electric power (W)
    float burner_fan;      // 0x5B @6    burner fan (rpm) = byte * 32
    float gas;             // 0x85 @0    1 = on (0x04), 0 = off (0x02), other values ignored
    float ac_light;        // 0x97 @6    light level of the AC unit (%)
    float ac_active;       // 1 = AC unit awake (0x08 answered recently), 0 = dormant - derived
};

struct AldeBusStats {
    uint32_t frames_ok;    // frames with a valid checksum
    uint32_t frames_bad;   // rejected (length or checksum)
    uint32_t headers_empty;// headers without a response
    uint32_t last_frame_ms;// time of the last valid frame (0 = never)
    uint32_t last_ac_ms;   // time of the last 0x08 response (0 = never)
};

class AldeLin {
public:
    AldeLin();

    // Feed received bytes. now_ms = millis(). Never blocks, never writes.
    void feed(const uint8_t* data, size_t len, uint32_t now_ms);

    // Re-evaluate time based states (bus timeout, AC awake). Call regularly.
    void tick(uint32_t now_ms);

    const AldeReadings& readings() const { return r_; }
    const AldeBusStats& stats() const { return s_; }

    // True if a published reading changed since the last call (clears the flag).
    bool takeChanged();

    // True while valid frames arrive (panel connected and bus running).
    bool busOk(uint32_t now_ms) const;

    // Optional: called for every signal frame whose payload changed
    // (pid, data, len). Used for the "sig" log in the serial monitor.
    void (*onSignalChange)(uint8_t pid, const uint8_t* d, int n);

    static const uint32_t BUS_TIMEOUT_MS = 5000;   // schedule repeats every 388 ms
    static const uint32_t AC_TIMEOUT_MS  = 20000;  // 0x08 answers ~every 5 s while the AC unit is awake

private:
    static const int BUF_MAX = 512;
    uint8_t buf_[BUF_MAX];
    int len_;

    AldeReadings r_;
    AldeBusStats s_;
    bool changed_;
    uint8_t last_[256][8];
    uint8_t lastLen_[256];

    void parse(uint32_t now_ms);
    void handleFrame(uint8_t pid, const uint8_t* d, int n, uint32_t now_ms);
    void set(float& field, float v);
};

// LIN checksum: classic (PID excluded) for 0x3C/0x3D, enhanced (PID included) for the rest.
uint8_t aldeLinChecksum(uint8_t pid, const uint8_t* d, int n, bool classic);

#endif // ALDE_LIN_H
