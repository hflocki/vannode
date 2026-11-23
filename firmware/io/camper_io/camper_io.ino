// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 hflocki - part of VanNode, https://github.com/hflocki/vannode
// GNU GPL v3 with additional terms (section 7); third-party parts keep their own license.
// See LICENSE and NOTICE.md. NO WARRANTY - use at your own risk (disclaimer in NOTICE.md).

/*
 * IO Node - relays, inputs, DS18B20 and analog values for the camper
 * =======================================================
 * Hardware: ESP32-C3 Super Mini (also ESP32-S3), Arduino-ESP32 3.3.x
 *
 *   - Channels set up in the web UI (io.h): relays on GPIOs, a 74HC595 chain or I2C expanders
 *     (MCP23017 / PCF8574), switching at LOW or HIGH; inputs with invert, pull-up and debounce;
 *     DS18B20 on one 1-Wire pin; analog inputs with two-point conversion
 *   - Web UI from LittleFS (data/index.html.gz), MQTT with Home Assistant discovery
 *   - Firmware (.bin) and web page (.gz) upload in the web UI, Arduino OTA
 */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

#include "config.h"
#include "wifi_manager.h"
#include "safeguard.h"
#include "evlog.h"
#include "io.h"
#include "mqtt_ha.h"
#include "webserver.h"

AppConfig config;
SensorData sensorData;

static unsigned long lastStatusPrint = 0;

static void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(DEFAULT_OTA_PASSWORD);
    ArduinoOTA.onStart([]() {
        // Filesystem upload over the network replaces config.json too; unmount first
        if (ArduinoOTA.getCommand() != U_FLASH) LittleFS.end();
        Serial.printf("[OTA] Start updating %s\n", ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem");
    });
    ArduinoOTA.onEnd([]() {
        evlog::add("Update über den IDE-Netzwerkport ok, Neustart");
        if (ArduinoOTA.getCommand() == U_FLASH) guard::otaDone();
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", total ? progress * 100U / total : 0);
    });
    ArduinoOTA.onError([](ota_error_t error) { evlog::add("Update über den IDE-Netzwerkport fehlgeschlagen (Fehler %u)", (unsigned)error); });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

// WiFi start. A changed WiFi that does not connect is replaced by the previous, working
// one, followed by a restart so the wifi manager starts clean with it (safeguard.h).
static void startWiFi() {
    wifiManagerInit();
    if (!guard::wifiTrialPending()) return;
    if (isConnected()) { guard::wifiTrialOk(); return; }
    String ssid, pass, extra;
    if (!guard::wifiTrialRestore(ssid, pass, extra)) return;
    strlcpy(config.wifi_ssid, ssid.c_str(), sizeof(config.wifi_ssid));
    strlcpy(config.wifi_pass, pass.c_str(), sizeof(config.wifi_pass));
    saveConfig();
    Serial.println("[MAIN] New WiFi failed - previous WiFi restored, restarting");
    delay(200);
    guard::plannedRestart();   // intended, not a crash (update probation)
    ESP.restart();
}

void setup() {
    Serial.begin(115200);
    evlog::begin();
    evlog::add("Start v%s, Grund: %s", FW_VERSION, evlog::resetReason());
    guard::begin();   // may boot the previous firmware (failed update)
    if (guard::status().length()) evlog::add("%s", guard::status().c_str());
    Serial.setTimeout(50);  // Do not block the loop while waiting for a line end
    delay(1000);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  " DEVICE_NAME " v" FW_VERSION);
    Serial.println("========================================");

    memset(&sensorData, 0, sizeof(SensorData));

    webserverInit();      // 1. LittleFS + config + channels
    ioInit();             // 2. relays to their start state first, then the rest
    startWiFi();    // 3. WiFi (or setup AP)
    webserverStart();     // 4. web UI
    setupOTA();           // 5. Arduino OTA
    mqttInit();           // 6. MQTT

    Serial.printf("[MAIN] Setup done! Heap: %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
    wifiManagerLoop();
    guard::loop(isConnected() || config.wifi_ssid[0] == '\0');
    evlog::watch(isConnected(), mqttIsConnected());
    ArduinoOTA.handle();
    ioLoop();
    webserverLoop();
    mqttLoop();

    // Serial commands (115200 baud, line end "newline")
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "read") {
            ioPrint();
        } else if (cmd == "noauth") {
            // Forgotten web login: back to the factory login over USB
            strlcpy(config.web_user, DEFAULT_WEB_USER, sizeof(config.web_user));
            strlcpy(config.web_pass, DEFAULT_WEB_PASSWORD, sizeof(config.web_pass));
            saveConfig();
            Serial.println("[CMD] Web login reset to " DEFAULT_WEB_USER " / " DEFAULT_WEB_PASSWORD);
        } else if (cmd == "help") {
            Serial.println("[CMD] Commands: read, noauth, help, <id> on|off|toggle");
        } else {
            int sp = cmd.indexOf(' ');
            if (sp > 0 && ioCommand(cmd.substring(0, sp).c_str(), cmd.substring(sp + 1).c_str()))
                Serial.printf("[CMD] %s\n", cmd.c_str());
        }
    }

    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();
        Serial.printf("[STATUS] Channels:%u WiFi:%s MQTT:%s Heap:%u\n", (unsigned)ioCount,
            getWiFiStateString().c_str(), mqttIsConnected() ? "OK" : "---", (unsigned)ESP.getFreeHeap());
    }

    delay(5);
}
