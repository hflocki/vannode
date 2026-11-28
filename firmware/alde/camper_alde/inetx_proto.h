// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Truma iNet X panel over Bluetooth LE: message format and the Alde values on its bus.
// No Arduino or BLE code here, so the host tests can run it (tests/inetx_test.cpp).
//
// Behind an iNet X panel the heater is one device on a bus that the panel relays over BLE.
// Every message is a frame with a 16 byte header and a CBOR body:
//
//   [0-1]   destination address (LE)        0xFFFF broadcast, 0x0000 message broker,
//   [2-3]   source address (LE)              0x0101 panel, 0x0404 Alde (as measured), ...
//   [4-5]   size = body length + 9 (LE)
//   [6]     control type                     0x01 registration, 0x03 message bus
//   [7-15]  segmentation header, all zero
//   [16]    sub type                         0x00 info, 0x01 write, 0x02 subscribe,
//                                            0x04 parameter discovery (answer 0x84)
//   [17]    correlation id
//   [18-]   CBOR map
//
// Values are named "topic.parameter" (e.g. AirHeating.TgtTemp), temperatures in tenths of a
// degree. Layout and names as described by the GPL-3.0 project
// github.com/darenshaw/truma-inet-x-to-alde-3020 (see NOTICE.md); this is an own implementation.

#ifndef INETX_PROTO_H
#define INETX_PROTO_H

#include <stdint.h>
#include <stddef.h>
#include <math.h>

// ---- bus addresses ----
static const uint16_t INETX_BROADCAST = 0xFFFF;
static const uint16_t INETX_BROKER    = 0x0000;
static const uint16_t INETX_PANEL     = 0x0101;
static const uint16_t INETX_HEATER    = 0x0404;   // Alde Compact as measured; learned at runtime
static const uint16_t INETX_BLE_MGMT  = 0x0601;   // the panel's own BLE management (keep-alive)
static const uint16_t INETX_APP       = 0x0500;   // our address until the panel assigns one

// ---- frame types ----
static const uint8_t INETX_CTRL_REG  = 0x01;
static const uint8_t INETX_CTRL_MBP  = 0x03;
static const uint8_t INETX_MBP_INFO  = 0x00;
static const uint8_t INETX_MBP_WRITE = 0x01;
static const uint8_t INETX_MBP_SUB   = 0x02;
static const uint8_t INETX_MBP_DISC  = 0x04;
static const uint8_t INETX_MBP_DISC_RESP = 0x84;
static const uint8_t INETX_REG_RESP  = 0x02;

static const size_t INETX_HDR = 18;            // header incl. sub type and correlation id
static const size_t INETX_MAX_FRAME = 512;     // what we build; received frames can be larger

// ============================================================
// CBOR (RFC 8949): the subset the panel uses
// ============================================================

// Reader over one buffer, no allocation: the panel's answers can describe a hundred parameters
// with their limits and value names, more than a tree in RAM should hold.
struct CborIn {
    const uint8_t* p;
    const uint8_t* end;
    bool ok;
};

// One scalar value, as far as this firmware needs it
enum CborKind : uint8_t { CBK_NONE, CBK_NUM, CBK_TEXT, CBK_ARRAY, CBK_MAP, CBK_NULL };
struct CborValue {
    CborKind kind;
    double num;            // CBK_NUM (also true / false as 1 / 0)
    const uint8_t* text;   // CBK_TEXT (not 0-terminated)
    uint32_t count;        // CBK_TEXT length, CBK_ARRAY / CBK_MAP entries
};

void cborInBegin(CborIn& r, const uint8_t* buf, size_t len);
// Head of the next item: major type 0..7 and its argument. false at the end or on bad input.
bool cborHead(CborIn& r, uint8_t& major, uint64_t& arg, uint8_t& info);
// Skip the next item completely (nested arrays / maps included)
bool cborSkip(CborIn& r);
// Next item must be text: copied 0-terminated (truncated to size)
bool cborReadText(CborIn& r, char* out, size_t size);
// Next item as a value; arrays and maps are skipped and only counted
bool cborReadValue(CborIn& r, CborValue& v);

