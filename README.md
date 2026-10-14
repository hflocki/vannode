# truma-lin-mqtt

Truma-Heizung (Combi / Combi D, Vario Heat) per **MQTT** steuern – mit einem ESP32 am LIN-Bus, reines **Arduino**, ohne ESPHome.

- Der ESP ist **LIN-Slave** und meldet sich am CP Plus als **iNet Box** (T23.70.0) an.
- Das **CP Plus bleibt LIN-Master** und voll bedienbar. Änderungen am CP Plus erscheinen in MQTT, MQTT-Befehle erscheinen am CP Plus.
- Topics kompatibel zu [mc0110/inetbox2mqtt](https://github.com/mc0110/inetbox2mqtt) (`service/truma/...`), dazu Home-Assistant-Auto-Discovery.
- Web-Oberfläche für Bedienung, Einstellungen, Log und Firmware-Update.

Protokoll-Logik portiert aus [skrebber/esphome-truma_inetbox](https://github.com/skrebber/esphome-truma_inetbox) (Fork von Fabian-Schmidt), basierend auf der Vorarbeit von [danielfett/inetbox.py](https://github.com/danielfett/inetbox.py) und [WomoLIN](https://github.com/muccc/WomoLIN).

## Voraussetzungen

- CP Plus **ab Version C4.xx** (ältere C3 können nur lesen, nicht schalten)
- **Keine echte iNet Box** am Bus, **kein iNet X** Bedienteil
- ESP32 mit LIN-Transceiver (TJA1020/TJA1021 o. ä.)

## Unterstützte Boards

In `truma_heater/config.h` ganz oben genau ein Board aktivieren:

| Profil | Board | LIN TX / RX | Arduino-Board |
|---|---|---|---|
| `BOARD_SMARTAVAN` (Standard) | smartavan heater controller | 16 / 17 | ESP32 Dev Module |
| `BOARD_ESP32_GENERIC` | ESP32 DevKit + LIN-Modul (Verdrahtung wie inetbox2mqtt, LEDs 4/15) | 17 / 16 | ESP32 Dev Module |
| `BOARD_ESP32S3_GENERIC` | ESP32-S3 + LIN-Modul | 17 / 18 | ESP32S3 Dev Module |
| `BOARD_ESP32C3_GENERIC` | ESP32-C3 + LIN-Modul | 21 / 20 | ESP32C3 Dev Module |

Pins lassen sich im jeweiligen Block in `config.h` anpassen. Optional: `LIN_CS_PIN` (Enable/SLP des Transceivers), `LIN_FAULT_PIN`, `MQTT_LED_PIN`, `LIN_LED_PIN`.

**Verdrahtung, Bus-Anschluss, Pins und Fallstricke: siehe [HARDWARE.md](HARDWARE.md).**

## Arduino IDE einrichten

1. Boardverwalter: **esp32 von Espressif, Version 3.x** (kompiliert mit 3.3).
2. Bibliotheken über den Bibliotheksverwalter installieren:
   - **PubSubClient** (Nick O'Leary)
   - **ArduinoJson** (Benoit Blanchon), Version 7.x
   - **ESP Async WebServer** (ESP32Async)
   - **Async TCP** (ESP32Async)
3. Ordner `truma_heater` öffnen (`truma_heater.ino`).
4. Board wählen (siehe Tabelle), **Partition Scheme: „Minimal SPIFFS (1.9MB APP with OTA)“** – lässt Platz für künftige Updates über die Weboberfläche.
5. Hochladen. Serieller Monitor: 115200 Baud.

Spätere Updates gehen ohne USB: In der Arduino IDE *Sketch → Kompilierte Binärdatei exportieren*, dann die `.bin` in der Weboberfläche unter **System → Firmware-Update** hochladen.

## Inbetriebnahme

1. Nach dem ersten Start öffnet der ESP den Access Point **`truma-heater-XXXX`** (Passwort `truma1234`). Verbinden, die Seite öffnet sich automatisch (sonst `http://192.168.4.1`).
2. Unter **Einstellungen** WLAN (optional feste IP), MQTT-Broker und ggf. Web-Login eintragen → Speichern & Neustart.
3. ESP am LIN-Bus anschließen (smartavan: zwischen CP Plus und Heizung durchschleifen, RJ12 und JST jeweils IN/OUT).
4. **Am CP Plus anmelden** (einmalig): Einstellungen → **RESET** → **PR SET** bestätigen. Das Display zeigt „INIT…“ und startet neu. Unter **INDEX** steht danach ein dritter Eintrag **T23.70.0**.
   - War das Board schon mit der smartavan/ESPHome-Firmware angemeldet, ist das in der Regel nicht nötig – es meldet sich mit derselben Kennung.
   - War vorher eine echte iNet Box angemeldet: zuerst ohne iNet Box einmal INIT ausführen, dann mit diesem Gerät.
5. In der Weboberfläche muss **„CP Plus verbunden“** grün sein. Nach ca. 30 s kommt der erste Heizungsstatus; die Uhrzeit wird dann automatisch per NTP ans CP Plus übertragen (und einmal täglich).

## MQTT

Basis-Topic standardmäßig `service/truma` (in den Einstellungen änderbar). Zustände werden **retained** veröffentlicht, Änderungen innerhalb einer Sekunde, alles einmal pro Minute.

### Status `service/truma/control_status/<key>`

| Key | Werte | Bedeutung |
|---|---|---|
| `alive` | ON / OFF | CP Plus fragt dieses Gerät ab |
| `current_temp_room` | °C | Raumtemperatur |
| `target_temp_room` | 0, 5–30 | Raum-Zieltemperatur (0 = aus) |
| `heating_mode` | off / eco / high | Lüfterstufe |
| `current_temp_water` | °C | Wassertemperatur |
| `target_temp_water` | 0 / 40 / 60 / 200 | Warmwasser aus / eco / hot / boost |
| `water_mode` | off / eco / high | wie oben als Text |
| `energy_mix` | gas / mix / electricity | Energiequelle (Diesel = gas) |
| `el_power_level` | 0 / 900 / 1800 | Elektroleistung |
| `operating_status` | 0–9 | 0/1 aus, 4 Start/Nachlauf, ≥5 läuft |
| `operating_status_text` | Text | |
| `error_code` | Zahl | Truma-Fehlercode (0 = keiner) |
| `heater_error` | ON / OFF | Fehler aktiv |
| `clock` | hh:mm | Uhrzeit des CP Plus |
| `timer`, `timer_active` | hh:mm-hh:mm / off, ON/OFF | Timer des CP Plus |
| `hvac_mode`, `hvac_action`, `room_setpoint`, `fan_mode` | | für den HA-Thermostat |
| `decoke`, `decoke_remaining`, `decoke_minutes` | ON/OFF, min, min | Freibrennen |
| `cpplus_version`, `heater_version`, `release`, `rssi`, `lin_traffic` | | Diagnose |

Verfügbarkeit: `service/truma/status` = `online` / `offline` (Last Will).

### Befehle `service/truma/set/<key>`

| Key | Payload | Wirkung |
|---|---|---|
| `target_temp_room` | 0, 5–30 | 0 = Raumheizung aus, sonst einschalten mit dieser Temperatur |
| `heating_mode` | off / eco / high | Lüfterstufe; schaltet bei Bedarf ein (letzte Temperatur) |
| `target_temp_water` | 0 / 40 / 60 | Warmwasser |
| `water_mode` | off / eco / high | Warmwasser |
| `hvac_mode` | heat / off | Raumheizung an/aus mit letzter Temperatur + Lüfterstufe |
| `room_setpoint` | 5–30 | Solltemperatur; bei ausgeschalteter Heizung nur gemerkt |
| `fan_mode` | eco / high | Lüfterstufe; bei ausgeschalteter Heizung nur gemerkt |
| `energy_mix` / `el_power_level` | gas / mix / electricity, 0 / 900 / 1800 | nur bei Geräten mit E-Heizstab |
| `decoke` | ON / OFF | Freibrennen starten / beenden |
| `decoke_minutes` | 5–120 | Dauer des Freibrennens |
| `sync_clock` | 1 | Uhrzeit sofort ans CP Plus senden |
| `reboot` | 1 | Neustart |

Anders als bei inetbox2mqtt muss Temperatur und Lüfterstufe nicht gemeinsam gesendet werden – fehlende Werte werden sinnvoll ergänzt.

Beispiel:
```
mosquitto_pub -h <broker> -t service/truma/set/target_temp_room -m 21
mosquitto_pub -h <broker> -t service/truma/set/heating_mode -m high
mosquitto_sub -h <broker> -t 'service/truma/control_status/#' -v
```

### Home Assistant

Mit aktivierter Auto-Discovery erscheint ein Gerät **„Truma Heater“** mit Thermostat (Aus/Heizen, Solltemperatur, Lüfter eco/high), Warmwasser-Auswahl, Freibrennen-Schalter, Sensoren und Diagnose. Die Heizungs-Entitäten sind nur verfügbar, wenn der ESP online ist **und** das CP Plus ihn abfragt.

## Freibrennen (Diesel)

Setzt 30 °C mit Lüfter **high** (Boost gibt es beim Diesel nicht) für die eingestellte Dauer und stellt danach die vorherigen Einstellungen wieder her. Wird während des Freibrennens am CP Plus oder per MQTT etwas geändert, endet es ohne Wiederherstellung.

## Geräteoptionen

In `config.h`:

- `HEATER_SUPPORTS_BOOST` – Lüfter-Boost (Gas-Combi), Standard `false`
- `WATER_SUPPORTS_BOOST` – Warmwasser-Boost, Standard `false`
- `HEATER_HAS_ELECTRIC` – E-Heizstab (Combi E), blendet die Energiequellen-Auswahl in HA ein
- `LIN_OBSERVER_MODE` – nur mithören, nie antworten (Fehlersuche)

## Fehlersuche

Weboberfläche → **System**:

- **LIN-Verkehr: nein** → Verkabelung, TX/RX vertauscht, 12 V am Transceiver, gemeinsame Masse.
- **LIN-Verkehr ja, CP Plus angemeldet: nein** → am CP Plus RESET → PR SET ausführen.
- **CRC-Fehler steigen ständig** → Störungen oder falsche Baudrate; Masse prüfen.
- **Abgelehnte Befehle** → das CP Plus hat eine Änderung abgelehnt (z. B. nicht unterstützter Modus).
- **smartavan-Board läuft an USB, aber nicht an 12–14 V** → das ist ein Hardware-Problem (Spannungsregler / Versorgung), keine Firmware-Frage. 3,3 V am ESP bei 12-V-Betrieb messen, auch während WLAN sendet.

## Logs ohne USB

- **Weboberfläche → System → Log:** läuft live mit (alle 2 s), solange der Reiter offen ist. Log-Level wählbar, Ausgabe lässt sich leeren und als Textdatei speichern.
- **MQTT:** Logzeilen gehen auf `service/truma/log` (nicht retained). Standard: Warnungen und Fehler. Einstellbar unter **Einstellungen → MQTT → Log per MQTT** (Aus / Fehler / Warnungen / Info / Debug). In Home Assistant erscheint die letzte Meldung als Diagnose-Sensor.
- **Seriell (115200 Baud):** nur am Schreibtisch, siehe Hinweis zu USB in [HARDWARE.md](HARDWARE.md).

Zeitstempel sind Uhrzeit (sobald NTP synchronisiert ist), davor Sekunden seit Start (`+12s`).

## Aufbau des Codes

```
truma_heater/
├── truma_heater.ino     Setup und Hauptschleife
├── config.h             Board-Auswahl, Pins, Geräteoptionen
├── truma_protocol.h     Truma-Frames, Enums, Hilfsfunktionen
├── inetbox.h / .cpp     LIN-Slave + iNet-Box-Protokoll (eigener FreeRTOS-Task)
├── heater_control.*     Sollwert-/Lüfter-Speicher, Freibrennen, Befehlslogik
├── mqtt_ha.*            MQTT-Topics + Home-Assistant-Discovery
├── webserver.*          Weboberfläche, REST-API, OTA
├── web_ui.h             HTML der Weboberfläche
├── wifi_manager.*       WLAN, feste IP, Setup-AP
├── settings.*           Einstellungen im NVS
└── logger.*             Log über Serial + Puffer für die Weboberfläche
```

Der LIN-Teil läuft in einem eigenen Task mit hoher Priorität, damit Antworten an das CP Plus rechtzeitig (wenige Millisekunden) rausgehen – unabhängig von WLAN, MQTT und Weboberfläche.

## Status

- Kompiliert mit Arduino-ESP32 3.x für ESP32, ESP32-S3 und ESP32-C3.
- LIN/iNet-Box-Protokoll am PC gegen echte Beispiel-Frames simuliert (Anmeldung, INIT, Status, Befehl).
- Gegen **echte Mitschnitte** eines CP Plus mit Combi D6 E und Original-iNet-Box geprüft (WomoLIN-Captures): alle Statusframes werden fehlerfrei gelesen, der erzeugte Heizungsbefehl ist im Inhalt byte-gleich mit dem der Original-iNet-Box. Siehe [tests/](tests/README.md).
- **Am echten LIN-Bus noch nicht getestet.**

## Lizenz

GPL-3.0, siehe [LICENSE](LICENSE). Die Protokoll-Logik ist aus [skrebber/esphome-truma_inetbox](https://github.com/skrebber/esphome-truma_inetbox) (GPL-3.0, Fork von Fabian-Schmidt/esphome-truma_inetbox) portiert.
