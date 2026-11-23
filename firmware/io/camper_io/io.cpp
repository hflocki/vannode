// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "io.h"
#include "evlog.h"

#include <LittleFS.h>
#include <Preferences.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

IoChannel ioCh[IO_MAX_CH];
IoLive ioLive[IO_MAX_CH];
uint8_t ioCount = 0;
IoHardware ioHw;

static OneWire* ow = nullptr;
static DallasTemperature* dallas = nullptr;
static uint8_t srBits[IO_SR_MAX];          // 74HC595 output levels (bit = pin level)
static uint16_t expOut[IO_EXP_MAX];        // expander output latch (bit = pin level)
static uint16_t expInMask[IO_EXP_MAX];     // expander pins used as inputs
static uint16_t expOutMask[IO_EXP_MAX];    // expander pins used as relay outputs (all others stay inputs)
static uint16_t expPullup[IO_EXP_MAX];
static bool expOk[IO_EXP_MAX];
static uint16_t expIn[IO_EXP_MAX];         // last read input levels

static uint32_t lastPoll = 0, lastMeasure = 0, convStart = 0;
static bool converting = false, measureFirst = true;
static bool stateSaveDue = false;
static uint32_t stateChanged = 0;
static volatile bool scanRequested = false;

// last scan (web UI "Suchen")
static uint8_t scanRom[IO_MAX_CH][8];
static float scanTemp[IO_MAX_CH];
static uint8_t scanRomCount = 0;
static uint8_t scanI2c[16];
static uint8_t scanI2cCount = 0;
static bool scanDone = false;

const char* ioTypeName(uint8_t t) {
    switch (t) {
        case IO_RELAY: return "relay";
        case IO_INPUT: return "input";
        case IO_TEMP: return "temp";
        case IO_ANALOG: return "analog";
    }
    return "?";
}

static int typeFromName(const char* s) {
    if (!s) return -1;
    if (!strcmp(s, "relay")) return IO_RELAY;
    if (!strcmp(s, "input")) return IO_INPUT;
    if (!strcmp(s, "temp")) return IO_TEMP;
    if (!strcmp(s, "analog")) return IO_ANALOG;
    return -1;
}

static const char* busName(uint8_t b) { return b == BUS_SR ? "sr" : b == BUS_EXP ? "exp" : "gpio"; }
static int busFromName(const char* s) {
    if (!s || !strcmp(s, "gpio")) return BUS_GPIO;
    if (!strcmp(s, "sr")) return BUS_SR;
    if (!strcmp(s, "exp")) return BUS_EXP;
    return -1;
}

static uint8_t expPins() { return ioHw.expType == 1 ? 8 : 16; }

// ---- GPIO rules of the chip ----------------------------------------------------------
// ESP32-C3: GPIO 0-10, 20, 21 on the header (18/19 = USB, 11-17 = flash). ADC1 on GPIO 0-4.
static bool gpioOk(int p) {
#if CONFIG_IDF_TARGET_ESP32C3
    return (p >= 0 && p <= 10) || p == 20 || p == 21;
#elif CONFIG_IDF_TARGET_ESP32S3
    return (p >= 0 && p <= 18) || p == 21 || (p >= 38 && p <= 48);   // 19/20 USB, 26-37 flash/PSRAM
#else
    return p >= 0 && p <= 39;
#endif
}
static bool adcOk(int p) {
#if CONFIG_IDF_TARGET_ESP32C3
    return p >= 0 && p <= 4;
#elif CONFIG_IDF_TARGET_ESP32S3
    return p >= 1 && p <= 10;
#else
    return p >= 32 && p <= 39;
#endif
}

// ---- defaults / persistence ------------------------------------------------------------
static void hwDefaults(IoHardware& h) {
    h.ow = -1;
    h.srData = h.srClock = h.srLatch = h.srOe = -1;
    h.srChips = 0;
    h.sda = h.scl = -1;
    h.expType = 0;
    h.expAddr = 0x20;
    h.expChips = 0;
    h.interval = 10;
}

static void chDefaults(IoChannel& c) {
    memset(&c, 0, sizeof(c));
    c.type = IO_RELAY;
    c.debounce = 50;
    c.pullup = true;
    c.raw2 = 1000; c.val2 = 1;   // 1:1 volt until set up
    c.dec = 2;
}

