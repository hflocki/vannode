// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

// Plugin "fridge": WCS compressor fridge via BLE connection.
//
// Protocol (devices named "WCS..." / "75L..."), plain ASCII, every command ends with "\n":
//   GATT:   service E530, write E531, notify E532 (no Bluetooth pairing)
//   Login:  "/SC0/U"            -> fridge answers with a 32-character id (no "/S" prefix)
//           "/SC0/P<password>"  -> "/SC0/P/001" = ok, "/SC0/P/000" = wrong password
//           The password is the app password (factory default 1234), not a Bluetooth PIN.
//           Without a successful login the fridge sends nothing.
//   Status: "/SC0/1/-01" (poll every 5 s) -> "/SC0/.../f0,f1,...,f18"
//           f0 zones (1/2), f1 power (1/0), f2 unit (1 = C, 2 = F), f3/f4 temperature
//           zone 1/2, f5/f6 set point zone 1/2, f7 battery protection level (1-3),
//           f8 voltage x10, f9 mode ("11" = sleep, 2 digits: "3" first / last = zone 1 / 2
//           in cool mode), f10 battery %, f11 dual zone power state, f12 fan (circulation, off in sleep mode),
//           f13 door heating, f15..f18 max / min set point zone 1, max / min zone 2
//   Error:  "/SCE/<zone>/<code>" or "/SEE/<zone>/<code>", code 1-7 (see errorText())
//   Commands: "/SC1/1/001|000" power on / off, "/SC3/<unit>/+NN" set point zone 1,
//           "/SC5/<unit>/+NN" set point zone 2, "/SC4/1/00<1-3>" battery protection.
// Frames may be split over several notifications: collected from "/S" to "\n".

#include "../core/plugin.h"

#if PLUGIN_WCS_FRIDGE

#include "../core/ble_link.h"
#include "../core/devlog.h"
#include "../core/util.h"

#define WCS_LINE_MAX      200
#define WCS_IDLE_FLUSH    300    // ms without new data: drop / evaluate an unfinished frame
#define WCS_MIN_FIELDS    10     // Minimum number of status fields
#define WCS_ID_LEN        32     // Length of the id answer to "/SC0/U"
#define WCS_STEP_MS       1000   // Login step interval
#define WCS_ID_TRIES      3      // "/SC0/U" attempts before the password is sent anyway
#define WCS_LOGIN_TRIES   3      // Password attempts without answer before polling anyway
#define WCS_LOGIN_WAIT_MS 4000   // Wait for the password answer
#define WCS_DEFAULT_PWD   "1234" // Factory default of the app password (printed in the manual)
#define WCS_SET_MIN       -22    // Set point range if the fridge does not report its own
#define WCS_SET_MAX       10

// GATT layout of the fridge, used when the web UI fields are empty
#define WCS_SERVICE_UUID "0000e530-1212-efde-1523-785feabcd123"
#define WCS_NOTIFY_UUID  "0000e532-1212-efde-1523-785feabcd123"
#define WCS_WRITE_UUID   "0000e531-1212-efde-1523-785feabcd123"

// Options of the set point selects in Home Assistant (WCS_SET_MIN .. WCS_SET_MAX)
#define WCS_SET_OPTIONS "-22|-21|-20|-19|-18|-17|-16|-15|-14|-13|-12|-11|-10|-9|-8|-7|-6|-5|-4|-3|-2|-1|0|1|2|3|4|5|6|7|8|9|10"
#define WCS_BPD_OPTIONS "Stufe 1|Stufe 2|Stufe 3"

struct WcsCfg {
    char mac[18];
    char pin[8];           // App password (digits), empty = factory default 1234
    bool zone2;            // Fridge has a second zone (freezer)
    char service[40];      // GATT service UUID, empty = E530, "*" = all services
    char notify[40];       // Notify characteristic, empty = E532, "*" = every one that can notify
    char write[40];        // Write characteristic, empty = E531
    char poll[24];         // Status command, empty = "/SC0/1/-01"
    int32_t poll_s;        // Poll interval in s
};