// Encoder: appends to buf, returns false once the buffer is full
struct CborOut {
    uint8_t* buf;
    size_t cap;
    size_t len;
    bool ok;
};
void cborBegin(CborOut& o, uint8_t* buf, size_t cap);
void cborMap(CborOut& o, uint32_t entries);
void cborArray(CborOut& o, uint32_t entries);
void cborTextOut(CborOut& o, const char* s);
void cborInt(CborOut& o, int64_t v);

// ============================================================
// Frames
// ============================================================

// Header + body into out, returns the frame length (0 = does not fit)
size_t inetxFrame(uint8_t* out, size_t cap, uint16_t dst, uint16_t src, uint8_t ctrl, uint8_t sub,
                  uint8_t corr, const uint8_t* body, size_t bodyLen);

size_t inetxRegisterFrame(uint8_t* out, size_t cap, uint16_t src);
size_t inetxSubscribeFrame(uint8_t* out, size_t cap, uint16_t src, const char* const* topics, uint8_t count);
size_t inetxDiscoverFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst);
size_t inetxWriteIntFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst, const char* topic, const char* param, int64_t v);
size_t inetxWriteTextFrame(uint8_t* out, size_t cap, uint16_t src, uint16_t dst, const char* topic, const char* param, const char* v);
size_t inetxLastMessageFrame(uint8_t* out, size_t cap, uint16_t src);

struct InetxHeader {
    uint16_t dst, src, size;
    uint8_t ctrl, sub, corr;
    const uint8_t* body;
    size_t bodyLen;
};
// false if shorter than a header
bool inetxParseHeader(const uint8_t* d, size_t len, InetxHeader& h);

// ============================================================
// Alde values on the bus
// ============================================================

// What the panel relays from the Alde (and the panel's own room mode). NAN = not reported yet.
struct InetxModel {
    float target;        // AirHeating.TgtTemp / 10 (the setpoint the Alde regulates to)
    float room;          // AirHeating.Temp / 10
    float outdoor;       // AirHeating.ExtTemp / 10 (or Temperature.External)
    float gas;           // EnergySrc.GasLevel != 0
    float el_level;      // EnergySrc.ElectricLevel (kW steps 0..3)
    float water_on;      // WaterHeating.Active != 0
    float water_boost;   // WaterHeating.BoostMode != 0
    float heating;       // AirHeating.Active == 1 (heating right now, not idle)
    float room_mode;     // RoomClimate.Mode (0 off, 3 heat, ...)
    float error;         // ErrorReset.ErrCode list not empty (any device)
    uint16_t heaterAddr; // who publishes AirHeating.TgtTemp (0 = not seen)
    uint16_t panelAddr;  // who publishes RoomClimate.Mode
    uint16_t energyAddr; // who publishes EnergySrc.*
    uint16_t waterAddr;  // who publishes WaterHeating.*
    bool outdoorFromHeater; // the heater's own outdoor sensor wins over the panel's copy
    uint16_t assigned;   // our address, from the registration answer (0 = none yet)
    char supplier[16];   // Identify.Supplier of the heater ("Alde"), if reported
    uint32_t updates;    // values taken over
};

void inetxModelClear(InetxModel& m);

// Takes one received frame. Returns the number of values that changed (0 = nothing for us).
// A registration answer sets m.assigned. *sender (optional) = the frame's source address.
int inetxModelFeed(InetxModel& m, const uint8_t* frame, size_t len, uint16_t* sender = nullptr);

// Water mode as the panel bus reports it: 0 off, 1 on, 2 boost, -1 unknown
int inetxWaterMode(const InetxModel& m);

// One parameter write that a command turns into
struct InetxWrite {
    uint16_t dst;
    const char* topic;
    const char* param;
    int32_t value;
};

// Command (fields < 0 / hasTarget false = unchanged) into the writes that differ from the model.
// Returns how many entries of out[] are used (at most 6).
struct InetxCommand {
    bool hasTarget;
    float target;      // 5..30 °C
    int8_t gas;        // 0/1
    int8_t el_level;   // 0..3
    int8_t water;      // 0 off, 1 on, 2 boost
    int8_t power;      // 0 off, 1 on (RoomClimate.Mode 0 / 3 on the panel)
};
void inetxCommandClear(InetxCommand& c);
int inetxCommandWrites(const InetxModel& m, const InetxCommand& c, InetxWrite* out, int max);

#endif // INETX_PROTO_H