static bool hexRom(const char* s, uint8_t* rom) {
    if (!s || strlen(s) != 16) return false;
    for (int i = 0; i < 8; i++) {
        char b[3] = {s[2 * i], s[2 * i + 1], 0};
        char* e;
        long v = strtol(b, &e, 16);
        if (*e) return false;
        rom[i] = (uint8_t)v;
    }
    return true;
}
static String romHex(const uint8_t* rom) {
    char b[17];
    for (int i = 0; i < 8; i++) sprintf(b + 2 * i, "%02X", rom[i]);
    return String(b);
}

static void hwToJson(JsonObject h, const IoHardware& x) {
    h["ow"] = x.ow;
    h["sr_data"] = x.srData; h["sr_clock"] = x.srClock; h["sr_latch"] = x.srLatch; h["sr_oe"] = x.srOe;
    h["sr_chips"] = x.srChips;
    h["sda"] = x.sda; h["scl"] = x.scl;
    h["exp_type"] = x.expType == 1 ? "pcf8574" : "mcp23017";
    h["exp_addr"] = x.expAddr;
    h["exp_chips"] = x.expChips;
    h["interval"] = x.interval;
}

static void chToJson(JsonObject o, const IoChannel& c) {
    o["id"] = c.id;
    o["name"] = c.name;
    o["type"] = ioTypeName(c.type);
    switch (c.type) {
        case IO_RELAY:
            o["bus"] = busName(c.bus); o["chip"] = c.chip; o["pin"] = c.pin;
            o["low"] = c.low; o["boot"] = c.boot; o["pulse"] = c.pulse;
            break;
        case IO_INPUT:
            o["bus"] = busName(c.bus); o["chip"] = c.chip; o["pin"] = c.pin;
            o["low"] = c.low; o["pullup"] = c.pullup; o["debounce"] = c.debounce;
            break;
        case IO_TEMP:
            o["rom"] = romHex(c.rom); o["offset"] = c.offset;
            break;
        case IO_ANALOG:
            o["pin"] = c.pin; o["raw1"] = c.raw1; o["val1"] = c.val1; o["raw2"] = c.raw2; o["val2"] = c.val2;
            o["unit"] = c.unit; o["dec"] = c.dec;
            break;
    }
}

static void setJson(JsonObject o, const IoHardware& hw, const IoChannel* ch, uint8_t n) {
    hwToJson(o["hw"].to<JsonObject>(), hw);
    JsonArray a = o["ch"].to<JsonArray>();
    for (uint8_t i = 0; i < n; i++) chToJson(a.add<JsonObject>(), ch[i]);
}

void ioJson(JsonObject o) {
    setJson(o, ioHw, ioCh, ioCount);
    o["max_ch"] = IO_MAX_CH;
    o["sr_max"] = IO_SR_MAX;
    o["exp_max"] = IO_EXP_MAX;
#if CONFIG_IDF_TARGET_ESP32C3
    o["chip"] = "c3";
#elif CONFIG_IDF_TARGET_ESP32S3
    o["chip"] = "s3";
#endif
}

static bool validId(const char* s) {
    size_t n = strlen(s);
    if (n == 0 || n >= sizeof(IoChannel::id)) return false;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    // reserved sub topics of the node itself
    return strcmp(s, "status") && strcmp(s, "info") && strcmp(s, "rssi") && strcmp(s, "cmd");
}

static int8_t pinVal(JsonVariantConst v) {
    if (v.isNull()) return -1;
    int p = v.as<int>();
    return (p < 0 || p > 48) ? -1 : (int8_t)p;
}

