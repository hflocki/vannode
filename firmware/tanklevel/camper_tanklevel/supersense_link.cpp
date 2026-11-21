// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

#include "supersense_link.h"
#include "supersense_decode.h"
#include "gas_sensor.h"
#include "evlog.h"
#include <NimBLEDevice.h>

#define SS_CONNECT_TIMEOUT_MS 4000
#define SS_RETRY_MS           10000    // First retry while the box is out of range
#define SS_RETRY_MAX_MS       60000    // Back-off limit
#define SS_RECONNECT_MS       2000     // After a regular disconnect
#define SS_RELEASE_MS         (30UL * 60UL * 1000UL)   // App release ends after 30 min

static bool connectBusy = false;   // Only one connection attempt at a time (scan is paused)

// One connection to a SuperSense receiver box. Both tanks share it if they use the same box.
class SsLink : public NimBLEClientCallbacks {
public:
    explicit SsLink(const char* tag_) : tag(tag_) {}

    char mac[18] = "";            // target, set by ssLoop() from the tank settings
    bool released = false;
    unsigned long releasedAt = 0;

    // Values of the box (loop task)
    bool hasLevels = false, hasTemps = false;
    int32_t level[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};
    int32_t temp[SS_CHANNELS] = {SS_NONE, SS_NONE, SS_NONE, SS_NONE};
    unsigned long last = 0;       // millis() of the last valid level packet
    uint32_t updates = 0;
    int rssi = 0;
    char state[24] = "Kein Sensor";
    char hex[26] = "";

    void setTarget(const char* m) {
        if (strcmp(mac, m) == 0) return;
        disconnect();
        strlcpy(mac, m, sizeof(mac));
        hasLevels = hasTemps = false;
        hex[0] = '\0';
        retryDelay = 0;
        lastAttempt = 0;
        if (mac[0]) Serial.printf("[SS] %s: box %s\n", tag, mac);
    }

    void loop() {
        takeData();

        if (lost) {
            lost = false;
            if (up) {
                Serial.printf("[SS] %s: disconnected (0x%03X)\n", tag, reason);
                retryDelay = SS_RECONNECT_MS;
                lastAttempt = millis();
            }
            up = false;
        }

        if (released && millis() - releasedAt > SS_RELEASE_MS) released = false;

        if (!mac[0]) {
            setState("Kein Sensor");
        } else if (released) {
            setState("Für App freigegeben");
        } else if (connecting) {
            int r = result;
            if (r == 0 && millis() - started > SS_CONNECT_TIMEOUT_MS + 2000) {
                client->cancelConnect();   // Safety net if the stack never reports a result
                r = -1;
            }
            if (r != 0) {
                connecting = false;
                // Discovery and subscription before the scan resumes (both at once fails)
                bool ok = r == 1 && finishConnect();
                gasScanPause(false);
                connectBusy = false;
                if (ok) {
                    retryDelay = 0;
                    setState("Verbunden");
                } else {
                    failed();
                }
            }
        } else if (!up) {
            if (lastAttempt == 0 || millis() - lastAttempt > retryDelay) {
                if (!startConnect()) failed();
            }
        } else {
            // Init packet about 1 s after connecting, then the box notifies on its own
            if (!initSent && millis() - connectedAt >= SS_INIT_DELAY_MS) {
                initSent = true;
                bool ok = wr && wr->writeValue(SS_INIT_PACKET, sizeof(SS_INIT_PACKET), wr->canWrite());
                Serial.printf("[SS] %s: init %s\n", tag, ok ? "OK" : "FAILED");
                lastRx = millis();
            }
            if (millis() - lastRx > SS_SILENCE_MS) {
                Serial.printf("[SS] %s: nothing received for %lu s - reconnecting\n", tag, SS_SILENCE_MS / 1000);
                disconnect();
                retryDelay = SS_RECONNECT_MS;
                lastAttempt = millis();
                setState("Keine Daten");
            } else {
                setState(hasLevels ? "OK" : "Warte auf Daten");
            }
            if (millis() - lastRssiPoll > 5000 && client && client->isConnected()) {
                lastRssiPoll = millis();
                rssi = client->getRssi();
            }
        }
    }

    void release(bool on) {
        if (on == released) return;
        released = on;
        if (on) {
            releasedAt = millis();
            disconnect();
            evlog::add("SuperSense %s: Bluetooth für App freigegeben", mac);
        } else {
            retryDelay = 0;
            lastAttempt = 0;
        }
    }

