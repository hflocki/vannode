#include "alerts.h"
#include "settings.h"
#include "datastore.h"
#include "sensors.h"
#include "weather.h"
#include "mqtt_client.h"
#include "truma_errors.h"
#include <map>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {

std::map<String, bool> states;
SemaphoreHandle_t mtx = nullptr;

struct Lock {
  Lock() {
    if (!mtx) mtx = xSemaphoreCreateMutex();   // first use is in setup, before other tasks run
    xSemaphoreTake(mtx, portMAX_DELAY);
  }
  ~Lock() { xSemaphoreGive(mtx); }
};

// "8,5" with one decimal and a comma, like everywhere on the display
String comma1(float v) {
  String s(v, 1);
  s.replace('.', ',');
  return s;
}

}  // namespace

namespace alerts {

bool latch(const String& key, bool on, bool off) {
  Lock lock;
  bool& st = states[key];
  if (!st && on) st = true;
  else if (st && off) st = false;
  return st;
}

bool below(const String& key, float value, float limit, float hyst) {
  if (limit == 0 || isnan(value)) { latch(key, false, true); return false; }
  return latch(key, value < limit, value >= limit + hyst);
}

bool above(const String& key, float value, float limit, float hyst) {
  if (isnan(limit) || isnan(value)) { latch(key, false, true); return false; }
  return latch(key, value > limit, value <= limit - hyst);
}

String warnings(std::vector<uint8_t>* cats) {
  String w;
  auto add = [&](const String& s, uint8_t cat) {
    if (w.length()) w += "\n";
    w += s;
    if (cats) cats->push_back(cat);
  };
  const float hp = settings.wnHystPct;

  // tanks: only while the tank firmware is online (old retained values are not a warning)
  bool tank = store::devOnline(DEV_TANK);
  float water = tank && store::has(DP_WATER_PCT) ? store::getf(DP_WATER_PCT) : NAN;
  if (below("water", water, settings.wnWater, hp))
    add("Frischwasser niedrig: " + String((int)lroundf(water)) + " %", WC_WATER);
  float gas = tank && store::has(DP_GAS_PCT) ? store::getf(DP_GAS_PCT) : NAN;
  if (below("gas", gas, settings.wnGas, hp))
    add("Gas niedrig: " + String((int)lroundf(gas)) + " %", WC_GAS);

  // frost: measured outside temperature first, otherwise the current weather
  float out = NAN;
  String src;
  if (settings.wnFrostOn) {
    sensors::TempOut o;
    if (sensors::outdoor(o) && !o.stale && !isnan(o.temp)) {
      out = o.temp;
      src = "au\xC3\x9F" "en";
    } else if (settings.wxOn) {
      wx::Data d = wx::get();
      if (d.valid && time(nullptr) - d.at < 3 * 3600) { out = d.temp; src = "Wetter"; }
    }
  }
  // limit 0 °C is a valid frost limit, so the on switch decides here, not the value
  if (!settings.wnFrostOn) latch("frost", false, true);
  else if (latch("frost", !isnan(out) && out < settings.wnFrost,
                 isnan(out) || out >= settings.wnFrost + settings.wnHystT / 10.0f))
    add("Frostgefahr: " + (isnan(out) ? String("--") : comma1(out)) + " \xC2\xB0" "C " + src, WC_FROST);

  // Truma FrostControl: sensor at the drain valve below the limit (the valve opens at about 3 °C
  // and drains the boiler). Off while no sensor is chosen. A silent sensor is reported as well,
  // because the warning would be blind then.
  // After a restart the sensor gets 2 h (its stale time) before it counts as silent.
  float fg = NAN;
  bool fgStale = false;
  if (settings.fgName.length()) {
    sensors::TempOut o;
    if (sensors::byName(settings.fgName, o) && !o.stale) fg = o.temp;
    else fgStale = millis() > 2UL * 60 * 60 * 1000;
  }
  if (below("fguard", fg, settings.fgLimit, settings.wnHystT / 10.0f))
    add("Frostw\xC3\xA4" "chter: " + comma1(fg) + " \xC2\xB0" "C am Boiler \xE2\x80\x93 Ventil \xC3\xB6" "ffnet bald, Wasser l\xC3\xA4" "uft ab", WC_FROST);
  if (latch("fguard_stale", fgStale, !fgStale))
    add("Frostw\xC3\xA4" "chter: Sensor " + settings.fgName + " liefert keine Werte", WC_FROST);

  // awning out and gusts above its own (lower) limit
  float gust = NAN;
  if (settings.wxOn && settings.awnGust > 0 && awning() == 1) {
    wx::Data d = wx::get();
    if (d.valid && time(nullptr) - d.at < 3 * 3600) gust = d.gust;
  }
  if (latch("awning", !isnan(gust) && gust >= settings.awnGust,
            isnan(gust) || gust < settings.awnGust - settings.wxGustHyst))
    add("Markise einfahren: B\xC3\xB6" "en bis " + (isnan(gust) ? String("--") : String((int)lroundf(gust))) + " km/h", WC_WEATHER);
  return w;
}

String heaterFault() {
  if (!store::devOnline(DEV_HEATER) || store::get(DP_HEAT_ERROR) != "ON") return "";
  int code = store::get(DP_HEAT_ERRCODE).toInt();
  // the model of the heater-node decides the meaning (Combi D: 121 = dosing pump ...);
  // older heater firmware sends no model: gas Combi
  truma_err::Info e = truma_err::lookup(code, truma_err::family(store::get(DP_HEAT_MODEL).c_str()));
  String s;
  if (e.kind) s = String(e.kind) + " " + String(code) + " H";        // like the CP Plus: "E 122 H"
  else s = String(store::get(DP_HEAT_OPSTATUS) == "warning" ? "Warnung " : "Fehler ") + String(code);
  if (e.text) s += String(": ") + e.text;
  return s;
}

bool awningIsSwitch() { return settings.awnSrc.isEmpty(); }

int8_t awning() {
  if (awningIsSwitch()) return settings.awnOut ? 1 : 0;
  bool stale;
  int8_t s = sensors::contactState(settings.awnSrc, stale);
  return stale ? -1 : s;
}

void setAwning(bool out) {
  if (!awningIsSwitch() || settings.awnOut == out) return;
  settings.awnOut = out;
  cfg::saveAwning();
  mqttc::publishSelfState();
}

}  // namespace alerts