// Parses and checks the whole set; fills h/ch only if everything is fine.
static const char* parse(JsonObjectConst in, IoHardware& h, IoChannel* ch, uint8_t& count) {
    static char err[96];
    hwDefaults(h);
    JsonObjectConst jh = in["hw"];
    if (!jh.isNull()) {
        h.ow = pinVal(jh["ow"]);
        h.srData = pinVal(jh["sr_data"]); h.srClock = pinVal(jh["sr_clock"]);
        h.srLatch = pinVal(jh["sr_latch"]); h.srOe = pinVal(jh["sr_oe"]);
        h.srChips = jh["sr_chips"] | 0;
        h.sda = pinVal(jh["sda"]); h.scl = pinVal(jh["scl"]);
        const char* et = jh["exp_type"] | "mcp23017";
        h.expType = strcmp(et, "pcf8574") == 0 ? 1 : 0;
        h.expAddr = jh["exp_addr"] | 0x20;
        h.expChips = jh["exp_chips"] | 0;
        h.interval = jh["interval"] | 10;
    }
    if (h.interval < 2 || h.interval > 3600) return "Intervall 2-3600 s";
    if (h.srChips > IO_SR_MAX) return "Zu viele 74HC595";
    if (h.srChips && (h.srData < 0 || h.srClock < 0 || h.srLatch < 0)) return "74HC595: Daten-, Takt- und Latch-Pin fehlen";
    if (h.expChips > IO_EXP_MAX) return "Zu viele Expander";
    if (h.expChips && (h.sda < 0 || h.scl < 0)) return "Expander: SDA- und SCL-Pin fehlen";
    if (h.expChips && (h.expAddr < 0x08 || h.expAddr + h.expChips - 1 > 0x77)) return "Expander: Adresse ungültig";

    // every GPIO only once
    int8_t used[64];
    memset(used, 0, sizeof(used));
    auto take = [&](int p, const char* what) -> const char* {
        if (p < 0) return nullptr;
        if (!gpioOk(p)) { snprintf(err, sizeof(err), "%s: GPIO%d gibt es auf diesem Chip nicht", what, p); return err; }
        if (used[p]) { snprintf(err, sizeof(err), "GPIO%d ist doppelt belegt (%s)", p, what); return err; }
        used[p] = 1;
        return nullptr;
    };
    const char* e;
    if ((e = take(h.ow, "1-Wire"))) return e;
    if (h.srChips) {
        if ((e = take(h.srData, "74HC595 Daten"))) return e;
        if ((e = take(h.srClock, "74HC595 Takt"))) return e;
        if ((e = take(h.srLatch, "74HC595 Latch"))) return e;
        if ((e = take(h.srOe, "74HC595 OE"))) return e;
    }
    if (h.expChips) {
        if ((e = take(h.sda, "I2C SDA"))) return e;
        if ((e = take(h.scl, "I2C SCL"))) return e;
    }

    JsonArrayConst a = in["ch"];
    count = 0;
    uint8_t srUsed[IO_SR_MAX * 8];
    memset(srUsed, 0, sizeof(srUsed));
    uint16_t expUsed[IO_EXP_MAX];
    memset(expUsed, 0, sizeof(expUsed));
    for (JsonObjectConst o : a) {
        if (count >= IO_MAX_CH) return "Zu viele Kanäle";
        IoChannel& c = ch[count];
        chDefaults(c);
        strlcpy(c.id, o["id"] | "", sizeof(c.id));
        strlcpy(c.name, o["name"] | "", sizeof(c.name));
        if (!validId(c.id)) { snprintf(err, sizeof(err), "Ungültige ID: %s", c.id); return err; }
        for (uint8_t j = 0; j < count; j++)
            if (!strcmp(ch[j].id, c.id)) { snprintf(err, sizeof(err), "ID doppelt: %s", c.id); return err; }
        if (!c.name[0]) strlcpy(c.name, c.id, sizeof(c.name));
        int t = typeFromName(o["type"] | "");
        if (t < 0) { snprintf(err, sizeof(err), "%s: Typ unbekannt", c.id); return err; }
        c.type = (uint8_t)t;
        if (c.type == IO_RELAY || c.type == IO_INPUT) {
            int b = busFromName(o["bus"] | "gpio");
            if (b < 0 || (c.type == IO_INPUT && b == BUS_SR)) { snprintf(err, sizeof(err), "%s: Anschluss ungültig", c.id); return err; }
            c.bus = (uint8_t)b;
            c.chip = o["chip"] | 0;
            int p = o["pin"] | -1;
            c.low = o["low"] | false;
            if (c.bus == BUS_GPIO) {
                if ((e = take(p, c.id))) return e;
                if (p < 0) { snprintf(err, sizeof(err), "%s: GPIO fehlt", c.id); return err; }
            } else if (c.bus == BUS_SR) {
                if (!h.srChips || p < 0 || p >= h.srChips * 8) { snprintf(err, sizeof(err), "%s: 74HC595-Ausgang ungültig", c.id); return err; }
                if (srUsed[p]) { snprintf(err, sizeof(err), "%s: 74HC595-Ausgang %d doppelt", c.id, p); return err; }
                srUsed[p] = 1;
            } else {
                uint8_t np = h.expType == 1 ? 8 : 16;
                if (c.chip >= h.expChips || p < 0 || p >= np) { snprintf(err, sizeof(err), "%s: Expander-Pin ungültig", c.id); return err; }
                if (expUsed[c.chip] & (1u << p)) { snprintf(err, sizeof(err), "%s: Expander %u Pin %d doppelt", c.id, c.chip + 1, p); return err; }
                expUsed[c.chip] |= (1u << p);
            }
            c.pin = (uint8_t)p;
            if (c.type == IO_RELAY) {
                c.boot = o["boot"] | 0;
                if (c.boot > BOOT_LAST) c.boot = BOOT_OFF;
                long pl = o["pulse"] | 0L;
                if (pl < 0 || pl > 3600000L) { snprintf(err, sizeof(err), "%s: Impuls 0-3600000 ms", c.id); return err; }
                c.pulse = (uint32_t)pl;
            } else {
                c.pullup = o["pullup"] | true;
                int db = o["debounce"] | 50;
                if (db < 0 || db > 10000) { snprintf(err, sizeof(err), "%s: Entprellen 0-10000 ms", c.id); return err; }
                c.debounce = (uint16_t)db;
            }
        } else if (c.type == IO_TEMP) {
            if (h.ow < 0) return "Temperatur: erst den 1-Wire-Pin setzen";
            if (!hexRom(o["rom"] | "", c.rom)) { snprintf(err, sizeof(err), "%s: Sensor-Adresse ungültig", c.id); return err; }
            c.offset = o["offset"] | 0.0f;
            if (fabsf(c.offset) > 10) { snprintf(err, sizeof(err), "%s: Offset max. ±10 °C", c.id); return err; }
        } else {   // analog
            int p = o["pin"] | -1;
            if ((e = take(p, c.id))) return e;
            if (!adcOk(p)) { snprintf(err, sizeof(err), "%s: GPIO%d kann nicht analog messen", c.id, p); return err; }
            c.pin = (uint8_t)p;
            c.raw1 = o["raw1"] | 0.0f; c.val1 = o["val1"] | 0.0f;
            c.raw2 = o["raw2"] | 1000.0f; c.val2 = o["val2"] | 1.0f;
            if (c.raw1 == c.raw2) { snprintf(err, sizeof(err), "%s: die zwei Punkte brauchen verschiedene mV", c.id); return err; }
            strlcpy(c.unit, o["unit"] | "", sizeof(c.unit));
            int d = o["dec"] | 2;
            c.dec = (uint8_t)constrain(d, 0, 4);
        }
        count++;
    }
    return nullptr;
}

