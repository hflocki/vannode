// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Channels of the IO node, all set up in the web UI (stored in /io.json):
//   relay   output on a GPIO, on a 74HC595 chain or on an I2C expander, switches at LOW or HIGH
//   input   GPIO or I2C expander pin, optionally inverted, with pull-up and debounce
//   temp    DS18B20 on the 1-Wire bus (one GPIO for all sensors)
//   analog  ADC pin, two-point conversion millivolts -> value
// Bus hardware (1-Wire pin, 74HC595 pins, I2C pins with MCP23017 / PCF8574) is part of the same file.
// Changes are saved and take effect after a restart; switching relays works at any time.
#ifndef IO_H
#define IO_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

enum IoType : uint8_t { IO_RELAY = 0, IO_INPUT = 1, IO_TEMP = 2, IO_ANALOG = 3 };
enum IoBus : uint8_t { BUS_GPIO = 0, BUS_SR = 1, BUS_EXP = 2 };   // relay: all three, input: GPIO / EXP
enum IoBoot : uint8_t { BOOT_OFF = 0, BOOT_ON = 1, BOOT_LAST = 2 };

struct IoChannel {
    char id[24];          // MQTT sub topic: <prefix>/<id>, command <prefix>/cmd/<id> (a-z, 0-9, _ -)
    char name[32];        // name in the web UI and in Home Assistant
    uint8_t type;         // IoType
    uint8_t bus;          // IoBus (relay / input)
    uint8_t chip;         // expander chip 0..IO_EXP_MAX-1 (BUS_EXP)
    uint8_t pin;          // GPIO, 595 output (0..), or expander pin (0..15 / 0..7)
    bool low;             // relay: switches at LOW; input: inverted (LOW = on)
    bool pullup;          // input: pull-up on (GPIO and MCP23017; the PCF8574 always has one)
    uint8_t boot;         // relay: IoBoot
    uint32_t pulse;       // relay: switch off again after this many ms (0 = stays on)
    uint16_t debounce;    // input: ms
    uint8_t rom[8];       // temp: DS18B20 address
    float offset;         // temp: added to the reading (°C)
    float raw1, val1;     // analog: two points millivolts -> value
    float raw2, val2;
    char unit[8];         // analog: unit
    uint8_t dec;          // analog: decimals
};

struct IoHardware {
    int8_t ow;            // 1-Wire GPIO, -1 = off
    int8_t srData, srClock, srLatch, srOe;   // 74HC595 (OE optional, -1 = not wired)
    uint8_t srChips;      // 0 = no chain, 1..IO_SR_MAX chips (8 outputs each)
    int8_t sda, scl;      // I2C, -1 = off
    uint8_t expType;      // 0 = MCP23017 (16 pins), 1 = PCF8574 (8 pins)
    uint8_t expAddr;      // address of the first chip, the others follow (0x20, 0x21 ...)
    uint8_t expChips;     // 0..IO_EXP_MAX
    uint16_t interval;    // s, temperatures and analog values
};

struct IoLive {
    bool on;              // relay / input state
    float v;              // temp / analog value, NAN = unknown
    float mv;             // analog: raw millivolts
    bool dirty;           // changed, MQTT has to publish it
    bool rawIn;           // input: last raw level (debounce)
    uint32_t rawSince;    // input: millis() of the last raw change
    uint32_t offAt;       // relay with pulse: millis() to switch off, 0 = none
};

extern IoChannel ioCh[IO_MAX_CH];
extern IoLive ioLive[IO_MAX_CH];
extern uint8_t ioCount;
extern IoHardware ioHw;

void ioLoad();                         // /io.json (after LittleFS is mounted)
void ioInit();                         // hardware, relays to their start state
void ioLoop();

// Web UI / MQTT
void ioJson(JsonObject o);             // settings: {hw:{...}, ch:[...]}
const char* ioApply(JsonObjectConst in, String& removed);   // validate + save; nullptr = ok, else error text
bool ioCommand(const char* id, const char* payload);        // ON / OFF / TOGGLE / 1 / 0
void ioLiveJson(JsonArray a);          // current states for the web UI
void ioRequestScan();                  // 1-Wire + I2C scan in loop()
void ioScanJson(JsonObject o);         // last scan result
void ioPrint();                        // serial command "read"
String ioTakeRemoved();                // "id:type " list of deleted channels, cleared on read (MQTT)
const char* ioTypeName(uint8_t t);

#endif // IO_H