    // NimBLE callbacks (host task)
    void onConnect(NimBLEClient*) override { result = 1; }
    void onConnectFail(NimBLEClient*, int r) override {
        reason = r;
        result = -1;
    }
    void onDisconnect(NimBLEClient*, int r) override {
        reason = r;
        if (connecting) result = -1;
        else lost = true;
    }

private:
    const char* tag;
    NimBLEClient* client = nullptr;
    NimBLERemoteCharacteristic* wr = nullptr;
    volatile int result = 0;    // 0 pending, 1 connected, -1 failed
    volatile int reason = 0;
    volatile bool lost = false;
    bool connecting = false;
    bool up = false;
    bool initSent = false;
    uint8_t tryType = BLE_ADDR_PUBLIC;
    unsigned long started = 0, lastAttempt = 0, retryDelay = 0, lastRx = 0, lastRssiPoll = 0, connectedAt = 0;

    // Notifications handed over from the host task (latest of each kind)
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t rxLevel[SS_PAYLOAD_LEN], rxTemp[SS_PAYLOAD_LEN];
    size_t rxLevelLen = 0, rxTempLen = 0;
    bool rxLevelNew = false, rxTempNew = false;

    void setState(const char* s) {
        if (strcmp(state, s) == 0) return;
        strlcpy(state, s, sizeof(state));
        Serial.printf("[SS] %s: %s\n", tag, s);
    }

    bool startConnect() {
        if (connectBusy) return true;   // The other link is connecting: try again shortly
        if (!client) {
            client = NimBLEDevice::createClient();
            if (!client) return false;
            client->setClientCallbacks(this, false);
            client->setConnectTimeout(SS_CONNECT_TIMEOUT_MS);
        }
        // Address type from the scan list; otherwise alternate between attempts
        uint8_t type = tryType;
        if (!gasFindAddrType(mac, &type)) tryType = (tryType == BLE_ADDR_PUBLIC) ? BLE_ADDR_RANDOM : BLE_ADDR_PUBLIC;

        connectBusy = true;
        gasScanPause(true);
        setState("Verbinde...");
        result = 0;
        if (!client->connect(NimBLEAddress(std::string(mac), type), true, true, true)) {
            gasScanPause(false);
            connectBusy = false;
            return false;
        }
        connecting = true;
        started = millis();
        return true;
    }

    bool subscribe(NimBLERemoteService* svc, const char* uuid, bool isLevel) {
        NimBLERemoteCharacteristic* ch = svc->getCharacteristic(uuid);
        if (!ch || !ch->canNotify()) return false;
        return ch->subscribe(true, [this, isLevel](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
            portENTER_CRITICAL(&mux);
            uint8_t* buf = isLevel ? rxLevel : rxTemp;
            size_t c = n > SS_PAYLOAD_LEN ? SS_PAYLOAD_LEN : n;
            memcpy(buf, d, c);
            if (isLevel) {
                rxLevelLen = n;   // real length, so a wrong packet size is noticed
                rxLevelNew = true;
            } else {
                rxTempLen = n;
                rxTempNew = true;
            }
            portEXIT_CRITICAL(&mux);
        });
    }

    bool finishConnect() {
        NimBLERemoteService* svc = client->getService(SS_SERVICE_UUID);
        wr = svc ? svc->getCharacteristic(SS_WRITE_UUID) : nullptr;
        bool lvl = svc && subscribe(svc, SS_LEVEL_UUID, true);
        if (!svc || !wr || !lvl) {
            Serial.printf("[SS] %s: service / characteristics not found - wrong device?\n", tag);
            setState("Falsches Gerät?");
            client->disconnect();
            return false;
        }
        bool tmp = subscribe(svc, SS_TEMP_UUID, false);
        Serial.printf("[SS] %s: levels subscribed, temperatures %s\n", tag, tmp ? "subscribed" : "FAILED");
        lost = false;
        up = true;
        initSent = false;
        connectedAt = millis();
        lastRx = millis();
        rssi = client->getRssi();
        evlog::add("SuperSense-Box %s verbunden", mac);
        return true;
    }

    void failed() {
        retryDelay = retryDelay ? min(retryDelay * 2, (unsigned long)SS_RETRY_MAX_MS) : SS_RETRY_MS;
        lastAttempt = millis();
        if (strcmp(state, "Falsches Gerät?") != 0) setState("Nicht erreichbar");
    }

    void disconnect() {
        if (connecting) {
            if (client) client->cancelConnect();
            connecting = false;
            gasScanPause(false);
            connectBusy = false;
        }
        if (client && client->isConnected()) client->disconnect();
        up = false;
    }