static void saveFile(const IoHardware& hw, const IoChannel* ch, uint8_t n) {
    JsonDocument doc;
    setJson(doc.to<JsonObject>(), hw, ch, n);
    File f = LittleFS.open("/io.tmp", "w");
    if (!f) return;
    size_t written = serializeJson(doc, f);
    f.close();
    if (!written) { LittleFS.remove("/io.tmp"); return; }
    LittleFS.remove(IO_FILE);
    LittleFS.rename("/io.tmp", IO_FILE);
}

void ioLoad() {
    hwDefaults(ioHw);
    ioCount = 0;
    File f = LittleFS.open(IO_FILE, "r");
    if (!f) return;
    JsonDocument doc;
    DeserializationError e = deserializeJson(doc, f);
    f.close();
    if (e) { evlog::add("io.json nicht lesbar (%s)", e.c_str()); return; }
    static IoChannel tmp[IO_MAX_CH];
    IoHardware h;
    uint8_t n = 0;
    const char* err = parse(doc.as<JsonObjectConst>(), h, tmp, n);
    if (err) { evlog::add("io.json: %s", err); return; }
    ioHw = h;
    memcpy(ioCh, tmp, sizeof(IoChannel) * n);
    ioCount = n;
}

// Validated in the web task, written to flash in ioLoop() (no flash writes in the web task)
static IoChannel pendCh[IO_MAX_CH];
static IoHardware pendHw;
static uint8_t pendCount = 0;
static String pendRemoved;
static volatile bool pendSave = false;

const char* ioApply(JsonObjectConst in, String& removed) {
    if (pendSave) return "Speichern läuft noch";
    static IoChannel tmp[IO_MAX_CH];
    IoHardware h;
    uint8_t n = 0;
    const char* err = parse(in, h, tmp, n);
    if (err) return err;
    // ids that are gone: MQTT clears their discovery and retained state after the restart
    removed = "";
    for (uint8_t i = 0; i < ioCount; i++) {
        bool keep = false;
        for (uint8_t j = 0; j < n; j++) if (!strcmp(ioCh[i].id, tmp[j].id) && ioCh[i].type == tmp[j].type) keep = true;
        if (!keep) { removed += ioCh[i].id; removed += ':'; removed += ioTypeName(ioCh[i].type); removed += ' '; }
    }
    memcpy(pendCh, tmp, sizeof(IoChannel) * n);
    pendHw = h;
    pendCount = n;
    pendRemoved = removed;
    pendSave = true;
    return nullptr;
}

