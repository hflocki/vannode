# MQTT-Topics

Alle Topics der VanNode-Geräte auf einen Blick. Die Tabellen nennen die Vorgaben ab Werk; das Basis-Topic jedes Geräts (`<prefix>`) lässt sich in seiner Web-Oberfläche ändern. Diese Datei ist die vollständige Liste; die README eines Geräts nennt nur das Basis-Topic und verweist hierher. Wer Topics in der Firmware ändert, trägt sie hier ein.

## Gemeinsames Schema

| Topic | Inhalt |
| --- | --- |
| `camper/<gerät>/status` | `online` / `offline`, Last Will, retained |
| `camper/<gerät>/info` | Diagnose-JSON, retained, beim Verbinden und jede Minute: `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset`, `heap` (`devinfo.*` aus `firmware/_common`) |
| `camper/<gerät>/rssi` | WLAN-Signal in dBm, retained |
| `camper/<gerät>/…` | Messwerte und Zustände, retained |
| `camper/<gerät>/cmd/…` bzw. `…/set/…` | Befehle, **nicht** retained |
| `homeassistant/<komponente>/<node>/<id>/config` | Home-Assistant-Discovery, retained (wer HA nicht nutzt, kann sie ignorieren) |

Das Display liest `camper/+/status` und `camper/+/info` für die Geräteliste auf der System-Seite.

## Übersicht