    void takeData() {
        uint8_t dl[SS_PAYLOAD_LEN], dt[SS_PAYLOAD_LEN];
        size_t nl = 0, nt = 0;
        bool gotL = false, gotT = false;
        portENTER_CRITICAL(&mux);
        if (rxLevelNew) {
            gotL = true;
            nl = rxLevelLen;
            memcpy(dl, rxLevel, sizeof(dl));
            rxLevelNew = false;
        }
        if (rxTempNew) {
            gotT = true;
            nt = rxTempLen;
            memcpy(dt, rxTemp, sizeof(dt));
            rxTempNew = false;
        }
        portEXIT_CRITICAL(&mux);
        int32_t v[SS_CHANNELS];
        if (gotT) {
            lastRx = millis();
            if (ssDecode(dt, nt, v)) {
                memcpy(temp, v, sizeof(temp));
                hasTemps = true;
            }
        }
        if (!gotL) return;
        lastRx = millis();
        size_t h = 0;
        for (size_t i = 0; i < nl && i < sizeof(dl) && h + 3 < sizeof(hex); i++) h += snprintf(hex + h, sizeof(hex) - h, "%02X ", dl[i]);
        if (h) hex[h - 1] = '\0';
        if (!ssDecode(dl, nl, v)) {
            Serial.printf("[SS] %s: invalid packet (%s)\n", tag, hex);
            return;
        }
        if (!hasLevels) Serial.printf("[SS] %s: first levels %ld / %ld / %ld / %ld\n", tag, (long)v[0], (long)v[1], (long)v[2], (long)v[3]);
        memcpy(level, v, sizeof(level));
        hasLevels = true;
        last = millis();
        updates++;
    }
};

static SsLink links[2] = {SsLink("Box 1"), SsLink("Box 2")};
static bool active[2] = {false, false};   // fresh / grey tank uses SuperSense (fixed at boot)

static TankConfig& tankCfg(uint8_t t) { return t == 0 ? config.fresh : config.grey; }
static WaterData& tankData(uint8_t t) { return t == 0 ? sensorData.water : sensorData.grey; }

static SsLink* linkFor(uint8_t t) {
    const char* m = tankCfg(t).ss_mac;
    if (!active[t] || !m[0]) return nullptr;
    for (SsLink& l : links)
        if (strcmp(l.mac, m) == 0) return &l;
    return nullptr;
}

bool ssUsed() {
    return (config.water_enabled && config.fresh_type == TANK_TYPE_SUPERSENSE) ||
           (config.grey_enabled && config.grey_type == TANK_TYPE_SUPERSENSE);
}

void ssInit() {
    active[0] = config.water_enabled && config.fresh_type == TANK_TYPE_SUPERSENSE;
    active[1] = config.grey_enabled && config.grey_type == TANK_TYPE_SUPERSENSE;
    for (uint8_t t = 0; t < 2; t++) {
        tankData(t).ss_temp = NAN;
        tankData(t).ss_raw = SS_NONE;
        for (int c = 0; c < SS_CHANNELS; c++) tankData(t).ss_box[c] = SS_NONE;
    }
}

void ssLoop() {
    // Boxes to connect: the fresh water box, and the grey water box if it is another one
    const char* m0 = active[0] ? config.fresh.ss_mac : "";
    const char* m1 = active[1] ? config.grey.ss_mac : "";
    if (!m0[0]) {
        m0 = m1;
        m1 = "";
    } else if (strcmp(m0, m1) == 0) {
        m1 = "";
    }
    links[0].setTarget(m0);
    links[1].setTarget(m1);
    for (SsLink& l : links) l.loop();

    // Values of the tank's channel into its WaterData (water_sensor.cpp converts them)
    for (uint8_t t = 0; t < 2; t++) {
        if (!active[t]) continue;
        WaterData& w = tankData(t);
        SsLink* l = linkFor(t);
        if (!l) {
            strlcpy(w.ss_state, "Kein Sensor", sizeof(w.ss_state));
            continue;
        }
        uint8_t ch = tankCfg(t).ss_channel;
        if (ch < 1 || ch > SS_CHANNELS) ch = 1;
        strlcpy(w.ss_state, l->state, sizeof(w.ss_state));
        strlcpy(w.ss_hex, l->hex, sizeof(w.ss_hex));
        w.ss_rssi = l->rssi;
        float tc;
        w.ss_temp = (l->hasTemps && ssTemperature(l->temp[ch - 1], tc)) ? tc : NAN;
        if (!l->hasLevels) continue;
        for (int c = 0; c < SS_CHANNELS; c++) w.ss_box[c] = l->level[c];
        w.ss_raw = l->level[ch - 1];
        if (w.ss_raw == SS_NONE) {
            strlcpy(w.ss_state, "Kanal ohne Sensor", sizeof(w.ss_state));
            continue;
        }
        if (w.ss_updates != l->updates) {
            w.ss_updates = l->updates;
            w.ss_last = l->last;
            w.ss_has_data = true;
        }
    }
}

void ssRelease(uint8_t tank, bool on) {
    if (tank >= 2) return;
    SsLink* l = linkFor(tank);
    if (l) l->release(on);
}

bool ssReleased(uint8_t tank) {
    SsLink* l = tank < 2 ? linkFor(tank) : nullptr;
    return l && l->released;
}