static const Field WCS_FIELDS[] = {
    FLD_MAC(WcsCfg, mac, "mac", "Kühlschrank-MAC", "Über \"Suchen\" aus der Liste wählen"),
    FLD_TEXT(WcsCfg, pin, "pin", "App-Passwort", "Das Passwort der WCS-App (Werkseinstellung 1234). "
                                            "Leer = 1234. Keine Bluetooth-Kopplung nötig."),
    FLD_BOOL(WcsCfg, zone2, "zone2", "Zweite Zone", "Kühlschrank mit zweitem Fach / Gefrierfach"),
    FLD_TEXT(WcsCfg, service, "service", "Service-UUID", "Leer = E530 (Standard), * = alle Services"),
    FLD_TEXT(WcsCfg, notify, "notify", "Notify-UUID", "Leer = E532 (Standard), * = jede, die senden kann"),
    FLD_TEXT(WcsCfg, write, "write", "Write-UUID", "Leer = E531 (Standard)"),
    FLD_TEXT(WcsCfg, poll, "poll", "Abfragebefehl", "Leer = /SC0/1/-01 (Standard). \\r und \\n erlaubt"),
    FLD_INT(WcsCfg, poll_s, "poll_s", "Abfrage alle", "s", 2, 3600, "Standard 5 s"),
};