static void savePending() {
    // the new set is only stored; hardware and states start fresh after the restart
    saveFile(pendHw, pendCh, pendCount);
    if (pendRemoved.length()) {
        Preferences p;
        p.begin("iorm", false);
        String prev = p.getString("rm", "");
        if (prev.length() + pendRemoved.length() < 1500) p.putString("rm", prev + pendRemoved);
        p.end();
    }
    evlog::add("Kanäle gespeichert (%u)", (unsigned)pendCount);
    pendSave = false;
}

// ---- relay states over a restart (only relays with "letzter Zustand") ------------------
// NVS keys are max. 15 characters, so the key is a hash of the channel id.
static String stateKey(const char* id) {
    uint32_t h = 2166136261u;
    for (const char* c = id; *c; c++) { h ^= (uint8_t)*c; h *= 16777619u; }
    char b[12];
    snprintf(b, sizeof(b), "s%08lx", (unsigned long)h);
    return String(b);
}

static void loadStates(bool* st) {
    Preferences p;
    p.begin("io", true);
    for (uint8_t i = 0; i < ioCount; i++) st[i] = p.getBool(stateKey(ioCh[i].id).c_str(), false);
    p.end();
}
static void saveStates() {
    Preferences p;
    p.begin("io", false);
    for (uint8_t i = 0; i < ioCount; i++)
        if (ioCh[i].type == IO_RELAY && ioCh[i].boot == BOOT_LAST) {
            bool v = ioLive[i].on;
            String k = stateKey(ioCh[i].id);
            if (p.getBool(k.c_str(), !v) != v) p.putBool(k.c_str(), v);
        }
    p.end();
}

// ---- outputs -------------------------------------------------------------------------
static void srWrite() {
    if (!ioHw.srChips) return;
    digitalWrite(ioHw.srLatch, LOW);
    for (int8_t c = ioHw.srChips - 1; c >= 0; c--) shiftOut(ioHw.srData, ioHw.srClock, MSBFIRST, srBits[c]);
    digitalWrite(ioHw.srLatch, HIGH);
}

static bool expWriteChip(uint8_t chip) {
    uint8_t a = ioHw.expAddr + chip;
    Wire.beginTransmission(a);
    if (ioHw.expType == 1) {
        // PCF8574: input pins must stay HIGH (weak pull-up), outputs as wanted
        Wire.write((uint8_t)(((expOut[chip] & expOutMask[chip]) | ~expOutMask[chip]) & 0xFF));
    } else {
        Wire.write(0x14);   // OLATA, OLATB follows
        Wire.write((uint8_t)(expOut[chip] & 0xFF));
        Wire.write((uint8_t)(expOut[chip] >> 8));
    }
    return Wire.endTransmission() == 0;
}

static bool mcpReg(uint8_t a, uint8_t reg, uint16_t v) {
    Wire.beginTransmission(a);
    Wire.write(reg);
    Wire.write((uint8_t)(v & 0xFF));
    Wire.write((uint8_t)(v >> 8));
    return Wire.endTransmission() == 0;
}

static void setOutput(uint8_t i, bool on) {
    const IoChannel& c = ioCh[i];
    bool level = on != c.low;   // active LOW: on -> LOW
    switch (c.bus) {
        case BUS_GPIO: digitalWrite(c.pin, level ? HIGH : LOW); break;
        case BUS_SR:
            if (level) srBits[c.pin / 8] |= (1 << (c.pin % 8)); else srBits[c.pin / 8] &= ~(1 << (c.pin % 8));
            srWrite();
            break;
        case BUS_EXP:
            if (level) expOut[c.chip] |= (1u << c.pin); else expOut[c.chip] &= ~(1u << c.pin);
            if (expOk[c.chip]) expWriteChip(c.chip);
            break;
    }
}

