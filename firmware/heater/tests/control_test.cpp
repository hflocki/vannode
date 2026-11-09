// Host test for heater_control.cpp: MQTT / web commands -> InetBox commands.
// No LIN traffic here: the CP Plus status is written straight into the InetBox state, the
// commands are read back from its command queue. The LIN side is covered by protocol_test/replay.
#define private public
#include "inetbox.h"
#undef private
#include "config.h"
#include "heater_control.h"
#include "heater_model.h"
#include "logger.h"
#include "mqtt_ha.h"
#include "settings.h"
#include <cstdarg>
#include <string>
#include <vector>
using namespace truma;

uint32_t g_us = 1000000;
int uart_write_bytes(uart_port_t, const char *, size_t n) { return n; }
Settings g_settings;
int g_saves = 0, g_discovery = 0;
void settings_save_heater() { g_saves++; }
namespace mqtt { void request_discovery() { g_discovery++; } }
std::vector<std::string> g_log;
void log_write(LogLevel, const char *tag, const char *fmt, ...) {
  char b[256]; va_list a; va_start(a, fmt); vsnprintf(b, sizeof(b), fmt, a); va_end(a);
  g_log.push_back(std::string(tag) + ": " + b);
}

static InetBox box;
static int fails = 0, total = 0;
static void check(bool ok, const char *what) {
  total++;
  if (!ok) { fails++; printf("FAIL %s\n", what); }
}
static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

// Next queued command; false if the queue is empty.
static bool pop(Command &c) { return xQueueReceive(box.cmd_queue_, &c, 0) == pdTRUE; }
static int queued() { return (int) ((FakeQ *) box.cmd_queue_)->q.size(); }
static void drain() { Command c; while (pop(c)) {} }

// CP Plus status: room target (0 = off), fan mode, running or not
static void status(float room, HeatingMode mode, bool running = false) {
  box.state_.heater_valid = true;
  box.state_.heater.target_temp_room = room < 5 ? TargetTemp::OFF : decimal_to_temp(room);
  box.state_.heater.heating_mode = room < 5 ? HeatingMode::MODE_OFF : mode;
  box.state_.heater.operating_status = running ? OperatingStatus::START_OR_COOL_DOWN : OperatingStatus::OFF;
  box.state_.update_pending = false;
  heater::loop();
}

static void reset(const char *model) {
  g_settings = Settings();
  g_settings.heater_model = model;
  g_settings.fan_mode = "eco";
  g_settings.room_setpoint = 20;
  box.state_ = TrumaState();
  heater::decoke_stop(false);
  heater::loop();
  drain();
}

