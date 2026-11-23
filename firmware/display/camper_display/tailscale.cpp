// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Tailscale on the display - see tailscale.h
#include "tailscale.h"
#include "settings.h"
#include "wifi_manager.h"
#include "evlog.h"
#include <Preferences.h>

#if WITH_TAILSCALE
#include "src/microlink/ml_arduino_config.h"
#include "src/microlink/microlink.h"   // C API with its own extern "C"
extern "C" void vn_ts_nodekey(const microlink_t* ml, char* out, size_t len);   // src/ts_nodekey.c
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace ts {

static const char* NS = "vn_ts";   // own flag: erase the microlink keys on the next start

// Tailnet name: lower case a-z, 0-9 and '-', at most 63 characters
String name() {
  String n = settings.tsName.length() ? settings.tsName : settings.hostname, r;
  n.toLowerCase();
  for (size_t i = 0; i < n.length() && r.length() < 63; i++) {
    char c = n[i];
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') r += c;
    else if (c == ' ' || c == '_' || c == '.') r += '-';
  }
  while (r.startsWith("-")) r.remove(0, 1);
  if (r.isEmpty()) r = "display-node";
  return r;
}

void forget() {
  Preferences p;
  if (!p.begin(NS, false)) return;
  p.putBool("reset", true);
  p.end();
}

#if WITH_TAILSCALE

// Below this much free internal RAM microlink is not started: its tasks and the TLS
// connection to the relay need internal memory, and the panel must keep running.
static const size_t MIN_INTERNAL_HEAP = 60 * 1024;
static const uint32_t RETRY_MS = 60000;

static microlink_t* ml = nullptr;
static SemaphoreHandle_t mx = nullptr;   // guards the strings below (read by the web task)
static String curIp, curUrl, curName, curNodeKey, cfgKey, cfgName;   // cfg*: microlink keeps pointers into these
static volatile int curCode = -1;        // microlink_state_t, -1 = not running
static volatile int curPeers = 0;
static int loggedCode = -2;
static uint32_t lastTry = 0, lastPoll = 0;
static bool gaveUp = false;

struct Lock {
  Lock() { if (mx) xSemaphoreTake(mx, portMAX_DELAY); }
  ~Lock() { if (mx) xSemaphoreGive(mx); }
};

static size_t internalFree() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }

static void onState(microlink_t*, microlink_state_t st, void*) { curCode = (int)st; }

static void start() {
  if (internalFree() < MIN_INTERNAL_HEAP) {
    evlog::add("Tailscale: zu wenig RAM (%u kB frei), nicht gestartet", (unsigned)(internalFree() / 1024));
    gaveUp = true;   // the free RAM will not grow by itself; next try after a restart
    return;
  }
  cfgKey = settings.tsKey;
  cfgName = name();
  microlink_config_t c = {};
  c.auth_key = cfgKey.length() ? cfgKey.c_str() : nullptr;   // no key: interactive sign-in (link)
  c.device_name = cfgName.c_str();
  c.enable_derp = true;
  c.enable_stun = true;
  c.enable_disco = true;
  c.max_peers = CONFIG_ML_MAX_PEERS;
  microlink_t* h = microlink_init(&c);
  if (!h) {
    evlog::add("Tailscale: Start fehlgeschlagen (Speicher?)");
    return;
  }
  microlink_set_state_callback(h, onState, nullptr);
  if (microlink_start(h) != ESP_OK) {
    evlog::add("Tailscale: Start fehlgeschlagen");
    microlink_destroy(h);
    return;
  }
  ml = h;
  char nk[80];
  vn_ts_nodekey(h, nk, sizeof(nk));   // fixed for this device until sign-out
  {
    Lock l;
    curName = cfgName;
    curNodeKey = nk;
  }
  evlog::add("Tailscale gestartet als %s%s", cfgName.c_str(), cfgKey.length() ? " (Auth-Key)" : "");
}

// Copy what the web page and the panel show, and log the important changes
static void poll() {
  int code = ml ? (int)microlink_get_state(ml) : -1;
  curCode = code;
  String ip, url;
  int peers = 0;
  if (ml && code == ML_STATE_CONNECTED) {
    char b[20];
    microlink_ip_to_str(microlink_get_vpn_ip(ml), b);
    ip = b;
    peers = microlink_get_peer_count(ml);
  }
  if (ml && code == ML_STATE_NEEDS_LOGIN) {
    char b[MICROLINK_AUTH_URL_MAX];
    if (microlink_get_auth_url(ml, b, sizeof(b)) == ESP_OK) url = b;
  }
  {
    Lock l;
    curIp = ip;
    curUrl = url;
  }
  curPeers = peers;
  if (code != loggedCode) {
    if (code == ML_STATE_CONNECTED) evlog::add("Tailscale verbunden: %s", ip.c_str());
    else if (code == ML_STATE_NEEDS_LOGIN) evlog::add("Tailscale wartet auf Anmeldung (Link auf der Webseite)");
    else if (code == ML_STATE_AUTH_FAILED) evlog::add("Tailscale: Anmeldung abgelehnt");
    else if (loggedCode == ML_STATE_CONNECTED) evlog::add("Tailscale getrennt");
    loggedCode = code;
  }
}

void begin() {
  if (!mx) mx = xSemaphoreCreateMutex();
  Preferences p;
  if (p.begin(NS, false)) {
    if (p.getBool("reset", false)) {
      p.remove("reset");
      // must run before microlink_init(): erases the device keys and the cached peers
      if (microlink_factory_reset() == ESP_OK) evlog::add("Tailscale abgemeldet, Schlüssel gelöscht");
    }
    p.end();
  }
}

void loop() {
  if (!settings.tsOn) return;
  uint32_t now = millis();
  if (!ml) {
    // start only with WiFi; after a failed start try again after RETRY_MS
    if (gaveUp || !wifimgr::staConnected()) return;
    if (!wifimgr::timeValid() && now < 120000) return;   // TLS to the relays wants the clock (NTP) first
    if (lastTry && now - lastTry < RETRY_MS) return;
    lastTry = now;
    start();
    return;
  }
  if (now - lastPoll < 1000) return;
  lastPoll = now;
  poll();
}

void json(JsonObject o) {
  o["built"] = true;
  o["on"] = settings.tsOn;
  o["code"] = ml ? (int)curCode : -1;
  o["peers"] = (int)curPeers;
  o["heap"] = (unsigned)(internalFree() / 1024);
  if (settings.tsOn && !ml) o["wait"] = gaveUp ? "ram" : (wifimgr::staConnected() ? "start" : "wifi");
  Lock l;
  o["name"] = curName.length() ? curName : name();
  if (curIp.length()) o["ip"] = curIp;
  if (curUrl.length()) o["url"] = curUrl;
  if (curNodeKey.length()) o["nodekey"] = curNodeKey;   // for Tailnet Lock: tailscale lock sign <nodekey>
}

String ip() {
  Lock l;
  return curIp;
}

bool needsLogin() { return ml && curCode == ML_STATE_NEEDS_LOGIN; }

#else  // !WITH_TAILSCALE

void begin() {}
void loop() {}
void json(JsonObject o) {
  o["built"] = false;
  o["on"] = false;
  o["code"] = -1;
}
String ip() { return String(); }
bool needsLogin() { return false; }

#endif

}  // namespace ts