static void relaySet(uint8_t i, bool on, bool fromBoot = false) {
    IoLive& l = ioLive[i];
    l.offAt = (on && ioCh[i].pulse) ? (millis() + ioCh[i].pulse) | 1 : 0;
    if (!fromBoot && l.on == on) return;
    l.on = on;
    l.dirty = true;
    setOutput(i, on);
    if (ioCh[i].boot == BOOT_LAST) { stateSaveDue = true; stateChanged = millis(); }
}

// Commands come from the web task and from MQTT; they are carried out in ioLoop() only,
// so outputs (I2C, shift register) are never driven from two tasks at once.
static volatile uint8_t cmdHead = 0, cmdTail = 0;
static uint8_t cmdCh[16], cmdAct[16];   // act: 0 off, 1 on, 2 toggle
static portMUX_TYPE cmdMux = portMUX_INITIALIZER_UNLOCKED;

bool ioCommand(const char* id, const char* payload) {
    for (uint8_t i = 0; i < ioCount; i++) {
        if (strcmp(ioCh[i].id, id) || ioCh[i].type != IO_RELAY) continue;
        String p = payload;
        p.trim();
        p.toUpperCase();
        uint8_t act;
        if (p == "ON" || p == "1" || p == "TRUE") act = 1;
        else if (p == "OFF" || p == "0" || p == "FALSE") act = 0;
        else if (p == "TOGGLE") act = 2;
        else return false;
        bool ok = false;
        portENTER_CRITICAL(&cmdMux);
        uint8_t next = (cmdHead + 1) % 16;
        if (next != cmdTail) { cmdCh[cmdHead] = i; cmdAct[cmdHead] = act; cmdHead = next; ok = true; }
        portEXIT_CRITICAL(&cmdMux);
        return ok;
    }
    return false;
}

static void runCommands() {
    while (cmdTail != cmdHead) {
        portENTER_CRITICAL(&cmdMux);
        uint8_t i = cmdCh[cmdTail], act = cmdAct[cmdTail];
        cmdTail = (cmdTail + 1) % 16;
        portEXIT_CRITICAL(&cmdMux);
        if (i < ioCount) relaySet(i, act == 2 ? !ioLive[i].on : act == 1);
    }
}

String ioTakeRemoved() {
    Preferences p;
    p.begin("iorm", false);
    String r = p.getString("rm", "");
    if (r.length()) p.remove("rm");
    p.end();
    return r;
}

// ---- inputs --------------------------------------------------------------------------
static void readExpanders() {
    for (uint8_t c = 0; c < ioHw.expChips; c++) {
        if (!expOk[c] || !expInMask[c]) continue;
        uint8_t a = ioHw.expAddr + c;
        if (ioHw.expType == 1) {
            if (Wire.requestFrom(a, (uint8_t)1) == 1) expIn[c] = Wire.read();
        } else {
            Wire.beginTransmission(a);
            Wire.write(0x12);   // GPIOA, GPIOB follows
            if (Wire.endTransmission(false) != 0) continue;
            if (Wire.requestFrom(a, (uint8_t)2) == 2) { uint8_t lo = Wire.read(); expIn[c] = lo | (Wire.read() << 8); }
        }
    }
}

static bool rawInput(const IoChannel& c) {
    bool level = c.bus == BUS_EXP ? (expIn[c.chip] >> c.pin) & 1 : digitalRead(c.pin) == HIGH;
    return level != c.low;   // inverted: LOW = on
}

static void pollInputs() {
    readExpanders();
    uint32_t now = millis();
    for (uint8_t i = 0; i < ioCount; i++) {
        if (ioCh[i].type != IO_INPUT) continue;
        IoLive& l = ioLive[i];
        bool r = rawInput(ioCh[i]);
        if (r != l.rawIn) { l.rawIn = r; l.rawSince = now; }
        if (l.on != l.rawIn && now - l.rawSince >= ioCh[i].debounce) { l.on = l.rawIn; l.dirty = true; }
    }
}

// ---- temperatures / analog -----------------------------------------------------------
static void measureAnalog() {
    for (uint8_t i = 0; i < ioCount; i++) {
        const IoChannel& c = ioCh[i];
        if (c.type != IO_ANALOG) continue;
        uint32_t sum = 0;
        for (int k = 0; k < 16; k++) sum += analogReadMilliVolts(c.pin);
        float mv = sum / 16.0f;
        float v = c.val1 + (mv - c.raw1) * (c.val2 - c.val1) / (c.raw2 - c.raw1);
        IoLive& l = ioLive[i];
        l.mv = mv;
        float q = powf(10, c.dec);
        v = roundf(v * q) / q;
        if (isnan(l.v) || v != l.v) { l.v = v; l.dirty = true; }
    }
}

