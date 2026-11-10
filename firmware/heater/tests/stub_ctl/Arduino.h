// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Arduino stub for control_test: the protocol stub plus a working String.
#pragma once
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <ctime>
typedef uint8_t byte;
extern uint32_t g_us;
inline uint32_t micros(){return g_us;}
inline uint32_t millis(){return g_us/1000+1;}
#define OUTPUT 1
#define INPUT_PULLUP 2
#define HIGH 1
#define LOW 0
inline void pinMode(int,int){} inline void digitalWrite(int,int){} inline int digitalRead(int){return 1;}
using std::isnan;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x)
#define portEXIT_CRITICAL(x)
#define CONFIG_FREERTOS_UNICORE 0

// Small std::string based String for the heater_control test (only what the firmware uses).
#include <cctype>
#include <cstdlib>
#include <algorithm>
class String {
 public:
  std::string s;
  String() {}
  String(const char *c) : s(c ? c : "") {}
  String(const std::string &c) : s(c) {}
  const char *c_str() const { return s.c_str(); }
  unsigned length() const { return s.size(); }
  bool isEmpty() const { return s.empty(); }
  void trim() {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    s = a == std::string::npos ? "" : s.substr(a, b - a + 1);
  }
  void toLowerCase() { for (auto &c : s) c = (char) tolower((unsigned char) c); }
  bool equalsIgnoreCase(const String &o) const {
    if (s.size() != o.s.size()) return false;
    for (size_t i = 0; i < s.size(); i++) if (tolower((unsigned char) s[i]) != tolower((unsigned char) o.s[i])) return false;
    return true;
  }
  int indexOf(char c, unsigned from = 0) const { size_t p = s.find(c, from); return p == std::string::npos ? -1 : (int) p; }
  String substring(unsigned from) const { return from >= s.size() ? String() : String(s.substr(from)); }
  String substring(unsigned from, unsigned to) const { return from >= s.size() || to <= from ? String() : String(s.substr(from, to - from)); }
  long toInt() const { return strtol(s.c_str(), nullptr, 10); }
  float toFloat() const { return strtof(s.c_str(), nullptr); }
  bool operator==(const String &o) const { return s == o.s; }
  bool operator==(const char *o) const { return s == o; }
  bool operator!=(const String &o) const { return s != o.s; }
  bool operator!=(const char *o) const { return s != o; }
};
