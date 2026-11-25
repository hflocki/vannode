# Gemeinsamer Code

Hier liegt das **Original** von Quelldateien, die mehrere Geräte benutzen. Jeder Sketch-Ordner bekommt eine Kopie, damit er für sich allein in der Arduino IDE kompiliert – Nachbauer brauchen nur den Ordner ihres Geräts.

## Regeln

- Gemeinsame Dateien **nur hier** ändern, nie in einem Sketch-Ordner. Die GitHub Action bricht ab, sobald eine Kopie vom Original abweicht.
- Welche Datei in welchen Sketch gehört, steht in [`manifest.json`](manifest.json).
- Nach jeder Änderung die Kopien aktualisieren:

  ```bash
  python3 -m venv .venv && source .venv/bin/activate
  pip install -r tools/common/requirements.txt
  python3 tools/common/sync_common.py
  ```

  Das Skript listet die betroffenen Geräte. Bei jedem davon `FW_VERSION` erhöhen und taggen.
- Nur prüfen, ohne zu schreiben: `python3 tools/common/sync_common.py --check`.

## Inhalt

| Datei | Zweck | Geräte |
| --- | --- | --- |
| `wifi_manager.h/.cpp` | WLAN mit Access-Point-Fallback, Reset-Grund, Sendeleistung (ESP32 und ESP8266) | template, temp, level, io, alde |
| `fwcheck.h/.cpp` | Firmware-Kennung `CAMPER-FW:<gerät>:<version>:<chip>;` und Prüfung beim Upload (Chip, Gerät, `.merged.bin`, Größe), Fehler im Klartext. Braucht `FW_DEVICE` in `config.h`. Einbinden: `fwcheck::begin(request->contentLength(), request->hasParam("force"))` beim ersten Stück, `fwcheck::chunk()` für jedes, im Request-Handler `fwcheck::ok()` / `fwcheck::error()`; `fwcheck::info()` und `evlog::json()` unter `GET /api/system` für die Webseite (Prüfung im Browser: `fwInspect()` in den Web-Oberflächen) | alle ESP32-Geräte, temp auch ESP8266 (ohne Rückfall) |
| `evlog.h/.cpp` | Ereignis-Log im RTC-Speicher (übersteht Neustart, Absturz und Update), `evlog::begin()` als Erstes in `setup()`, `evlog::add()`, `evlog::watch(wlan, mqtt)` in `loop()` | alle ESP32-Geräte, temp auch ESP8266 (ohne Rückfall) |
| `clockset.h/.cpp` | Uhrzeit ohne Internet: `clockset::begin()` nach `configTzTime()`, `clockset::json()` unter `GET /api/system` als `clock`, `POST /api/time?t=<Unix-Sekunden>[&m=1]` ruft `clockset::set()`. Die Karte *Uhrzeit* in `web/vannode-system.js` erscheint nur, wenn `clock` da ist, und übernimmt ohne gültige Zeit automatisch die des Browsers | display, heater, ble, battery |
| `safeguard.h/.cpp` | Rückfall auf die vorherige Firmware, wenn ein Update kein WLAN bekommt; WLAN-Änderung mit Rückweg (nur ESP32). Einbinden: `guard::begin()` am Anfang von `setup()`, `guard::loop(verbunden)` in `loop()`, `guard::otaDone()` nach erfolgreichem `Update.end()` bzw. in `ArduinoOTA.onEnd` (nur Firmware), `guard::wifiTrialStart(alteSsid, altesPasswort)` vor dem Überschreiben der WLAN-Daten, beim Start `wifiTrialPending()` / `wifiTrialOk()` / `wifiTrialRestore()`, `guard::status()` im Web-UI | alle ESP32-Geräte, temp auch ESP8266 (ohne Rückfall) |
| `mqttfb.h/.cpp` | Ersatz-Broker: ist der Haupt-Broker zu lange nicht erreichbar, verbindet sich das Gerät mit dem Ersatz (meist das Display mit eingeschaltetem Broker) und kehrt zurück, sobald der Haupt-Broker wieder da ist. Einbinden im Reconnect: `mqttfb::begin()`, `host()`/`port()`, `result()`, `backDue()`; `mqttfb::json()` unter `GET /api/system` | alle Geräte |
| `wsauth.h` | Login für den WebSocket per Token (`GET /api/wstoken`, `/ws?t=…`), weil Safari die Anmeldung der Seite nicht mitschickt | template, tanklevel, temp, level, alde, display, ble, battery, io |
| `fw_config.h` | holt `FW_VERSION`, `FW_DEVICE` und `GUARD_*` für die drei Module oben aus `fw_id.h` (falls vorhanden, z. B. Pocket-Node, dessen `config.h` Variablen definiert) oder aus `config.h` | wie oben |
| `supersense_decode.h` | Dekodierung der SuperSense-Empfängerbox (UUIDs, Startbefehl, 4 BCD-Werte für Füllstand und Temperatur, Prozent), Host-Test `firmware/ble/tests/supersense_test.cpp` | tanklevel, ble |
| `ble/` (Ordner) | Gemeinsamer Kern der BLE-Nodes: `src/core/` (`main.cpp` mit `setup()`/`loop()`, WLAN, MQTT/HA, Webserver, Bluetooth-Scan und -Verbindung, Plugin-Basis und Typ-Tabelle `registry.cpp`) und die Web-Oberfläche `web/index.html`. Wird als ganzer Ordner kopiert (`dirs` in `manifest.json`, Aufbau darunter bleibt), danach `tools/web/pack_web.py`. Was ein Node enthält, steht in seiner `config.h` (`PLUGIN_*`, `CT_*`, `NODE_TITLE`, `NODE_DIR`) | ble, battery |
| `devinfo.h/.cpp` | Diagnose-JSON für `<prefix>/info` (Version, IP, MAC, RSSI, Laufzeit, Reset-Grund, freier Speicher) und Zeitsteuerung `devinfoDue()` | alle Geräte |
| `truma_errors.h` | Fehlercodes der Truma-Heizung mit kurzem deutschem Text, je Heizungsfamilie | heater, display |
| `truma_lc_decode.h` | Dekodierung des Truma-LevelControl-Gassensors, Host-Test `firmware/ble/tests/trumalc_test.cpp` | ble |

Beim `wifi_manager` haben tanklevel, maxxfan, heater, display sowie ble und battery (im BLE-Kern) noch eigene, abweichende Fassungen. Sie werden einzeln umgestellt, jeweils mit Test am Gerät.

## Web-Design (`web/camper-theme.css`)

Gemeinsame Farben und Bausteine aller Geräte-Webseiten im Stil des Displays. Wird nicht als Datei kopiert, sondern von `tools/common/sync_common.py` zwischen den Markern `/* camper-theme:begin */` und `/* camper-theme:end */` am Ende des `<style>`-Blocks jeder `web/index.html` eingefügt (Liste unter `blocks` in `manifest.json`). Danach `python3 tools/web/pack_web.py`.