static void readTemps() {
    for (uint8_t i = 0; i < ioCount; i++) {
        const IoChannel& c = ioCh[i];
        if (c.type != IO_TEMP) continue;
        float t = dallas->getTempC(c.rom);
        float v = (t == DEVICE_DISCONNECTED_C || t == 85.0f) ? NAN : roundf((t + c.offset) * 10) / 10;
        IoLive& l = ioLive[i];
        if (isnan(v) != isnan(l.v) || (!isnan(v) && v != l.v)) { l.v = v; l.dirty = true; }
    }
}

static void doScan() {
    scanRomCount = 0;
    if (ow) {
        uint8_t a[8];
        ow->reset_search();
        while (scanRomCount < IO_MAX_CH && ow->search(a)) {
            if (OneWire::crc8(a, 7) != a[7]) continue;
            memcpy(scanRom[scanRomCount], a, 8);
            scanTemp[scanRomCount] = dallas ? dallas->getTempC(a) : NAN;
            scanRomCount++;
        }
    }
    scanI2cCount = 0;
    if (ioHw.sda >= 0 && ioHw.scl >= 0) {
        for (uint8_t a = 0x08; a <= 0x77 && scanI2cCount < sizeof(scanI2c); a++) {
            Wire.beginTransmission(a);
            if (Wire.endTransmission() == 0) scanI2c[scanI2cCount++] = a;
        }
    }
    scanDone = true;
}

void ioRequestScan() { scanRequested = true; }

void ioScanJson(JsonObject o) {
    o["done"] = scanDone;
    JsonArray r = o["roms"].to<JsonArray>();
    for (uint8_t i = 0; i < scanRomCount; i++) {
        JsonObject x = r.add<JsonObject>();
        x["rom"] = romHex(scanRom[i]);
        if (!isnan(scanTemp[i]) && scanTemp[i] != DEVICE_DISCONNECTED_C) x["t"] = roundf(scanTemp[i] * 10) / 10;
    }
    JsonArray b = o["i2c"].to<JsonArray>();
    for (uint8_t i = 0; i < scanI2cCount; i++) b.add(scanI2c[i]);
}

// ---- start ---------------------------------------------------------------------------
void ioInit() {
    for (uint8_t i = 0; i < IO_MAX_CH; i++) { ioLive[i] = IoLive(); ioLive[i].v = NAN; ioLive[i].mv = NAN; }
    bool last[IO_MAX_CH] = {};
    loadStates(last);

    // start levels first, then switch the pins to output: no short click at the start
    auto startOn = [&](uint8_t i) { return ioCh[i].boot == BOOT_ON || (ioCh[i].boot == BOOT_LAST && last[i]); };
    for (uint8_t i = 0; i < ioCount; i++) {
        const IoChannel& c = ioCh[i];
        if (c.type != IO_RELAY) continue;
        bool on = startOn(i);
        bool level = on != c.low;
        ioLive[i].on = on;
        if (c.bus == BUS_SR) { if (level) srBits[c.pin / 8] |= (1 << (c.pin % 8)); }
        else if (c.bus == BUS_EXP) { if (level) expOut[c.chip] |= (1u << c.pin); }
        else { digitalWrite(c.pin, level ? HIGH : LOW); pinMode(c.pin, OUTPUT); digitalWrite(c.pin, level ? HIGH : LOW); }
        ioLive[i].dirty = true;
    }

    // 74HC595: outputs off (OE high) while shifting the start levels in
    if (ioHw.srChips) {
        if (ioHw.srOe >= 0) { digitalWrite(ioHw.srOe, HIGH); pinMode(ioHw.srOe, OUTPUT); digitalWrite(ioHw.srOe, HIGH); }
        pinMode(ioHw.srData, OUTPUT);
        pinMode(ioHw.srClock, OUTPUT);
        pinMode(ioHw.srLatch, OUTPUT);
        srWrite();
        if (ioHw.srOe >= 0) digitalWrite(ioHw.srOe, LOW);
    }

    // I2C expanders
    if (ioHw.sda >= 0 && ioHw.scl >= 0) {
        Wire.begin(ioHw.sda, ioHw.scl, 100000);
        for (uint8_t i = 0; i < ioCount; i++) {
            const IoChannel& c = ioCh[i];
            if (c.type == IO_INPUT && c.bus == BUS_EXP) {
                expInMask[c.chip] |= (1u << c.pin);
                if (c.pullup) expPullup[c.chip] |= (1u << c.pin);
            }
            if (c.type == IO_RELAY && c.bus == BUS_EXP) expOutMask[c.chip] |= (1u << c.pin);
        }
        for (uint8_t c = 0; c < ioHw.expChips; c++) {
            uint8_t a = ioHw.expAddr + c;
            bool ok;
            if (ioHw.expType == 1) {
                ok = expWriteChip(c);
            } else {
                ok = mcpReg(a, 0x14, expOut[c])                          // OLAT first: start levels
                     && mcpReg(a, 0x0C, expPullup[c])                    // GPPU
                     && mcpReg(a, 0x00, (uint16_t)~expOutMask[c]);       // IODIR: 1 = input, only relay pins are outputs
            }
            expOk[c] = ok;
            if (!ok) evlog::add("Expander 0x%02X antwortet nicht", a);
        }
    }

    for (uint8_t i = 0; i < ioCount; i++) {
        const IoChannel& c = ioCh[i];
        if (c.type == IO_INPUT && c.bus == BUS_GPIO) pinMode(c.pin, c.pullup ? INPUT_PULLUP : INPUT);
        if (c.type == IO_ANALOG) pinMode(c.pin, INPUT);
    }
    analogReadResolution(12);

    if (ioHw.ow >= 0) {
        ow = new OneWire(ioHw.ow);
        dallas = new DallasTemperature(ow);
        dallas->begin();
        dallas->setWaitForConversion(false);
        dallas->setResolution(12);
    }

    // inputs: start state without waiting for the debounce
    readExpanders();
    for (uint8_t i = 0; i < ioCount; i++)
        if (ioCh[i].type == IO_INPUT) { ioLive[i].rawIn = ioLive[i].on = rawInput(ioCh[i]); ioLive[i].dirty = true; }

    Serial.printf("[IO] %u channels, 1-Wire %d, 74HC595 %u, expander %u\n", ioCount, ioHw.ow, ioHw.srChips, ioHw.expChips);
}

