#include "wifi_manager.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "config.h"
#include "logger.h"
#include "settings.h"

namespace wifi {

static const char *const TAG = "wifi";

static DNSServer g_dns;
static bool g_ap = false;
static bool g_was_connected = false;
static uint32_t g_connect_start = 0;
static uint32_t g_last_retry = 0;
static bool g_mdns = false;
static bool g_sta_paused = false;

static String ap_ssid() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "%s-%02X%02X", DEFAULT_HOSTNAME, mac[4], mac[5]);
  return String(buf);
}

static void start_ap() {
  if (g_ap) return;
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap_ssid().c_str(), g_settings.ap_pass.c_str());
  g_dns.start(53, "*", WiFi.softAPIP());
  g_ap = true;
  LOGI(TAG, "Setup access point '%s' started, open http://%s", ap_ssid().c_str(),
       WiFi.softAPIP().toString().c_str());
}

static void stop_ap() {
  if (!g_ap) return;
  g_dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  g_ap = false;
  LOGI(TAG, "Setup access point stopped");
}

static void connect_sta() {
  if (g_settings.wifi_ssid.isEmpty()) return;
  IPAddress ip, gw, mask, dns;
  if (ip.fromString(g_settings.static_ip) && gw.fromString(g_settings.gateway)) {
    if (!mask.fromString(g_settings.subnet)) mask = IPAddress(255, 255, 255, 0);
    if (!dns.fromString(g_settings.dns)) dns = gw;
    WiFi.config(ip, gw, mask, dns);
  }
  WiFi.begin(g_settings.wifi_ssid.c_str(), g_settings.wifi_pass.c_str());
  g_connect_start = millis();
  LOGI(TAG, "Connecting to '%s'", g_settings.wifi_ssid.c_str());
}

void begin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(g_settings.hostname.c_str());
  WiFi.setAutoReconnect(true);
  // Stability: no modem sleep, slightly reduced TX power against brown-outs on weak supplies.
  WiFi.setSleep(false);
  esp_wifi_set_max_tx_power(WIFI_TX_POWER_QDBM);

  if (g_settings.wifi_ssid.isEmpty()) {
    start_ap();
  } else {
    connect_sta();
  }
}

void loop() {
  if (g_ap) g_dns.processNextRequest();

  const bool up = WiFi.status() == WL_CONNECTED;
  if (up && !g_was_connected) {
    g_was_connected = true;
    LOGI(TAG, "Connected, IP %s, RSSI %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    esp_wifi_set_max_tx_power(WIFI_TX_POWER_QDBM);
    if (!g_mdns && MDNS.begin(g_settings.hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      g_mdns = true;
    }
    stop_ap();
  } else if (!up && g_was_connected) {
    g_was_connected = false;
    g_connect_start = millis();
    LOGW(TAG, "Connection lost");
  }

  if (!up && !g_settings.wifi_ssid.isEmpty()) {
    // Fall back to the setup AP after 60 s without connection, keep retrying the station.
    if (!g_ap && millis() - g_connect_start > 60000) start_ap();
    // While a phone is connected to the setup AP, do not retry: a station connect attempt makes the
    // radio scan other channels and drops the AP clients (saving the settings would fail).
    const bool ap_in_use = g_ap && WiFi.softAPgetStationNum() > 0;
    if (ap_in_use && !g_sta_paused) {
      WiFi.disconnect();  // also stops the automatic reconnect of the station
      g_sta_paused = true;
      LOGI(TAG, "Setup AP in use, station retries paused");
    } else if (!ap_in_use && g_sta_paused) {
      g_sta_paused = false;
      g_last_retry = 0;  // retry right away
    }
    if (!ap_in_use && millis() - g_last_retry > 30000 && millis() - g_connect_start > 15000) {
      g_last_retry = millis();
      WiFi.disconnect();
      connect_sta();
    }
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
bool ap_active() { return g_ap; }
String ip() { return connected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); }
int rssi() { return connected() ? WiFi.RSSI() : 0; }

}  // namespace wifi
