// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).
//
// Alde external panel bus ("CI-bus", yellow RJ12 on the 3030 Plus panel, "External
// panel" port on the 3020): LIN at 19200 baud 8N1. Here the ESP is the LIN MASTER and
// the Alde panel answers as a slave:
//   0x3C  registration (ReadByIdentifier with supplier 0x41DE and the function id)
//   0x1B  info frame, protected id 0x5B: header only, the panel answers 8 bytes + checksum
//   0x1A  control frame: 8 bytes + checksum from the ESP
//
// Frame layout, encodings and sequence taken from daviddootson/alde_3030_plus (MIT,
// Copyright (c) 2026 daviddootson), see LICENSE-alde3030-MIT and NOTICE.md.
// Verified there on an Alde 3030 Plus. For the Compact 3020 (function id 0x0001) and the
// 3030 (0x0002) the same frames are assumed - NOT verified.
// Checksum fallback (classic LIN 1.x checksum on some 3020 panels) and the longer answer
// window follow holdestmade/esphome-alde-3020 (MIT), see NOTICE.md.
//
// Plain C++ without Arduino dependencies, so it can be tested on the host.
#ifndef ALDE_CI_H
#define ALDE_CI_H

#include <stdint.h>

#define ALDE_CI_ID_CONTROL  0x1A
#define ALDE_CI_ID_INFO     0x5B   // id 0x1B with parity bits
#define ALDE_CI_ID_DIAG     0x3C   // sent like this by the reference implementation

struct AldeCiState {
    bool valid;
    float room;        // zone 1 actual (NAN if not available)
    float outdoor;     // outdoor sensor (NAN if not fitted)
    float setpoint;    // zone 1 setpoint
    uint8_t gas;       // gas heating enabled
    uint8_t valve;     // gas valve open
    uint8_t el_level;  // electric 0..3 kW
    uint8_t water;     // 0 = off, 1 = on, 2 = boost, 3 = unknown (not published)
    uint8_t panel_on;
    uint8_t busy;
    uint8_t error;
    uint8_t ac_input;  // 230 V present
    uint8_t pump;
    uint8_t raw[8];    // info payload, used to build control frames
    uint8_t classic;   // 1 = the panel answered with the classic checksum (data only);
                       // control frames then use the same kind
};

// A requested change. Fields left at -1 / hasTarget=false keep the current value.
struct AldeCiCommand {
    bool hasTarget;
    float target;
    int8_t gas;        // 0/1
    int8_t el_level;   // 0..3
    int8_t water;      // 0..2
    int8_t power;      // 0/1: heater off / on (bit 0 of byte 5)
};

void aldeCiCommandClear(AldeCiCommand& c);
bool aldeCiCommandEmpty(const AldeCiCommand& c);

// Enhanced checksum over the id as sent plus the data (carry wrap-around)
uint8_t aldeCiChecksum(uint8_t id, const uint8_t* d, int n);

// Classic checksum (LIN 1.x): data only. Used only if the panel answers with it.
uint8_t aldeCiChecksumClassic(const uint8_t* d, int n);

// Checksum for a control frame: the kind the panel used in its last info answer
uint8_t aldeCiControlChecksum(const AldeCiState& s, const uint8_t* d, int n);

// Temperature byte: raw * 0.5 - 42; 0xFB..0xFF = special values -> NAN
float aldeCiTemp(uint8_t raw);

// Registration payload for the 0x3C frame (function id: 1 = 3020, 2 = 3030, 3 = 3030 Plus)
void aldeCiDiagPayload(uint16_t functionId, uint8_t out[8]);

// Find the echo of our header ("55 5B", or only "5B" if the sync byte was lost after
// the break) and decode the 8 data bytes + checksum after it. The enhanced checksum is
// tried first, the classic one only as fallback (sets out->classic). Returns false if
// nothing valid was found.
bool aldeCiParseInfo(const uint8_t* buf, int n, AldeCiState* out);

// True if the panel must not get a command right now (busy or error bit set)
bool aldeCiBlocked(const AldeCiState& s);

// Drop every requested change that the panel already reports (setpoint compared in
// 0.5 degree steps). Returns true if something is left to send.
bool aldeCiCommandReduce(const AldeCiState& s, AldeCiCommand& c);

// Control payload from the last info state with the requested changes applied.
// Everything that is not changed is copied from the panel's own state, so a command
// never resets settings made on the panel.
void aldeCiBuildControl(const AldeCiState& s, const AldeCiCommand& c, uint8_t out[8]);

#endif // ALDE_CI_H
