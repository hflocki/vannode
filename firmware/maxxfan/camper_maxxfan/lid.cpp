// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "lid.h"
#include "config.h"
#include "settings.h"
#include "evlog.h"

namespace {

// ESP32-C3: I2C uses 5/6, 2/8/9 are strapping pins, 11-17 flash, 18/19 USB.
const int8_t FREE_PINS[] = {0, 1, 3, 4, 7, 10, 20, 21};

int8_t   pin = -1;
int8_t   stable = -1;          // debounced state
int8_t   candidate = -1;
uint32_t candidateSince = 0;
uint32_t lastPoll = 0;
uint32_t closeDeadline = 0;    // 0 = no close request pending

int8_t readRaw() {
  bool high = digitalRead(pin) == HIGH;
  // Default: normally open reed + pull-up -> magnet at the contact (lid closed) pulls the pin LOW
  bool open = settings.lidInvert ? !high : high;
  return open ? 1 : 0;
}

} // namespace

// Defined in mqtt_ha.cpp, avoids a second change callback in fan_control
namespace mqttha { void markDirty(); }

namespace lid {

bool validPin(int p) {
  for (int8_t f : FREE_PINS) if (f == p) return true;
  return false;
}

void begin() {
  pin = validPin(settings.lidPin) ? settings.lidPin : -1;
  if (pin < 0) return;
  pinMode(pin, INPUT_PULLUP);
  delay(5);
  stable = candidate = readRaw();
  candidateSince = millis();
  Serial.printf("[lid] reed contact on GPIO%d: %s\n", pin, text());
}

void loop() {
  if (pin < 0) return;
  uint32_t now = millis();
  if (now - lastPoll < 10) return;
  lastPoll = now;

  int8_t raw = readRaw();
  if (raw != candidate) { candidate = raw; candidateSince = now; }
  if (candidate != stable && now - candidateSince >= LID_DEBOUNCE_MS) {
    stable = candidate;
    Serial.printf("[lid] %s\n", text());
    if (stable == 0) closeDeadline = 0;
    mqttha::markDirty();
  }

  if (closeDeadline && (int32_t)(now - closeDeadline) >= 0) {
    closeDeadline = 0;
    if (stable == 1) evlog::add("Haube nach dem Schließen noch offen");
  }
}

bool enabled()      { return pin >= 0; }
int8_t state()      { return pin >= 0 ? stable : -1; }
const char* text()  { return state() == 1 ? "open" : state() == 0 ? "closed" : ""; }

void expectClosed() {
  if (pin < 0) return;
  closeDeadline = millis() + LID_CLOSE_TIMEOUT_MS;
  if (closeDeadline == 0) closeDeadline = 1;
}

} // namespace lid
