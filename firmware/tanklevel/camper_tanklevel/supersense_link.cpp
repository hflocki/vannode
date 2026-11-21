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
#define SS_RETRY_MS           10000    // First retry while the sensor is out of range
#define SS_RETRY_MAX_MS       60000    // Back-off limit
#define SS_RECONNECT_MS       2000     // After a regular disconnect
#define SS_RELEASE_MS         (30UL * 60UL * 1000UL)   // App release ends after 30 min

static bool connectBusy = false;   // Only one connection attempt at a time (scan is paused)

class SsLink : public NimBLEClientCallbacks {
public:
    SsLink(const char* tag_, const TankConfig* cfg_, WaterData* out_) : tag(tag_), cfg(cfg_), out(out_) {}

    bool active = false;   // Tank switched on and type SuperSense (set at boot)
    bool released = false;
    unsigned long releasedAt = 0;

    void begin() {
        out->ss_temp = NAN;
        strlcpy(mac, cfg->ss_mac, sizeof(mac));
        setState(mac[0] ? "Warte auf Verbindung" : "Kein Sensor");
        Serial.printf("[SS] %s: sensor %s\n", tag, mac[0] ? mac : "(not set)");
    }

    void loop() {
        if (!active) return;

        // MAC changed in the web UI: drop the old sensor
        if (strcmp(mac, cfg->ss_mac) != 0) {
            disconnect();
            strlcpy(mac, cfg->ss_mac, sizeof(mac));
            out->ss_has_data = false;
            out->ss_hex[0] = '\0';
            retryDelay = 0;
            lastAttempt = 0;
            Serial.printf("[SS] %s: sensor %s\n", tag, mac[0] ? mac : "(not set)");
        }

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
            if (millis() - lastRx > SS_SILENCE_MS) {
                Serial.printf("[SS] %s: nothing received for %lu s - reconnecting\n", tag, SS_SILENCE_MS / 1000);
                disconnect();
                retryDelay = SS_RECONNECT_MS;
                lastAttempt = millis();
                setState("Keine Daten");
            } else {
                setState(out->ss_has_data ? "OK" : "Warte auf Daten");
            }
            if (millis() - lastRssiPoll > 5000 && client && client->isConnected()) {
                lastRssiPoll = millis();
                out->ss_rssi = client->getRssi();
            }
        }
    }

    void release(bool on) {
        if (on == released) return;
        released = on;
        if (on) {
            releasedAt = millis();
            disconnect();
            evlog::add("SuperSense %s: Bluetooth für App freigegeben", tag);
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
    const TankConfig* cfg;
    WaterData* out;
    char mac[18] = "";
    NimBLEClient* client = nullptr;
    volatile int result = 0;    // 0 pending, 1 connected, -1 failed
    volatile int reason = 0;
    volatile bool lost = false;
    bool connecting = false;
    bool up = false;
    uint8_t tryType = BLE_ADDR_PUBLIC;
    unsigned long started = 0, lastAttempt = 0, retryDelay = 0, lastRx = 0, lastRssiPoll = 0;

    // Notification handed over from the host task
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
    uint8_t rx[SS_PAYLOAD_LEN];
    size_t rxLen = 0;
    bool rxPending = false;

    void setState(const char* s) {
        if (strcmp(out->ss_state, s) == 0) return;
        strlcpy(out->ss_state, s, sizeof(out->ss_state));
        Serial.printf("[SS] %s: %s\n", tag, s);
    }

    bool startConnect() {
        if (connectBusy) return true;   // The other tank is connecting: try again shortly
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

    bool finishConnect() {
        NimBLERemoteService* svc = client->getService(SS_SERVICE_UUID);
        NimBLERemoteCharacteristic* notify = svc ? svc->getCharacteristic(SS_NOTIFY_UUID) : nullptr;
        NimBLERemoteCharacteristic* wr = svc ? svc->getCharacteristic(SS_WRITE_UUID) : nullptr;
        if (!notify || !wr) {
            Serial.printf("[SS] %s: service not found - wrong device?\n", tag);
            setState("Falsches Gerät?");
            client->disconnect();
            return false;
        }
        // Init packet first (like the source), then the sensor notifies on its own
        bool wOk = wr->writeValue(SS_INIT_PACKET, sizeof(SS_INIT_PACKET), wr->canWrite());
        bool sOk = notify->subscribe(true, [this](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
            portENTER_CRITICAL(&mux);
            rxLen = n > sizeof(rx) ? sizeof(rx) : n;
            memcpy(rx, d, rxLen);
            rxLen = n;   // Real length, so a wrong packet size is noticed
            rxPending = true;
            portEXIT_CRITICAL(&mux);
        });
        Serial.printf("[SS] %s: init %s, subscribe %s\n", tag, wOk ? "OK" : "FAILED", sOk ? "OK" : "FAILED");
        if (!sOk) {
            setState("Abo fehlgeschlagen");
            client->disconnect();
            return false;
        }
        lost = false;
        up = true;
        lastRx = millis();
        out->ss_rssi = client->getRssi();
        evlog::add("SuperSense %s verbunden", tag);
        return true;
    }

    void failed() {
        retryDelay = retryDelay ? min(retryDelay * 2, (unsigned long)SS_RETRY_MAX_MS) : SS_RETRY_MS;
        lastAttempt = millis();
        if (strcmp(out->ss_state, "Falsches Gerät?") && strcmp(out->ss_state, "Abo fehlgeschlagen"))
            setState("Nicht erreichbar");
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
        uint8_t d[SS_PAYLOAD_LEN];
        size_t n = 0;
        bool have = false;
        portENTER_CRITICAL(&mux);
        if (rxPending) {
            have = true;
            n = rxLen;
            memcpy(d, rx, n > sizeof(d) ? sizeof(d) : n);
            rxPending = false;
        }
        portEXIT_CRITICAL(&mux);
        if (!have) return;
        lastRx = millis();

        size_t h = 0;
        for (size_t i = 0; i < n && i < sizeof(d) && h + 3 < sizeof(out->ss_hex); i++)
            h += snprintf(out->ss_hex + h, sizeof(out->ss_hex) - h, "%02X ", d[i]);
        if (h) out->ss_hex[h - 1] = '\0';

        SuperSenseReading r;
        if (!ssDecode(d, n, r)) {
            Serial.printf("[SS] %s: packet with %u bytes ignored\n", tag, (unsigned)n);
            return;
        }
        if (!r.levelValid) {
            Serial.printf("[SS] %s: invalid level (%s)\n", tag, out->ss_hex);
            return;
        }
        bool first = !out->ss_has_data;
        out->ss_raw = r.level;
        out->ss_temp = r.tempValid ? r.temperature : NAN;
        out->ss_last = millis();
        out->ss_has_data = true;
        out->ss_updates++;
        if (first) Serial.printf("[SS] %s: first reading raw %ld (%s)\n", tag, (long)r.level, out->ss_hex);
    }
};

static SsLink links[2] = {
    SsLink("Frischwasser", &config.fresh, &sensorData.water),
    SsLink("Grauwasser", &config.grey, &sensorData.grey),
};

bool ssUsed() {
    return (config.water_enabled && config.fresh_type == TANK_TYPE_SUPERSENSE) ||
           (config.grey_enabled && config.grey_type == TANK_TYPE_SUPERSENSE);
}

void ssInit() {
    links[0].active = config.water_enabled && config.fresh_type == TANK_TYPE_SUPERSENSE;
    links[1].active = config.grey_enabled && config.grey_type == TANK_TYPE_SUPERSENSE;
    for (SsLink& l : links) if (l.active) l.begin();
}

void ssLoop() {
    for (SsLink& l : links) l.loop();
}

void ssRelease(uint8_t tank, bool on) {
    if (tank < 2) links[tank].release(on);
}

bool ssReleased(uint8_t tank) {
    return tank < 2 && links[tank].released;
}
