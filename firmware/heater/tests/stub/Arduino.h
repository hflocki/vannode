// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

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
struct String{};