| Gerät | Basis-Topic | Firmware | README |
| --- | --- | --- | --- |
| Tanks (Frischwasser, Grauwasser, Gas) | `camper/tanklevel` | tanklevel | [firmware/tanklevel](../firmware/tanklevel/README.md) |
| Levelsensor | `camper/level` | level | [firmware/level](../firmware/level/README.md) |
| Heizung (Truma) | `camper/heater` | heater | [firmware/heater](../firmware/heater/README.md) |
| Heizung (Alde) | `camper/alde` | alde | [firmware/alde](../firmware/alde/README.md) |
| Dachlüfter (MaxxFan) | `camper/maxxfan` | maxxfan | [firmware/maxxfan](../firmware/maxxfan/README.md) |
| Temperaturen (Kühlschrank …) | `camper/temp` | temp | [firmware/temp](../firmware/temp/README.md) |
| Battery-Node (Batterien: Daly, Ective, BM2, JBD, JK-BMS, LiTime) | `camper/battery` | battery | [firmware/battery](../firmware/battery/README.md) |
| BLE-Node (ble-node: Dachhaube, Pumpe, Kühlschrank, Thermometer, optional Gas; weitere als `camper/ble2`, `camper/ble3`; bis ble 2.5.0 `camper/ble…`) | `camper/ble` | ble (Gerätename ble-node) | [firmware/ble](../firmware/ble/README.md) |
| Display (Wandpanel) | `camper/display` | display | [firmware/display](../firmware/display/README.md) |
| Pocket-Node (M5Stack Core2, bis 1.5.8 Road Buddy) | `camper/pocket` | pocket | [firmware/pocket](../firmware/pocket/README.md) |
| Emulator (Werkzeug am PC) | `camper/emulator` (+ die Topics der nachgebildeten Geräte) | – | [tools/emulator](../tools/emulator/README.md) |
| Muster-Gerät | `camper/example` | template | [firmware/_template](../firmware/_template/README.md) |
| Router RUT956 | `camper/rut`, `camper/door` | Teltonika „Data to Server“ | – |
| Zigbee-Gateway (SMLIGHT) | `camper/zigbee/data`, `camper/zigbee/cmd` | Gateway | [firmware/display](../firmware/display/README.md#sensoren-zigbee--bluetooth) |
| Bluetooth-Gateway (OpenMQTTGateway) | `camper/OMG` | omg | [firmware/omg](../firmware/omg/README.md) |

## Tanks `camper/tanklevel`

| Topic | Inhalt |
| --- | --- |
| `…/water/percent`, `liters`, `voltage` | Frischwasser (`voltage` nur mit Votronic-Geber) |
| `…/water/ss_raw`, `temperature`, `ble_rssi`, `ble_state` | nur Gebertyp SuperSense: Rohwert des gewählten Sensors der Box, Sensortemperatur °C, Bluetooth-Signal, Verbindungsstatus (`OK`, `Verbinde...`, `Nicht erreichbar` …); `ss_raw` und `temperature` erst nach dem ersten gültigen Wert |
| `…/water/filter_active` | `ON` / `OFF` |
| `…/grey/percent`, `liters`, `voltage` | Grauwasser (nur wenn eingeschaltet; `voltage` nur mit Votronic-Geber) |
| `…/grey/ss_raw`, `temperature`, `ble_rssi`, `ble_state` | Grauwasser mit Gebertyp SuperSense, wie bei `water` |
| `…/grey/filter_active` | `ON` / `OFF` |
| `…/gas/level`, `raw_level`, `content` | Gasflasche in %, Rohwert, Restmenge in kg (erst nach dem ersten gültigen Wert) |
| `…/gas/battery`, `ble_rssi`, `ble_state` | Gassensor: Akku, Bluetooth-Signal, `OK` oder Fehler |
| `…/gas/distance`, `temperature`, `quality` | nur Mopeka: Füllhöhe in mm, Sensortemperatur, Messqualität 0–3 |
| `…/cmd/water/filter/set`, `…/cmd/grey/filter/set` | `ON` / `OFF` / `TOGGLE` |

## Levelsensor `camper/level`

| Topic | Wert |
|---|---|
| `…/status` | `online` / `offline` (Last Will) |
| `…/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset` (Grund des letzten Neustarts), `heap` |
| `…/pitch` | Längsneigung in °, positiv = vorne höher |
| `…/roll` | Querneigung in °, positiv = links höher |
| `…/wedge_fl`, `wedge_fr`, `wedge_rl`, `wedge_rr` | Keilhöhe in cm vorne links / vorne rechts / hinten links / hinten rechts |
| `…/is_level` | `ON`, wenn Längs- und Querneigung innerhalb der Toleranz liegen |
| `…/sensor` | `OK` / `ERROR` |
| `…/rssi` | WLAN-Signal |
| `…/tolerance`, `track_width`, `wheelbase`, `mount`, `tare_active`, `tare_pitch`, `tare_roll`, `filter_active`, `mqtt_fast` | Einstellungen (bei Änderung und einmal pro Minute) |

**Befehle** (`PRESS` bzw. Wert, nicht retained):

| Topic | Payload |
|---|---|
| `…/cmd/tare/set` | `PRESS` – aktuelle Lage als waagerecht speichern |
| `…/cmd/tare_reset/set` | `PRESS` |
| `…/cmd/sensor_restart/set` | `PRESS` – BNO085 neu starten |
| `…/cmd/tolerance/set` | `0.1` … `5` |
| `…/cmd/track_width/set`, `wheelbase/set` | cm |
| `…/cmd/mount/set` | Name der Einbaulage, z. B. `Wand links` |
| `…/cmd/tare_pitch/set`, `tare_roll/set` | `-10` … `10` |
| `…/cmd/filter/set`, `mqtt_speed/set` | `ON` / `OFF` / `TOGGLE` |

## Heizung `camper/heater`

Unterstruktur wie bei inetbox2mqtt (bis heater 1.0 lag sie unter `service/truma`). Dazu `…/log`: Logzeilen (nicht retained), Stufe in der Web-Oberfläche einstellbar.

**Status** `…/control_status/<key>` (retained, Änderungen innerhalb einer Sekunde, alles einmal pro Minute)

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
| `error_code` | Zahl | Fehlercode wie am CP Plus, ohne E/W und H (z. B. `212` für E212), 0 = keiner. Berechnung wie in inetbox.py, inetbox2mqtt und ESPHome. Kurz nach dem Start des CP Plus kommt `255` (im WomoLIN-Mitschnitt zu sehen). Bedeutung siehe `error_text` |
| `error_text` | Text | Kurzer deutscher Text zum Fehlercode passend zum eingestellten Heizungsmodell (Gas-Combi, Combi D, VarioHeat; z. B. 122 = `Kraftstoffmangel (…)` bei der Combi D), leer = kein Fehler oder unbekannter Code. Tabelle: `truma_errors.h` (gemeinsam mit dem Display, Quelle `firmware/_common`) |
| `error_kind` | `E` / `W` / leer | `E` = Fehler (muss am CP Plus quittiert werden), `W` = Warnung (verschwindet von selbst), leer = unbekannt |
| `heater_error` | ON / OFF | Fehler oder Warnung aktiv (jeder Code ≠ 0) |
| `clock` | hh:mm | Uhrzeit des CP Plus |
| `vent_level` | 0–10 | Lüftungsstufe (0 = kein Lüften). Nur bei Heizungen, die den LIN-Frame 0x20 nutzen, sonst nicht gesendet |
| `vent_mode` | off / 1–10 / eco / high | Rohwert dazu (eco/high = Lüfter beim Heizen) |
| `timer`, `timer_active` | hh:mm-hh:mm / off, ON/OFF | Timer des CP Plus |
| `timer_times`, `timer_temp_room`, `timer_heating_mode`, `timer_water_mode` | hh:mm-hh:mm, °C, eco/high, off/eco/high | Gespeicherte Timer-Einstellungen (auch bei Timer aus) |
| `hvac_mode`, `hvac_action`, `room_setpoint`, `fan_mode` | | für den HA-Thermostat; `hvac_action` = `fan`, wenn die Heizung ohne Raum- und Wasserheizung läuft (Lüften am CP Plus oder Nachlauf) |
| `decoke`, `decoke_remaining`, `decoke_minutes` | ON/OFF, min, min | Freibrennen |
| `heater_model` | combi / combi_e / combi_d / combi_d_e / vario | gewähltes Heizungsmodell |
| `energy_select` | gas / mix_900 / mix_1800 / electricity_900 / electricity_1800 | Energie als ein Wert (nur E-Modelle) |
| `cpplus_version`, `heater_version`, `release`, `rssi`, `lin_traffic` | | Diagnose |

**Befehle** `…/set/<key>`

| Key | Payload | Wirkung |
|---|---|---|
| `target_temp_room` | 0, 5–30 | 0 = Raumheizung aus, sonst einschalten mit dieser Temperatur |
| `heating_mode` | off / eco / high | Lüfterstufe; schaltet bei Bedarf ein (letzte Temperatur) |
| `target_temp_water` | 0 / 40 / 60 | Warmwasser |
| `water_mode` | off / eco / high | Warmwasser |
| `hvac_mode` | heat / off | Raumheizung an/aus mit letzter Temperatur + Lüfterstufe |
| `room_setpoint` | 5–30 | Solltemperatur; bei ausgeschalteter Heizung nur gemerkt |
| `fan_mode` | eco / high (Gas-Combi auch boost, VarioHeat night / auto / boost) | Lüfterstufe; bei ausgeschalteter Heizung nur gemerkt |
| `energy_mix` / `el_power_level` | gas / mix / electricity, 0 / 900 / 1800 | nur bei E-Modellen |
| `energy_select` | gas / mix_900 / … / electricity_1800 | Energie und Leistung in einem Befehl (nur E-Modelle) |
| `heater_model` | combi / combi_e / combi_d / combi_d_e / vario | Heizungsmodell umstellen (sofort wirksam) |
| `decoke` | ON / OFF | Freibrennen starten / beenden |
| `decoke_minutes` | 5–120 | Dauer des Freibrennens |
| `sync_clock` | 1 | Uhrzeit sofort ans CP Plus senden |
| `timer` | `22:00-06:30[,temp[,eco/high[,off/eco/high]]]`, `on`, `off` | Timer setzen. Fehlende Werte: bisherige Timer-Werte, sonst gemerkte Solltemperatur und Lüfterstufe. Warmwasser ohne Angabe bleibt unverändert |
| `timer_active` | ON/OFF | Timer mit den gespeicherten Zeiten ein- oder ausschalten |
| `sniffer` | ON / ALL / OFF | LIN-Sniffer (Diagnose): `ON` geänderte Frames, `ALL` jeder Frame aller PIDs mit Zeitstempel (Mitschnitt); nach Neustart aus |
| `reboot` | 1 | Neustart |

Anders als bei inetbox2mqtt muss Temperatur und Lüfterstufe nicht gemeinsam gesendet werden – fehlende Werte werden sinnvoll ergänzt.

## Alde-Heizung `camper/alde`

Werte vom Heizbus (passiv mitgelesen, nur wenn der Heizbus in den Einstellungen an ist), Spalte *Quelle* = LIN-Frame und Byte. Alle Werte retained. Ist der Heizbus aus (nur Bedienteil-Bus verdrahtet), fehlen diese Topics.

| Topic | Inhalt | Quelle (Frame @ Byte) |
|---|---|---|
| `…/room` | Raumtemperatur °C | `0x47` @0–1 |
| `…/setpoint` | Sollwert vom Heizbus °C – **nur verlässlich, solange die Klimaanlage ruht** (sonst Ziel- bzw. Raumtemperatur − 3 °C), nicht für Frostschutz verwenden | `0x03` @0–1 |
| `…/glycol` | Glykol-Temperatur °C | `0x5B` @0–1 |
| `…/water` | Warmwasser-Temperatur °C | `0xD6` @4–5 |
| `…/hotwater_target` | Warmwasser-Ziel: `0` = aus, `50` = normal, `65` = voll | `0xC4` @0–1 |
| `…/pcb` | Platinentemperatur °C | `0xD3` @0–1 |
| `…/battery` | 12-V-Batterie V | `0xD6` @6–7 |
| `…/mains_current` | Landstrom des ganzen Fahrzeugs A (nicht nur der Heizung) | `0x5B` @2–3 |
| `…/el_power` | Heizstab-Leistung W (0/1000/2000) | `0x55` @0–1 |
| `…/el_allowed` | erlaubte Heizstab-Leistung W – ein Zustand, sinkt bei Lastabwurf von selbst | `0x06` @0–1 |
| `…/burner_fan` | Brennergebläse U/min (0 = Brenner aus) | `0x5B` @6 |
| `…/gas` | `1` = Gas an, `0` = aus | `0x85` @0 |
| `…/ac_active` | `1` = Klimaanlage wach (abgeleitet: `0x08` antwortet in den letzten 20 s) | `0x08` |
| `…/ac_light` | Licht der Klimaanlage % | `0x97` @6 |
| `…/bus_ok` | `1` = gültige LIN-Frames in den letzten 5 s | – |
| `…/status` | `online` / `offline` (Last Will) | |
| `…/info` | Diagnose (retained): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime`, `reset`, `heap`, `hw` | |
| `…/cmd/measure` | beliebiger Inhalt: Werte sofort senden | |

**Bedienteil-Bus** (nur mit Modus „Nur lesen“ oder „Lesen und steuern“, nicht beim Mithören), Quelle Info-Frame `0x1B`:

| Topic | Inhalt |
|---|---|
| `…/target` | Sollwert °C (der echte Sollwert des Bedienteils) |
| `…/panel_room` | Raumtemperatur am Bedienteil °C |
| `…/outdoor` | Außentemperatur °C (fehlt ohne Fühler) |
| `…/gas_enabled` | `1` = Gasbetrieb an |
| `…/el_level` | Heizstab-Stufe 0–3 (kW) |
| `…/water_mode` | Warmwasser `0` = aus, `1` = an, `2` = Boost (ein unbekannter Modus wird nicht gesendet) |
| `…/pump` | `1` = Umwälzpumpe läuft |
| `…/panel_on` | `1` = Heizung an |
| `…/error` | `1` = Störung |
| `…/ac_input` | `1` = 230 V liegen an |
| `…/ci_ok` | `1` = Bedienteil antwortet (letzte 5 s) |

**Befehle** (nur mit Modus „Lesen und steuern“, nicht retained):

| Topic | Inhalt |
|---|---|
| `…/cmd/target` | Sollwert 5–30 (0,5er-Schritte) |
| `…/cmd/gas` | `1`/`0` bzw. `ON`/`OFF` |
| `…/cmd/el_level` | `0` bis Heizstab-Höchststufe (Einstellung, ab Werk `1`, höchstens `3`); höhere Stufen werden abgelehnt |
| `…/cmd/water` | `0`/`1`/`2` bzw. `aus`/`an`/`boost` |
| `…/cmd/power` | Heizung `1`/`0` bzw. `ON`/`OFF` (`an`/`aus`) |

Unbekannte Werte werden verworfen (sie werden nie zu „aus“). Vor jedem Befehl meldet sich der Node am Bedienteil an und liest den Stand; gesendet wird nur, was sich ändert, und nie, solange das Bedienteil beschäftigt ist oder eine Störung meldet.

## Dachlüfter `camper/maxxfan`

`p<n>` ist der Anschluss am VanTurtle-Board (`p1`, `p2`). Zustände sind retained.

| Topic | Wert |
| --- | --- |
| `…/status` | `online` / `offline` (Last Will) |
| `…/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset` (Grund des letzten Neustarts), `heap` |
| `…/p<n>/auto_hold` | `ON` / `OFF` |
| `…/p<n>/led` | `ON` / `OFF` |
| `…/expander` | `ON` / `OFF` (TCA9555 erreichbar) |
| `…/lid` | `open` / `closed` – nur mit Haubenkontakt (Reedkontakt, ab maxxfan-node 1.9.0) |
| `…/rssi` | WLAN-Signal (dBm) |

**Befehle**

| Topic | Payload | Wirkung |
| --- | --- | --- |
| `…/cmd/p<n>/<taste>` | beliebig (HA: `PRESS`) | Taste drücken: `auto`, `direction`, `on_off`, `faster`, `slower`, `beep` |
| `…/cmd/p<n>/press` | Tastenname | dasselbe, Taste im Payload |
| `…/cmd/p<n>/speed/set` | `0` … `10` | Stufe setzen (Lüfter muss laufen), `0` = Haube schließen |
| `…/cmd/p<n>/close` | beliebig | Haube schließen (11× Langsamer) |
| `…/cmd/p<n>/led/set` | `ON` / `OFF` / `TOGGLE` | Controller-LED |
| `…/cmd/sync_off` | beliebig | Lüfter auf AUS synchronisieren |
| `…/cmd/reboot` | beliebig | Neustart |

Die MaxxFan meldet ihren Zustand (an/aus, Stufe) nicht zurück, es werden nur Tastendrücke gesendet. „Sync auf AUS“ bringt sie in einen bekannten Zustand. Ob die Haube offen ist, meldet `…/lid`, wenn ein Haubenkontakt verbaut ist.

## Temperaturen `camper/temp`

| Topic | Wert |
|---|---|
| `…/status` | `online` / `offline` (Last Will) |
| `…/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset` (Grund des letzten Neustarts), `heap` |
| `…/rssi` | WLAN-Signal |
| `…/<id>` | Temperatur in °C (2 Nachkommastellen), retained |
| `…/<id>/available` | `online` / `offline` – Sensor antwortet am Bus |
| `…/<id>/alarm` | `ON` / `OFF` – Grenzwert dieses Sensors verletzt |
| `…/alarm` | `ON`, solange irgendein Sensor im Alarm ist |
| `…/alarm_text` | Lesbarer Text aller Alarme, z. B. `Kühlschrank zu warm: 9,2 °C (Grenze 8,0 °C)`; leer = kein Alarm |
| `…/sensors` | Alle Sensoren als eine JSON-Liste für Displays: `[{"id":"fridge","name":"Kühlschrank","t":4.31,"alarm":0}, …]` (`t` = `null` ohne gültigen Wert, `alarm` 0 = keiner, 1 = zu kalt, 2 = zu warm) |

Gesendet wird nach jeder Messung (Intervall in der Web-UI, Standard 10 s), spätestens jede Minute. Liefert ein Sensor keinen gültigen Wert (abgezogen, Kabelbruch, 85-°C-Startwert), wird kein Wert gesendet und die Entität in HA als nicht verfügbar markiert.

## Batterien `camper/battery`

Der battery-node hat eine eigene Firmware (`firmware/battery`, bis ble 2.4.0 die BLE-Firmware; Gerätename `battery-node`, Topic `camper/battery`). `<id>` ist die Geräte-ID: `daly`, `daly2`, `ective`, `bm2`, `jbd`, `jk`, `litime` … (bis battery-node 1.x eine frei gewählte ID).

| Topic | Inhalt |
| --- | --- |
| `…/<id>/available` | `online`, solange das BMS antwortet |
| `…/<id>/soc`, `voltage`, `current`, `power`, `charge`, `energy`, `runtime`, `charging` | Messwerte (Strom positiv = Laden) |
| `…/<id>/problem`, `…/<id>/problem_text` | Fehler des BMS |
| `…/<id>/temperature`, `cycles`, `delta`, `cell_1` … | Daly, JBD, JK-BMS, LiTime |
| `…/<id>/state` | nur BM2: `Normal`, `Schwach`, `Kritisch`, `Lädt`, `Erhaltungsladung` |
| `…/<id>/connected`, `…/<id>/release`, `…/<id>/ble_rssi` | Bluetooth-Verbindung, für die Hersteller-App freigegeben, Signal |
| `…/batteries` | alle Batterien als JSON-Liste für Displays: `[{"id","name","ok","t","soc","v","a","w","st","rt","p","pt"}, …]` (`t` = Typ: 1 Daly, 2 Ective, 3 BM2, 4 JBD, 5 JK-BMS, 6 LiTime) |

## BLE-Node `camper/ble`

Jedes Plugin hat eine eigene ID: `rvtech` (Dachhaube), `pump` (CaraWater), `fridge` (WCS), `wipro` (Alarm WiPro III, nur lesen), `obd` (OBD2-Adapter, nur lesen), `dimes`, `mopeka`, `mopeka_std` (Gas; im VanNode-System liest Gas aber tanklevel-node, Display und Pocket-Node nehmen es von dort), `govee` (Thermometer Govee), `bthome` (BTHome-Sensor), `blesensor` (eigener BLE-Sensor), `supersense` (Tanksensor SuperSense). Die zweite Instanz desselben Plugins bekommt eine `2` angehängt (`rvtech2`). Befehle haben die Form `…/cmd/<plugin>/<key>/set`.

| Topic | Inhalt |
| --- | --- |
| `…/<plugin>/available` | `online`, solange das Bluetooth-Gerät erreichbar ist |
| `…/<plugin>/state` | Verbindungsstatus als Text („Verbunden“, „Nicht erreichbar“ …) |
| `…/<plugin>/connected`, `…/<plugin>/release` | `ON` / `OFF`: Verbindung steht, Bluetooth für die Hersteller-App freigegeben |
| `…/rvtech/cover` | Dachhaube `open` / `closed` / `opening` / `closing` |
| `…/rvtech/cover_position` | Position 0 … 100 % |
| `…/rvtech/reported` | gemeldeter Rohwert (0 … 100, `102` = Lüften) |
| `…/cmd/rvtech/cover/set` | `OPEN` / `CLOSE` / `STOP` (Stopp nur während Auf/Zu: die Haube hält an, wenn derselbe Befehl noch einmal kommt) |
| `…/cmd/rvtech/cover_position/set` | `0` … `100` |
| `…/cmd/rvtech/<preset>/set` | Voreinstellung `auf`, `zu`, `regen`, `lueften`, `fav` (Favorit), `fav_set` (Favorit speichern) (Payload beliebig). Die Werte für Regen, Lüften und Favorit stehen in den Einstellungen der Haube |
| `…/cmd/<plugin>/release/set` | `ON` / `OFF`: Bluetooth für die App freigeben |
| `…/pump/running`, `pressure`, `flow`, `voltage`, `current`, `temperature`, `daily_liters`, `total_liters` … | Wasserpumpe; Befehle `enable`, `pressure_level` |
| `…/wipro/armed` (`ON`/`OFF`), `reason`, `open`, `contact0` … `contact7` (`ON`/`OFF`), `raw` | Alarm WiPro III (nur lesen): Scharf-Zustand, Alarmgrund (Text), offene Kontakte als Namensliste, je Kontakt ein Bit aus Byte 6, Rohdaten (Hex) Kontaktnamen im Feld „Kontaktnamen“ der Plugin-Einstellungen |
| `…/obd/rpm`, `speed`, `coolant`, `oil_temp`, `fuel`, `fuel_rate`, `adblue`, `adblue_temp`, `adblue_conc`, `load`, `ambient`, `intake_temp`, `map`, `baro`, `maf`, `throttle`, `ecu_voltage`, `runtime`, `dist_clear`, `odometer`, `dtc_count`, `voltage`, `running` (`ON`/`OFF`), `mil` (`ON`/`OFF`), `dtc`, `protocol`, `adapter`, `raw`, `scan` | OBD2-Adapter (nur lesen): Standardwerte des Motorsteuergeräts (nur die, die das Fahrzeug meldet; `fuel_rate` in l/h, `adblue` Füllstand %, `adblue_temp` °C, `adblue_conc` Konzentration %), Bordspannung am Adapter, Motor läuft, Motorkontrollleuchte, Fehlercodes als Text. `scan` = Bericht des letzten Fahrzeug-Scans (Text, retained). Befehle `…/cmd/obd/read_dtc/set` (`PRESS`), `…/cmd/obd/scan/set` (`PRESS`, Fahrzeug scannen), `…/cmd/obd/raw/set` (Adapter-Befehl, Antwort in `raw`) |
| `…/<gas>/level`, `content`, `battery`, `raw` | Gasflasche (optional; Füllstand %, Inhalt in Litern) |
| `…/supersense/s1_percent` … `s4_percent`, `sN_liters`, `sN_temperature`, `sN_raw`, `state`, `connected`, `ble_rssi` | SuperSense-Empfängerbox (nur lesen): je benutztem Sensor N (1–4) Füllstand %, Inhalt in Litern (Prozent × Tankgröße), Temperatur °C und Rohwert (2475 = 100 % ohne eigene Leer-/Voll-Werte); Status und Verbindung der Box |
| `…/tpms/r1_pressure` … `r8_pressure`, `rN_temperature`, `rN_battery`, `rN_rssi` | Reifendruck SuperSense TireCheck (nur mithören): je belegtem Rad N (1–8, Position in den Einstellungen) Druck in bar, Reifentemperatur °C, Batterie %, Signal dBm |
| `…/tpms/r1_alarm` … `r8_alarm` | `ON` / `OFF` je Rad: Druck unter bzw. über der Grenze seiner Achse |
| `…/tpms/warning`, `warning_text` | `ON` / `OFF`, wenn irgendein Rad außerhalb der Grenzen seiner Achse liegt; Text z. B. `Vorne links: 2.80 bar zu niedrig` oder `OK` |
| `…/govee/temperature`, `humidity`, `battery`, `rssi` | Thermometer Govee: °C, %, Batterie %, Signal |
| `…/bthome/<wert>`, `rssi` | BTHome-Sensor: je erkanntem Wert ein Topic, z. B. `temperature`, `humidity`, `battery`, `voltage`, `door` (`ON`/`OFF`), `button` (`press` …); ein zweiter gleicher Wert bekommt `_2` |
| `…/blesensor/v1` … `v3`, `rssi`, `raw` | Eigener BLE-Sensor: die eingetragenen Werte, Signal, Rohdaten (Hex) |

**Kühlschrank** (`fridge`). Die Topics heißen nach der Zone (`…1` = Zone 1, `…2` = Zone 2); welches Fach das ist, legen die Felder *Name Zone 1/2* fest (leer = Gefrierfach / Kühlfach, bei einer Zone Innenraum).

| Topic | Inhalt |
| --- | --- |
| `…/fridge/temp1`, `temp2`, `set1`, `set2` | Ist- und Soll-Temperatur in °C |
| `…/fridge/voltage`, `battery` | Spannung in V, Batterie in % (laut Kühlschrank) |
| `…/fridge/fan`, `night` | `ON` / `OFF`: Umluft (im Nachtmodus aus), Nachtmodus |
| `…/fridge/zone1`, `zone2` | `ON` / `OFF`: Zone läuft (nur mit „Zweite Zone“) |
| `…/fridge/quick1`, `quick2`, `doorheat` | `ON` / `OFF`: Schnellkühlen Zone 1 / 2, Türheizung |
| `…/fridge/error` | aktueller Fehler: E1 Unterspannung, E2 Lüfter, E3 Anlauf Kompressor, E4 Kompressor, E5 Kompressorsteuerung, E6 Temperaturfühler, E7 Tür offen |
| `…/fridge/error_last` | letzter Fehler mit Zeit, bleibt nach dem Ende stehen |
| `…/fridge/errors` | die letzten 5 Fehler als JSON: `code`, `text`, `zone`, `zone_name`, `first`, `last`, `ago_s`, `count` (nur im RAM, nach einem Neustart leer) |
| `…/fridge/message` | letzte Nachricht des Kühlschranks (Diagnose) |
| `…/cmd/fridge/power/set` | `ON` / `OFF`: „Power Gesamt“, ganzer Kühlschrank |
| `…/cmd/fridge/zone1/set`, `zone2/set` | `ON` / `OFF`: Zone einzeln; die andere bleibt, wie sie ist |
| `…/cmd/fridge/fan/set`, `night/set`, `quick1/set`, `quick2/set`, `doorheat/set` | `ON` / `OFF` |
| `…/cmd/fridge/target1/set`, `target2/set` | Soll-Temperatur in °C (auch wenn der Kühlschrank auf °F steht) |
| `…/cmd/fridge/bpd/set` | Batterieschutz Stufe 1–3 |
| `…/cmd/fridge/raw/set` | Diagnose: beliebiger Befehl an den Kühlschrank, die Antwort steht im Geräte-Log |

## Display `camper/display`

| Topic | Inhalt |
| --- | --- |
| `…/status` | `online` / `offline` (Last Will) |
| `…/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset` (Grund des letzten Neustarts), `heap` |
| `…/ip`, `/rssi`, `/brightness`, `/screen` | Zustand (retained) |
| `…/cmd/brightness` | `5` … `100` |
| `…/cmd/screen` | `ON` / `OFF` |
| `…/cmd/page` | Seitenname (`Heizung`) oder Nummer (`0` = erste) |
| `…/awning` | Markise `ON` (ausgefahren) / `OFF` / `unknown` (retained) |
| `…/cmd/awning` | `ON` / `OFF` / `TOGGLE` (nur ohne Kontakt-Sensor) |
| `…/driving` | Abfahrtscheck: `ON` während der Fahrt, sonst `OFF` (retained) |
| `…/depart` | Abfahrtscheck: `OK` oder die offenen Teile, z. B. `Tür offen, Markise ausgefahren` (retained) |
| `…/cmd/reboot` | beliebig |

Das Display abonniert `<Basis-Topic>/#` jedes Geräts, die Router-Topics, die Gateways und die Topics der eigenen Kacheln und Automationen (frei eingestellt, z. B. `zigbee2mqtt/…`). Befehle sendet es nicht retained.

**Gelesene Werte**

| Seite | Topics |
| --- | --- |
| Tanks | `water/percent`, `water/liters`, `grey/percent`, `grey/liters`, `gas/level`, `gas/content`, `status` |
| Ausrichtung | `pitch`, `roll`, `is_level`, `tolerance`, `wedge_fl/fr/rl/rr`, `status` |
| Heizung | `control_status/current_temp_room`, `room_setpoint`, `hvac_mode`, `fan_mode`, `water_mode`, `current_temp_water`, `operating_status_text`, `error_code`, `heater_error`, `decoke`, `decoke_remaining`, `alive`; `status` |
| MaxxFan | `p<n>/auto_hold`, `p<n>/led`, `status` |
| Dachhaube (BLE-Node) | `rvtech/cover`, `rvtech/cover_position`, `rvtech/reported` (102 = Lüften), `rvtech/connected`, `status` |
| Temperaturen | `sensors` (JSON-Liste aller Sensoren mit Name, Wert und Alarm), `alarm_text`, `status` |
| Batterien | `batteries` (JSON-Liste aller Batterien mit Ladestand, Spannung, Leistung, Restlaufzeit, BM2-Zustand), `status`. Seite *Batterien*: je Batterie eine Karte mit Ladestand-Balken, Leistung (▲ lädt / ▼ entlädt), Restlaufzeit; beim BM2 Spannung und Zustand |

**Gesendete Befehle**

| Bedienung | Topic | Payload |
| --- | --- | --- |
| Heizung an/aus | `<heizung>/set/hvac_mode` | `heat` / `off` |
| Solltemperatur | `<heizung>/set/room_setpoint` | `5` … `30` (1 s nach dem letzten Tippen) |
| Gebläse | `<heizung>/set/fan_mode` | `eco` / `high` |
| Warmwasser | `<heizung>/set/water_mode` | `off` / `eco` / `high` |
| Freibrennen | `<heizung>/set/decoke` | `ON` / `OFF` (am Display lange drücken) |
| Lüfter-Tasten | `<maxxfan>/cmd/p<n>/<taste>` | `PRESS` (`on_off`, `auto`, `faster`, `slower`, `direction`, `close` = Haube schließen, ab maxxfan-node 1.4.0) |
| Lüfter-Stufe | `<maxxfan>/cmd/p<n>/speed/set` | `1` … `10` |
| Sync AUS | `<maxxfan>/cmd/sync_off` | `PRESS` |
| Tara | `<level>/cmd/tare/set` | `PRESS` (am Display lange drücken) |
| Dachhaube Auf / Zu / Regen / Lüften | `<ble>/cmd/rvtech/<auf\|zu\|regen\|lueften>/set` | `PRESS` |
| Dachhaube Position | `<ble>/cmd/rvtech/cover_position/set` | `0` … `100` (beim Loslassen des Reglers) |

## Pocket-Node `camper/pocket`

Reiner Empfänger, fragt nichts ab. Gelesen:

| Topic | Bedeutung |
| ----- | --------- |
| `camper/level/pitch` | Neigung vorne/hinten in Grad |
| `camper/level/roll` | Neigung links/rechts in Grad |
| `camper/level/wedge_fl`, `_fr`, `_rl`, `_rr` | Keilhöhe in cm unter jedem Rad (vorne links, vorne rechts, hinten links, hinten rechts) |
| `camper/level/is_level` | `ON` / `OFF`: steht das Fahrzeug innerhalb der Toleranz |
| `camper/level/tolerance` | Toleranz in Grad (Größe des Zielrings) |
| `camper/level/status` | `online` / `offline` von level-node |
| `camper/tanklevel/water/percent` | Füllstand Frischwasser in % |
| `camper/tanklevel/water/liters` | Tankinhalt in Litern |
| `camper/tanklevel/gas/level` | Füllstand Gasflasche in % |
| `camper/tanklevel/gas/content` | Restmenge der Gasflasche (kg) |
| `camper/tanklevel/gas/battery` | Akku des Gassensors in % |
| `camper/tanklevel/gas/ble_state` | `OK`, sonst wird „BLE“ als Warnung gezeigt |
| `camper/tanklevel/status` | `online` / `offline` (Last Will, gilt für Wasser und Gas) |
| `camper/temp/sensors` | Alle Temperatursensoren als JSON-Liste: `[{"id":"fridge","name":"Kühlschrank","t":4.31,"alarm":0}, …]` |
| `camper/temp/status` | `online` / `offline` von temp-node |
| `camper/battery/batteries` | Alle Batterien als JSON-Liste: `[{"name":"Bordbatterie","ok":true,"soc":78,"v":13.31,"w":-56,"rt":2830}, …]` (ab battery-node 1.2.2 mit Restlaufzeit) |
| `camper/battery/status` | `online` / `offline` von battery-node |
| `camper/router/mobile` | Mobilfunkdaten des Routers als JSON (`rsrp`, `rssi`, Netztyp), für die Signalbalken. Einstellbar mit `ROUTER_TOPIC` in `config_local.h`, `""` schaltet es ab |

Eigene Topics (sendet die Pocket-Node selbst, Präfix änderbar mit `#define SELF_TOPIC` in `config_local.h`):

| Topic | Bedeutung |
| ----- | --------- |
| `camper/pocket/status` | `online` / `offline` (Last Will) |
| `camper/pocket/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset`, `heap` |

Weitere Topics unter diesen Präfixen (z. B. `rssi`, `voltage`, `raw_level`, `tare_*`, Grauwasser) werden ignoriert.

## Muster-Gerät `camper/example`

| Topic | Inhalt |
|---|---|
| `…/status` | `online` / `offline` (Last Will) |
| `…/info` | Diagnose (retained, beim Verbinden und jede Minute): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime` (s), `reset` (Grund des letzten Neustarts), `heap` |
| `…/rssi` | WLAN-Signal in dBm |
| `…/<key>` | ein Topic pro Wert, siehe `VALUES[]` in `sensor.cpp` |
| `…/cmd/measure` | beliebiger Inhalt: sofort messen |

Ein neues Gerät aus der Vorlage ergänzt hier seinen eigenen Abschnitt.

## Router RUT956

Kommt vom Router selbst („Data to Server“), nicht aus diesem Repo.

| Topic | Inhalt | Wer liest |
| --- | --- | --- |
| `camper/rut` | JSON, retained: `TIME` und `GPS` mit `latitude`, `longitude`, `altitude`, `speed` (km/h), `accuracy`, `satellites`, `fix_status`, `timestamp`; Werte teils als Text | Display: Abfahrtscheck; lässt sich auch als GPS-Topic fürs Wetter eintragen |
| `camper/door` | `{"Door":{"name":"din2","type":"Digital In","state":"open"\|"close","custom_name":"Door"}}` | Display: Abfahrtscheck |
| `camper/router/mobile` | Mobilfunk-Signal als JSON (`rsrp`, `rssi`, Netztyp) | Display (Balken in der Kopfzeile), Pocket-Node |

`camper/router/mobile` ist die Vorgabe in Display und Pocket-Node. Ob und unter welchem Topic der Router die Mobilfunkdaten sendet, ist noch nicht bestätigt; bis dahin kommen sie nur vom Simulator.

## Gateways

| Topic | Inhalt |
| --- | --- |
| `camper/zigbee/data/<ieee>/1/<cluster>/<attribut>` | Zigbee-Attribute vom SMLIGHT-Gateway, z. B. `0006/0000` Kontakt, `0402/0000` Temperatur, `0405/0000` Luftfeuchte, `0001/0021` Batterie |
| `camper/zigbee/cmd/<ieee>/1/<cluster>/<befehl>` | Befehle an Zigbee-Lampen und -Schalter (vom Display) |
| `camper/OMG/BTtoMQTT/<MAC>` | Bluetooth-Thermometer über OpenMQTTGateway (`tempc`, `hum`, `batt` …) |

## Werkzeuge

Der **Emulator** (`tools/emulator`, Python mit lokaler Webseite) sendet die Topics von tanklevel, level, heater, maxxfan, temp, battery, Router und Zigbee-Gateway in denselben Formaten wie die echten Geräte, jedes Gerät über eine eigene Verbindung mit Last Will. Er selbst meldet sich unter `camper/emulator/status` und `camper/emulator/info`; die `info` der nachgebildeten Geräte trägt die Version `emu-py-…`. Befehle (`…/cmd/…`, `…/set/…`, Zigbee `camper/zigbee/cmd/…`) setzt er wie das echte Gerät um. Home-Assistant-Discovery sendet er nicht.

Der Simulator `tools/mqtt/sim.py` sendet dieselben Topics wie die Firmware (retained), dazu `camper/rut`, `camper/door` und `camper/router/mobile` wie der Router und mit `--contact`, `--fridge`, `--outdoor` Zigbee-Werte. Beim Szenario `depart` schickt er `camper/display/cmd/awning`. Mit `--cleanup` löscht er beim Beenden alle retained Topics, die er gesendet hat.

## Alte Topics aufräumen

Retained Nachrichten bleiben am Broker, bis man sie mit einer leeren Nachricht überschreibt. Diese stammen von früheren Versionen oder älteren Simulator-Läufen und werden nicht mehr gebraucht:

| Topic | Herkunft |
| --- | --- |
| `camper/router/gps` | Simulator bis display-v2.9.15 (jetzt `camper/rut`) |
| `camper/roadbuddy/status`, `camper/roadbuddy/info` | Road Buddy bis 1.5.8 (jetzt Pocket-Node, `camper/pocket`) |
| `camper/waterlevel/#`, `camper/gaslevel/#` | Einzelprojekte vor `camper/tanklevel` |
| `camper/truma/#`, `service/truma/#` | frühere Heizungs-Firmware (bis heater 1.0 `service/truma`); nur löschen, wenn kein Gerät mehr darauf eingestellt ist |
| `homeassistant/sensor/camper_water/#`, `homeassistant/switch/camper_water/#`, `homeassistant/sensor/camper_gaslevel/#` | Discovery der Einzelprojekte |

Vorher nachsehen, was wirklich noch da ist (zeigt nur retained Nachrichten, nach 3 s Ende):

```bash
mosquitto_sub -h <broker> -t 'camper/#' -v --retained-only -W 3
```

Ein einzelnes Topic löschen:

```bash
mosquitto_pub -h <broker> -t camper/router/gps -r -n
```

Bei `#`-Einträgen jedes Topic einzeln löschen, das `mosquitto_sub` zeigt (`mosquitto_pub` kennt keine Wildcards).