void ioLoop() {
    uint32_t now = millis();
    runCommands();
    if (now - lastPoll >= 10) { lastPoll = now; pollInputs(); }

    // pulse relays
    for (uint8_t i = 0; i < ioCount; i++)
        if (ioLive[i].offAt && (int32_t)(now - ioLive[i].offAt) >= 0) { ioLive[i].offAt = 0; relaySet(i, false); }

    if (measureFirst || now - lastMeasure >= (uint32_t)ioHw.interval * 1000UL) {
        measureFirst = false;
        lastMeasure = now;
        measureAnalog();
        if (dallas && !converting) { dallas->requestTemperatures(); convStart = now; converting = true; }
    }
    if (converting && now - convStart >= 800) { converting = false; readTemps(); }

    if (scanRequested && !converting) { scanRequested = false; doScan(); }

    if (stateSaveDue && now - stateChanged > 2000) { stateSaveDue = false; saveStates(); }
    if (pendSave) savePending();
}

void ioLiveJson(JsonArray a) {
    for (uint8_t i = 0; i < ioCount; i++) {
        JsonObject o = a.add<JsonObject>();
        o["id"] = ioCh[i].id;
        o["name"] = ioCh[i].name;
        o["type"] = ioTypeName(ioCh[i].type);
        const IoLive& l = ioLive[i];
        if (ioCh[i].type <= IO_INPUT) o["on"] = l.on;
        else if (isnan(l.v)) o["v"] = nullptr;
        else o["v"] = l.v;
        if (ioCh[i].type == IO_ANALOG) { o["unit"] = ioCh[i].unit; o["dec"] = ioCh[i].dec; if (!isnan(l.mv)) o["mv"] = (int)l.mv; }
        if (ioCh[i].type == IO_TEMP) { o["unit"] = "°C"; o["dec"] = 1; }
    }
}

void ioPrint() {
    for (uint8_t i = 0; i < ioCount; i++) {
        const IoLive& l = ioLive[i];
        if (ioCh[i].type <= IO_INPUT) Serial.printf("[IO] %-6s %-20s %s\n", ioTypeName(ioCh[i].type), ioCh[i].id, l.on ? "ON" : "OFF");
        else Serial.printf("[IO] %-6s %-20s %.2f\n", ioTypeName(ioCh[i].type), ioCh[i].id, l.v);
    }
}