static const Entity WCS_ENTITIES[] = {
    {E_SENSOR, "temp1",     "Ist-Temperatur Zone 1",  "°C", "temperature",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "temp2",     "Ist-Temperatur Zone 2",  "°C", "temperature",     "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "set1",      "Soll-Temperatur Zone 1", "°C", "temperature",     nullptr,       "mdi:thermometer-check", 0, nullptr},
    {E_SENSOR, "set2",      "Soll-Temperatur Zone 2", "°C", "temperature",     nullptr,       "mdi:thermometer-check", 0, nullptr},
    {E_SENSOR, "voltage",   "Spannung",               "V",  "voltage",         "measurement", nullptr, 0, nullptr},
    {E_SENSOR, "battery",   "Batterie",               "%",  "battery",         "measurement", nullptr, 0, nullptr},
    {E_BINARY, "fan",       "Umluft",                 nullptr, nullptr,        nullptr,       "mdi:fan", 0, nullptr},
    {E_BINARY, "night",     "Nachtmodus",             nullptr, nullptr,        nullptr,       "mdi:weather-night", 0, nullptr},
    {E_SENSOR, "error",     "Fehler",                 nullptr, nullptr,        nullptr,       "mdi:alert-circle", EF_ALWAYS, nullptr},
    {E_SWITCH, "power",     "Kühlschrank an",         nullptr, nullptr,        nullptr,       "mdi:fridge", 0, nullptr},
    {E_SELECT, "target1",   "Soll Zone 1 einstellen", nullptr, nullptr,        nullptr,       "mdi:thermometer-lines", 0, WCS_SET_OPTIONS},
    {E_SELECT, "target2",   "Soll Zone 2 einstellen", nullptr, nullptr,        nullptr,       "mdi:thermometer-lines", 0, WCS_SET_OPTIONS},
    {E_SELECT, "bpd",       "Batterieschutz",         nullptr, nullptr,        nullptr,       "mdi:battery-lock", EF_CONFIG, WCS_BPD_OPTIONS},
    {E_BINARY, "connected", "Verbindung",             nullptr, "connectivity", nullptr,       nullptr, EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "state",     "Status",                 nullptr, nullptr,        nullptr,       "mdi:bluetooth-connect", EF_DIAG | EF_ALWAYS, nullptr},
    {E_SENSOR, "ble_rssi",  "Bluetooth Signal",       "dBm", "signal_strength","measurement", "mdi:bluetooth", EF_DIAG | EF_HIDDEN, nullptr},
    {E_SENSOR, "message",   "Letzte Nachricht",       nullptr, nullptr,        nullptr,       "mdi:message-text", EF_DIAG | EF_HIDDEN | EF_ALWAYS, nullptr},
    {E_SWITCH, "release",   "Bluetooth für App freigeben", nullptr, nullptr,   nullptr,       "mdi:bluetooth-off", EF_CONFIG | EF_ALWAYS, nullptr},
};

// Login state after each (re)connect
enum WcsLogin : uint8_t {
    WL_IDLE,       // Not connected
    WL_ID,         // Asking for the id ("/SC0/U")
    WL_PASSWORD,   // Password sent, waiting for "/SC0/P/..."
    WL_OK,         // Logged in (or no answer: polling anyway)
    WL_FAILED,     // Wrong password: stays disconnected until the settings change
};

class WcsFridgePlugin : public Plugin {
public:
    explicit WcsFridgePlugin(uint8_t index) : Plugin("fridge", "WCS Kühlschrank", "Kühlschrank", index) {}

    size_t fields(const Field*& out) override {
        out = WCS_FIELDS;
        return sizeof(WCS_FIELDS) / sizeof(WCS_FIELDS[0]);
    }
    void* cfg() override { return &c; }
    void setDefaults() override {
        memset(&c, 0, sizeof(c));
        c.zone2 = true;
        c.poll_s = 5;
    }
    void onConfigChanged() override {
        if (strcmp(c.mac, lastMac) != 0) {
            strlcpy(lastMac, c.mac, sizeof(lastMac));
            hasData = false;
            updates++;
        }
        unescape(c.poll[0] ? c.poll : "/SC0/1/-01\\n", pollCmd, sizeof(pollCmd));
        // Every command ends with "\n": add it if the field has none
        size_t n = strlen(pollCmd);
        if (n && pollCmd[n - 1] != '\n' && n < sizeof(pollCmd) - 1) { pollCmd[n] = '\n'; pollCmd[n + 1] = '\0'; }
        if (login == WL_FAILED) login = WL_IDLE;   // New password: try again
        if (begun) applyTarget();
    }

    uint8_t bleUse() const override { return BLE_LINK; }
    const char* bleAddress() const override { return c.mac; }
    bool bleCandidate(int32_t company, const char* uuid, const char* name) const override {
        return containsIgnoreCase(name, "WCS") || !strncmp(name, "75L", 3);
    }
    const char* manufacturer() const override { return "WCS"; }

    void begin() override {
        link.begin(id, effUuid(c.service, WCS_SERVICE_UUID), effUuid(c.notify, WCS_NOTIFY_UUID),
                   effUuid(c.write, WCS_WRITE_UUID));
        link.setSilenceTimeout(WCS_OFFLINE_MS);   // Connected but silent: reconnect
        begun = true;
        applyTarget();
    }

    void loop() override {
        link.loop();
        if (link.changes != lastLinkChanges) {
            lastLinkChanges = link.changes;
            lineLen = 0;
            if (login != WL_FAILED) login = WL_IDLE;
            // An error is reported again after the next connect if it is still active
            errorCode = 0;
            errorLine[0] = '\0';
            updates++;
        }

        BlePacket pkt;
        while (link.receive(pkt)) feed(pkt.data, pkt.len);
        if (lineLen && millis() - lastByte > WCS_IDLE_FLUSH) idleFlush();

        if (link.canWrite()) loginStep();

        if (login == WL_OK && link.canWrite() && (lastPoll == 0 || millis() - lastPoll > (uint32_t)c.poll_s * 1000UL)) {
            lastPoll = millis();
            if (!link.write(pollCmd, true)) devlogf(id, "Status request failed");
        }

        bool avail = available();
        if (avail != lastAvailable) {
            lastAvailable = avail;
            updates++;
        }
    }

    bool available() override {
        return link.connected() && hasData && millis() - lastMsg <= WCS_OFFLINE_MS;
    }

    void live(JsonObject o) override {
        o["state"] = stateText();
        o["ok"] = available();
        JsonObject g = o["gauge"].to<JsonObject>();
        if (hasData) g["v"] = roundf(temp1 * 10.0f) / 10.0f;
        else g["v"] = nullptr;
        g["unit"] = "°C";
        g["label"] = "Zone 1";
        g["min"] = -25;
        g["max"] = 15;
        liveRowNum(o, "Soll Zone 1", hasData ? set1 : NAN, 0, "°C");
        if (c.zone2) {
            liveRowNum(o, "Ist Zone 2", hasData ? temp2 : NAN, 0, "°C");
            liveRowNum(o, "Soll Zone 2", hasData ? set2 : NAN, 0, "°C");
        }
        liveRowNum(o, "Spannung", hasData ? voltage : NAN, 1, "V");
        liveRowNum(o, "Batterie", hasData ? battery : NAN, 0, "%");
        if (hasData) liveRowText(o, "Modus", modeText());
        if (hasData && fanKnown) liveRowText(o, "Umluft", fanOn ? "an" : "aus");
        if (errorCode) liveRowText(o, "Fehler", errorLine);
        liveRowNum(o, "Bluetooth", link.connected() ? (float)link.rssi() : NAN, 0, "dBm");
        liveRowNum(o, "Nachrichten", (float)messages, 0, "");
        if (link.lastError()) liveRowText(o, "Letzter BT-Fehler", link.lastErrorText());
        if (hasData && login == WL_OK) {
            liveCtrl(o, "switch", "power", "Kühlschrank an")["v"] = powerOn;
            JsonObject s = liveCtrl(o, "slider", "target1", "Soll Zone 1");
            s["min"] = min1; s["max"] = max1; s["v"] = isnan(set1) ? 0 : (int)lroundf(set1);
            if (c.zone2) {
                s = liveCtrl(o, "slider", "target2", "Soll Zone 2");
                s["min"] = min2; s["max"] = max2; s["v"] = isnan(set2) ? 0 : (int)lroundf(set2);
            }
            s = liveCtrl(o, "select", "bpd", "Batterieschutz");
            s["o"] = WCS_BPD_OPTIONS;
            s["v"] = bpd >= 1 && bpd <= 3 ? bpd - 1 : 0;
        }
        liveCtrl(o, "switch", "release", "Bluetooth für App freigeben")["v"] = link.released();
        o["raw"] = (const char*)lastLine;
    }

    size_t entities(const Entity*& out) override {
        out = WCS_ENTITIES;
        return sizeof(WCS_ENTITIES) / sizeof(WCS_ENTITIES[0]);
    }
    bool entityActive(const Entity& e) override {
        if (!strcmp(e.key, "temp2") || !strcmp(e.key, "set2") || !strcmp(e.key, "target2")) return c.zone2;
        return true;
    }
    uint32_t publishIntervalMs() const override { return MQTT_WCS_MS; }

    void publish(MqttOut& out) override {
        out.text("state", stateText());
        out.onoff("connected", link.connected());
        out.onoff("release", link.released());
        out.text("message", lastLine);
        out.text("error", errorCode ? errorLine : "Kein Fehler");
        if (link.connected()) out.integer("ble_rssi", link.rssi());
        if (!hasData) return;
        out.num("temp1", temp1, 0);
        out.num("set1", set1, 0);
        if (!isnan(set1)) out.integer("target1", lroundf(set1));
        if (fanKnown) out.onoff("fan", fanOn);
        out.onoff("night", !strcmp(mode, "11"));
        if (c.zone2) {
            out.num("temp2", temp2, 0);
            out.num("set2", set2, 0);
            if (!isnan(set2)) out.integer("target2", lroundf(set2));
        }
        out.num("voltage", voltage, 1);
        out.num("battery", battery, 0);
        out.onoff("power", powerOn);
        if (bpd >= 1 && bpd <= 3) {
            char b[10];
            snprintf(b, sizeof(b), "Stufe %d", bpd);
            out.text("bpd", b);
        }
    }

    void command(const char* key, const char* value) override {
        if (!strcmp(key, "release")) {
            link.setReleased(payloadOn(value));
            updates++;
            return;
        }
        // Device commands only once logged in and the current state is known
        if (login != WL_OK || !hasData || !link.canWrite()) {
            devlogf(id, "Command %s ignored: not ready", key);
            return;
        }
        char cmd[24];
        if (!strcmp(key, "power")) {
            snprintf(cmd, sizeof(cmd), "/SC1/1/%s\n", payloadOn(value) ? "001" : "000");
        } else if (!strcmp(key, "target1") || !strcmp(key, "target2")) {
            bool z2 = key[6] == '2';
            int t = atoi(value);   // Degrees Celsius from the web UI / Home Assistant
            int lo = z2 ? min2 : min1, hi = z2 ? max2 : max1;
            if (t < lo) t = lo;
            if (t > hi) t = hi;
            int dev = unit == 2 ? (int)lroundf(t * 9.0f / 5.0f + 32.0f) : t;   // Fridge in F
            snprintf(cmd, sizeof(cmd), "/SC%c/%d/%c%02d\n", z2 ? '5' : '3', unit == 2 ? 2 : 1,
                     dev >= 0 ? '+' : '-', abs(dev));
        } else if (!strcmp(key, "bpd")) {
            int l = 0;
            if (!strncmp(value, "Stufe ", 6)) l = atoi(value + 6);
            else if (value[0] >= '0' && value[0] <= '2' && !value[1]) l = value[0] - '0' + 1;   // Select index
            if (l < 1 || l > 3) return;
            snprintf(cmd, sizeof(cmd), "/SC4/1/00%d\n", l);
        } else {
            return;
        }
        logCmd("Command", cmd);
        if (!link.write(cmd, true)) devlogf(id, "Command failed");
        lastPoll = millis() - (uint32_t)c.poll_s * 1000UL + 500;   // Fresh status shortly after
    }

private:
    // Effective UUID: empty field = default, "*" = no filter (all services / every
    // notifying characteristic, for diagnosis), otherwise the value as entered
    static const char* effUuid(const char* field, const char* def) {
        if (!field[0]) return def;
        if (!strcmp(field, "*")) return "";
        return field;
    }

    WcsCfg c;
    BleLink link;
    bool begun = false;
    char lastMac[18] = "";
    char pollCmd[28] = "";
    uint32_t lastLinkChanges = 0;
    unsigned long lastPoll = 0;
    bool lastAvailable = false;

    WcsLogin login = WL_IDLE;
    uint8_t loginTries = 0;
    unsigned long loginAt = 0;
    bool gotId = false;

    char line[WCS_LINE_MAX + 1];
    size_t lineLen = 0;
    unsigned long lastByte = 0;
    char lastLine[WCS_LINE_MAX + 1] = "";

    bool hasData = false;
    float temp1 = NAN, temp2 = NAN, set1 = NAN, set2 = NAN, voltage = NAN, battery = NAN;
    bool powerOn = false;
    int unit = 1;                 // 1 = Celsius, 2 = Fahrenheit (as reported by the fridge)
    int bpd = 0;                  // Battery protection level 1-3
    char mode[4] = "";
    bool fanOn = false;           // Circulation fan (status field 12); off in sleep mode
    bool fanKnown = false;
    int min1 = WCS_SET_MIN, max1 = WCS_SET_MAX, min2 = WCS_SET_MIN, max2 = WCS_SET_MAX;
    int errorCode = 0;
    char errorLine[48] = "";
    unsigned long lastMsg = 0;
    uint32_t messages = 0;

    void applyTarget() {
        link.setUuids(effUuid(c.service, WCS_SERVICE_UUID), effUuid(c.notify, WCS_NOTIFY_UUID),
                      effUuid(c.write, WCS_WRITE_UUID));
        // No Bluetooth pairing. After a wrong password: stay away until the settings change
        link.setTarget(login == WL_FAILED ? "" : c.mac, BLE_NO_PIN);
    }

    const char* password() const { return c.pin[0] ? c.pin : WCS_DEFAULT_PWD; }

    // Log a command without the password
    void logCmd(const char* what, const char* cmd) {
        char b[32];
        printableCopy((const uint8_t*)cmd, strlen(cmd), b, sizeof(b));
        devlogf(id, "%s: %s", what, b);
    }

    // Login sequence: ask for the id each second (max 3 times), then send the
    // password; the fridge only sends status after a successful login
    void loginStep() {
        unsigned long now = millis();
        switch (login) {
        case WL_IDLE:
            login = WL_ID;
            loginTries = 0;
            gotId = false;
            loginAt = 0;
            lastPoll = 0;
            // fall through
        case WL_ID:
            if (gotId || loginTries >= WCS_ID_TRIES) {
                if (!gotId) devlogf(id, "No id answer to /SC0/U, sending the password anyway");
                sendPassword();
                return;
            }
            if (loginAt == 0 || now - loginAt >= WCS_STEP_MS) {
                loginAt = now;
                loginTries++;
                logCmd("Login step 1", "/SC0/U\n");
                link.write("/SC0/U\n", true);
            }
            break;
        case WL_PASSWORD:
            if (now - loginAt >= WCS_LOGIN_WAIT_MS) {
                if (loginTries < WCS_LOGIN_TRIES) {
                    sendPassword();
                } else {
                    devlogf(id, "No answer to the password, polling status anyway");
                    login = WL_OK;
                    updates++;
                }
            }
            break;
        default:
            break;
        }
    }

    void sendPassword() {
        if (login != WL_PASSWORD) loginTries = 0;
        login = WL_PASSWORD;
        loginTries++;
        loginAt = millis();
        char cmd[24];
        snprintf(cmd, sizeof(cmd), "/SC0/P%s\n", password());
        devlogf(id, "Login step 2: password (%s, attempt %u)", c.pin[0] ? "from settings" : "default", loginTries);
        link.write(cmd, true);
    }

    const char* stateText() {
        if (login == WL_FAILED) return "Passwort falsch";
        if (link.connected() && login == WL_ID) return "Anmelden";
        if (link.connected() && login == WL_PASSWORD) return "Passwort prüfen";
        if (link.connected() && !hasData) return "Warte auf Daten";
        if (link.connected() && millis() - lastMsg > WCS_OFFLINE_MS) return "Keine Daten";
        return link.state();
    }

    const char* modeText() const {
        if (!strcmp(mode, "11")) return "Nachtmodus";
        if (strlen(mode) == 2) {
            if (mode[0] == '3' && mode[1] == '3') return "Schnellkühlen Zone 1 + 2";
            if (mode[0] == '3') return "Schnellkühlen Zone 1";
            if (mode[1] == '3') return "Schnellkühlen Zone 2";
        }
        return mode[0] ? mode : "-";
    }

    static const char* errorText(int code) {
        switch (code) {
        case 1: return "Unterspannung";
        case 2: return "Lüfter";
        case 3: return "Anlauf Kompressor";
        case 4: return "Kompressor";
        case 5: return "Kompressorsteuerung";
        case 6: return "Temperaturfühler";
        case 7: return "Tür offen";
        default: return "Unbekannt";
        }
    }

    // "\r" and "\n" in the web UI field become real control characters
    static void unescape(const char* in, char* out, size_t size) {
        size_t o = 0;
        for (size_t i = 0; in[i] && o < size - 1; i++) {
            if (in[i] == '\\' && in[i + 1] == 'r') { out[o++] = '\r'; i++; }
            else if (in[i] == '\\' && in[i + 1] == 'n') { out[o++] = '\n'; i++; }
            else out[o++] = in[i];
        }
        out[o] = '\0';
    }

    // Collect bytes; a frame runs from "/S" to "\n". Text before "/S" is dropped, except
    // the 32-character id the fridge sends without prefix.
    void feed(const uint8_t* d, size_t n) {
        lastByte = millis();
        for (size_t i = 0; i < n; i++) {
            char ch = (char)d[i];
            if (ch == '\0') continue;
            if (ch == '\n') {
                line[lineLen] = '\0';
                handleChunk();
                lineLen = 0;
                continue;
            }
            if (ch == '\r') continue;
            // A new frame starts in the middle of the buffer: finish the text before it
            if (ch == 'S' && lineLen && line[lineLen - 1] == '/' && lineLen > 1) {
                line[lineLen - 1] = '\0';
                handleChunk();
                line[0] = '/';
                lineLen = 1;
            }
            if (lineLen < WCS_LINE_MAX) line[lineLen++] = ch;
        }
        // The id has no terminator: exactly 32 characters, no "/S"
        if (lineLen == WCS_ID_LEN && line[0] != '/') {
            line[lineLen] = '\0';
            handleChunk();
            lineLen = 0;
        }
    }

    void idleFlush() {
        line[lineLen] = '\0';
        handleChunk();
        lineLen = 0;
    }

    void handleChunk() {
        if (!line[0]) return;
        printableCopy((const uint8_t*)line, strlen(line), lastLine, sizeof(lastLine));
        updates++;
        const char* f = strstr(line, "/S");
        if (!f) {
            if (strlen(line) == WCS_ID_LEN) {
                gotId = true;
                devlogf(id, "Id received (%u characters)", (unsigned)WCS_ID_LEN);
            } else {
                devlogf(id, "Text without frame: %s", lastLine);
            }
            return;
        }
        parseFrame(f);
    }

    static bool toInt(const char* s, int& out) {
        char* end;
        long v = strtol(s, &end, 10);
        if (end == s) return false;
        out = (int)v;
        return true;
    }

    float toCelsius(int v) const { return unit == 2 ? (v - 32) * 5.0f / 9.0f : (float)v; }

    void parseFrame(const char* f) {
        // Error frame "/SCE/r/e" or "/SEE/r/e"
        if (!strncmp(f, "/SCE/", 5) || !strncmp(f, "/SEE/", 5)) {
            if (strlen(f) >= 8 && f[6] == '/') {
                int zone = f[5] - '0', code = f[7] - '0';
                errorCode = code;
                snprintf(errorLine, sizeof(errorLine), "E%d %s (Zone %d)", code, errorText(code), zone);
                devlogf(id, "Error: %s", errorLine);
            } else {
                devlogf(id, "Error frame not understood: %s", lastLine);
            }
            return;
        }
        if (strncmp(f, "/SC0/", 5) != 0) {
            devlogf(id, "Frame: %s", lastLine);   // Answers to commands, e.g. "/SC1/1/001"
            return;
        }
        // Password answer
        if (!strncmp(f, "/SC0/P/", 7)) {
            if (!strncmp(f + 7, "001", 3)) {
                devlogf(id, "Password ok");
                login = WL_OK;
                lastPoll = 0;
            } else if (!strncmp(f + 7, "000", 3)) {
                devlogf(id, "Password wrong: set the app password in the settings");
                login = WL_FAILED;
                link.disconnect();
                applyTarget();
            }
            updates++;
            return;
        }

        // Status: fields after the last '/'
        const char* p = strrchr(f, '/');
        char buf[WCS_LINE_MAX + 1];
        strlcpy(buf, p + 1, sizeof(buf));
        // Split by hand: strtok() would skip empty fields and shift the positions
        const char* tok[20];
        int n = 0;
        char* s = buf;
        while (n < 20) {
            tok[n++] = s;
            char* comma = strchr(s, ',');
            if (!comma) break;
            *comma = '\0';
            s = comma + 1;
        }
        if (n < WCS_MIN_FIELDS) {
            devlogf(id, "Frame with %d fields: %s", n, lastLine);
            return;
        }
        if (login != WL_OK) { login = WL_OK; updates++; }   // Status arrives: logged in

        int v;
        if (toInt(tok[0], v) && !c.zone2 && v == 2) devlogf(id, "Fridge reports 2 zones: enable \"Zweite Zone\"");
        if (toInt(tok[1], v)) powerOn = v == 1;
        if (toInt(tok[2], v)) unit = v == 2 ? 2 : 1;
        temp1 = toInt(tok[3], v) ? toCelsius(v) : NAN;
        temp2 = toInt(tok[4], v) ? toCelsius(v) : NAN;
        if (toInt(tok[5], v)) set1 = toCelsius(v);
        if (toInt(tok[6], v)) set2 = toCelsius(v);
        if (toInt(tok[7], v)) bpd = v;
        if (toInt(tok[8], v)) voltage = v / 10.0f;
        strlcpy(mode, tok[9], sizeof(mode));
        if (n > 12 && toInt(tok[12], v)) { fanOn = v == 1; fanKnown = true; }
        if (n > 10 && toInt(tok[10], v)) battery = v < 0 ? 0 : (v > 100 ? 100 : v);
        if (n > 18) {
            int a, b, cc, d;
            if (toInt(tok[15], a) && toInt(tok[16], b) && toInt(tok[17], cc) && toInt(tok[18], d)) {
                max1 = (int)lroundf(toCelsius(a)); min1 = (int)lroundf(toCelsius(b));
                max2 = (int)lroundf(toCelsius(cc)); min2 = (int)lroundf(toCelsius(d));
            }
        }
        hasData = true;
        lastMsg = millis();
        messages++;
        // Full status only now and then, it repeats every few seconds
        if (messages <= 3 || messages % 60 == 0) {
            devlogf(id, "Status: %s | zone 1 %.0f (set %.0f) | zone 2 %.0f (set %.0f) | %.1f V | %.0f%% | protection %d | mode %s",
                    powerOn ? "on" : "off", temp1, set1, temp2, set2, voltage, battery, bpd, mode);
        }
    }
};

Plugin* createWcsFridge(uint8_t index) {
    return new WcsFridgePlugin(index);
}

#endif // PLUGIN_WCS_FRIDGE
