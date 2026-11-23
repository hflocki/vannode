// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Shared source – edit only in firmware/_common, then run tools/common/sync_common.py
#include "mqttfb.h"
#include "evlog.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

namespace mqttfb {

static char     s_main[72] = "";
static uint16_t s_mainPort = 1883;
static char     s_fb[72] = "";
static uint16_t s_fbPort = 1883;
static bool     s_onFb = false;       // next / current connection goes to the fallback
static uint32_t s_failSince = 0;      // first failed attempt of the current series, 0 = none
static uint32_t s_switched = 0;       // millis() of the last switch
static uint32_t s_lastProbe = 0;

void begin(const char* mainHost, uint16_t mainPort, const char* fallback) {
    strlcpy(s_main, mainHost ? mainHost : "", sizeof(s_main));
    s_mainPort = mainPort ? mainPort : 1883;
    s_fb[0] = 0;
    s_fbPort = s_mainPort;
    if (fallback && *fallback) {
        String f = fallback;
        f.trim();
        int c = f.lastIndexOf(':');
        if (c > 0) {
            long p = f.substring(c + 1).toInt();
            if (p > 0 && p <= 65535) s_fbPort = (uint16_t)p;
            f = f.substring(0, c);
        }
        // the fallback must differ from the main broker, otherwise it is pointless
        if (!(f.equalsIgnoreCase(s_main) && s_fbPort == s_mainPort)) strlcpy(s_fb, f.c_str(), sizeof(s_fb));
    }
    s_onFb = false;
    s_failSince = 0;
    s_switched = 0;
}

bool configured() { return s_fb[0] != 0; }
bool active() { return s_onFb && configured(); }
const char* host() { return active() ? s_fb : s_main; }
uint16_t port() { return active() ? s_fbPort : s_mainPort; }

static void use(bool fb) {
    s_onFb = fb;
    s_failSince = 0;
    s_switched = millis();
    s_lastProbe = millis();
}

void result(bool connected) {
    if (!configured() || !s_main[0]) return;
    if (connected) {
        s_failSince = 0;
        return;
    }
    uint32_t now = millis();
    if (!s_failSince) { s_failSince = now ? now : 1; return; }
    if (now - s_failSince < MQTTFB_SWITCH_MS) return;
    // this broker has been unreachable long enough: try the other one
    if (!s_onFb) {
        evlog::add("MQTT: Broker %s weg, Ersatz-Broker %s", s_main, s_fb);
        use(true);
    } else {
        evlog::add("MQTT: Ersatz-Broker %s weg, zurück zu %s", s_fb, s_main);
        use(false);
    }
}

bool backDue() {
    if (!active() || !s_main[0]) return false;
    uint32_t now = millis();
    if (now - s_lastProbe < MQTTFB_PROBE_MS) return false;
    s_lastProbe = now;
    WiFiClient probe;
#if defined(ESP8266)
    probe.setTimeout(1500);
    bool up = probe.connect(s_main, s_mainPort);
#else
    bool up = probe.connect(s_main, s_mainPort, 1500);
#endif
    probe.stop();
    if (!up) return false;
    evlog::add("MQTT: Broker %s wieder da", s_main);
    use(false);
    return true;
}

void json(JsonObject o) {
    if (!configured()) return;
    String fb = s_fb;
    if (s_fbPort != s_mainPort) { fb += ':'; fb += s_fbPort; }
    o["fb"] = fb;
    o["on"] = active();
    if (active()) o["since"] = (uint32_t)((millis() - s_switched) / 1000);
}

}  // namespace mqttfb