int main() {
  box.cmd_queue_ = xQueueCreate(8, sizeof(Command));
  heater::begin(&box);
  Command c;

  // --- no status from the CP Plus yet: nothing is sent
  reset("combi_d");
  check(!heater::set_room_temp(21) && queued() == 0, "no status: command refused");

  // --- room heating on/off with the remembered setpoint and fan mode
  reset("combi_d");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_room_on(true) && pop(c) && c.type == CommandType::SET_ROOM && near(c.value, 20) &&
            c.mode == HeatingMode::MODE_ECO, "room on: 20 C eco");
  check(heater::set_room_on(false) && pop(c) && c.type == CommandType::SET_ROOM && c.value == 0 &&
            c.mode == HeatingMode::MODE_OFF, "room off");

  // --- setpoint while off: switch on with that temperature; clamped to 30
  check(heater::set_room_temp(22.5) && pop(c) && c.type == CommandType::SET_ROOM && near(c.value, 22.5),
        "room temp while off switches on");
  check(near(g_settings.room_setpoint, 22.5) && g_saves > 0, "room temp remembered and saved");
  check(heater::set_room_temp(35) && pop(c) && near(c.value, 30), "room temp clamped to 30");
  check(heater::set_room_temp(3) && pop(c) && c.type == CommandType::SET_ROOM && c.mode == HeatingMode::MODE_OFF,
        "room temp below 5 = off");

  // --- setpoint while on: only the temperature changes
  status(21, HeatingMode::MODE_HIGH, true);
  check(heater::set_room_temp(19) && pop(c) && c.type == CommandType::SET_ROOM_TEMP && near(c.value, 19),
        "room temp while on");
  check(!strcmp(heater::hvac_action(), "heating") && !strcmp(heater::fan_mode(), "high"), "hvac heating, fan high");

  // --- HA thermostat setpoint: only remembered while off
  status(0, HeatingMode::MODE_OFF);
  drain();
  check(heater::set_setpoint(18) && queued() == 0 && near(heater::room_setpoint(), 18), "HA setpoint while off: remembered only");
  check(heater::set_setpoint(2) && near(g_settings.room_setpoint, 5), "HA setpoint clamped to 5");
  check(!strcmp(heater::hvac_action(), "off"), "hvac off");

  // --- fan modes per model
  reset("combi_d");
  status(0, HeatingMode::MODE_OFF);
  check(!heater::set_fan_mode("boost", true) && queued() == 0, "Combi D: no boost");
  check(heater::set_fan_mode("HIGH ", false) && queued() == 0 && g_settings.fan_mode == "high",
        "fan mode while off (HA): remembered only, trimmed + lower case");
  check(heater::set_fan_mode("eco", true) && pop(c) && c.type == CommandType::SET_ROOM && c.mode == HeatingMode::MODE_ECO,
        "fan mode with power on switches on");
  status(20, HeatingMode::MODE_ECO, true);
  check(heater::set_fan_mode("high", true) && pop(c) && c.type == CommandType::SET_FAN_MODE && c.mode == HeatingMode::MODE_HIGH,
        "fan mode while on");
  check(heater::set_fan_mode("off", true) && pop(c) && c.type == CommandType::SET_ROOM && c.mode == HeatingMode::MODE_OFF,
        "fan mode off = room off");
  const char *fm[4];
  check(heater::fan_modes(fm, 4) == 2 && !strcmp(fm[0], "eco") && !strcmp(fm[1], "high"), "Combi D fan modes");

  reset("combi");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_fan_mode("boost", true) && pop(c) && c.mode == HeatingMode::MODE_BOOST, "Combi gas: boost");
  check(heater::fan_modes(fm, 4) == 3, "Combi gas fan modes");

  reset("vario");
  status(0, HeatingMode::MODE_OFF);
  check(!heater::set_fan_mode("eco", true), "VarioHeat: no eco");
  check(heater::set_room_on(true) && pop(c) && c.mode == HeatingMode::MODE_AUTO, "VarioHeat: switch on with auto");
  check(heater::fan_modes(fm, 4) == 3 && !strcmp(fm[0], "night"), "VarioHeat fan modes");

  // --- model switch keeps a valid fan mode and refreshes discovery
  reset("combi");
  g_settings.fan_mode = "boost";
  int d0 = g_discovery;
  check(heater::set_model("vario") && g_settings.fan_mode == "boost" && g_discovery == d0 + 1, "model -> vario keeps boost");
  check(heater::set_model("combi_d") && g_settings.fan_mode == "eco", "model -> combi_d: boost replaced by eco");
  check(!heater::set_model("foo") && g_settings.heater_model == "combi_d", "unknown model refused");
  check(heater::model_mismatch(Device::HEATER_VARIO) != nullptr && heater::model_mismatch(Device::HEATER_COMBI6D) == nullptr,
        "model mismatch hint");

  // --- hot water
  reset("combi_d");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_water_mode("eco") && pop(c) && c.type == CommandType::SET_WATER && near(c.value, 40), "water eco = 40");
  check(heater::set_water_mode("boost") && pop(c) && near(c.value, 60), "Combi D: water boost becomes 60");
  check(!heater::set_water_mode("lukewarm") && queued() == 0, "unknown water mode refused");
  reset("combi");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_water_mode("boost") && pop(c) && near(c.value, 200), "Combi gas: water boost = 200");
  reset("vario");
  status(0, HeatingMode::MODE_OFF);
  check(!heater::set_water_mode("eco") && queued() == 0, "VarioHeat: no water");

  // --- energy source
  reset("combi_d");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_energy_mix("diesel", 0) && pop(c) && c.mix == EnergyMix::GAS, "Combi D: diesel");
  check(!heater::set_energy_mix("mix", 900) && queued() == 0, "Combi D: no 230 V");
  reset("combi_d_e");
  status(0, HeatingMode::MODE_OFF);
  check(heater::set_energy_mix("Mix", 1800) && pop(c) && c.mix == EnergyMix::MIX &&
            c.level == decimal_to_el_power_level(1800), "Combi D E: mix 1800 W");
  check(!heater::set_energy_mix("solar", 0), "unknown energy source refused");

  // --- timer
  reset("combi_d");
  status(0, HeatingMode::MODE_OFF);
  check(!heater::set_timer("22:00-06:30") && queued() == 0, "timer without timer status refused");
  box.state_.timer_valid = true;
  box.state_.timer.timer_start_hours = 21; box.state_.timer.timer_start_minutes = 15;
  box.state_.timer.timer_stop_hours = 7;   box.state_.timer.timer_stop_minutes = 0;
  heater::loop();
  check(heater::set_timer("22:00-06:30,21,high,eco") && pop(c) && c.type == CommandType::SET_TIMER &&
            c.start == 22 * 60 && c.stop == 6 * 60 + 30 && near(c.value, 21) && c.mode == HeatingMode::MODE_HIGH &&
            near(c.water, 40), "timer full spec");
  check(heater::set_timer(" 05:00-07:00 ") && pop(c) && near(c.value, 20) && c.mode == HeatingMode::MODE_ECO &&
            isnan(c.water), "timer defaults: remembered setpoint/fan, water unchanged");
  check(heater::set_timer("on") && pop(c) && c.type == CommandType::SET_TIMER && c.start == 21 * 60 + 15 && c.stop == 7 * 60,
        "timer on: stored times");
  check(heater::set_timer("off") && pop(c) && c.type == CommandType::TIMER_OFF, "timer off");
  check(!heater::set_timer("25:00-06:00") && !heater::set_timer("06:00-06:00") && !heater::set_timer("22:00-06:00,20,boost") &&
            queued() == 0, "invalid timers refused");

  // --- decoking (diesel only): 30 C + HIGH, then restore
  reset("combi");
  status(20, HeatingMode::MODE_ECO, true);
  check(!heater::decoke_start() && queued() == 0, "decoking only on diesel");
  reset("combi_d");
  g_settings.decoke_minutes = 2;
  status(19, HeatingMode::MODE_ECO, true);
  check(heater::decoke_start() && pop(c) && c.type == CommandType::SET_ROOM && near(c.value, DECOKE_TARGET_TEMP) &&
            c.mode == HeatingMode::MODE_HIGH && heater::decoke_active(), "decoking started");
  check(heater::decoke_remaining_s() == 120, "decoking 2 min");
  status(DECOKE_TARGET_TEMP, HeatingMode::MODE_HIGH, true);    // CP Plus confirms
  check(heater::decoke_active() && near(heater::room_setpoint(), 19), "decoking confirmed, setpoint shows the old value");
  g_us += 121 * 1000000UL;
  heater::loop();
  check(!heater::decoke_active() && pop(c) && c.type == CommandType::SET_ROOM && near(c.value, 19) &&
            c.mode == HeatingMode::MODE_ECO, "decoking done: 19 C eco restored");
  status(19, HeatingMode::MODE_ECO, true);
  check(heater::decoke_start() && pop(c), "decoking again");
  status(DECOKE_TARGET_TEMP, HeatingMode::MODE_HIGH, true);
  status(22, HeatingMode::MODE_ECO, true);                      // changed on the CP Plus
  check(!heater::decoke_active() && queued() == 0, "decoking cancelled by a change on the CP Plus, nothing restored");
  check(heater::decoke_start() && pop(c) && heater::set_room_temp(21) && !heater::decoke_active(), "decoking cancelled by user");

  // --- following the CP Plus: last used values for the next switch-on
  reset("combi_d");
  status(23, HeatingMode::MODE_HIGH, true);
  check(near(g_settings.room_setpoint, 23) && g_settings.fan_mode == "high", "follows CP Plus setpoint and fan mode");

  // --- error code
  box.state_.heater.error_code_high = 1; box.state_.heater.error_code_low = 12;
  heater::loop();
  check(heater::error_code() == 112, "error code high*100+low");

  printf("control_test: %d/%d passed\n", total - fails, total);
  return fails ? 1 : 0;
}
