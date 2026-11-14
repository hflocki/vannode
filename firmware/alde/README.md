# Alde Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.de.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

> ⚠️ **IN ENTWICKLUNG** – noch nicht an einer echten Alde-Heizung getestet. Die Dekodierung ist aus [alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) übernommen und dort am Bedienteil geprüft, diese Arduino-Umsetzung aber bisher nur mit künstlichen Bus-Daten (`tests/run.sh`).

Liest die Werte einer **Alde Compact 3020 HE** (Warmwasser-Heizung) passiv vom LIN-Bus und stellt sie per **MQTT** bereit – für das Display, die Road Buddy und optional Home Assistant.

**Nur lesen, keine Steuerung.** Das Alde-Bedienteil ist LIN-Master; wie man der Heizung Befehle gibt, ist bisher nicht entschlüsselt (ausführlich: [CONTROL-NOT-SOLVED.md](https://github.com/Mellowgroup-dk/alde-3020he-lin/blob/main/docs/CONTROL-NOT-SOLVED.md)). Die Firmware öffnet den UART **ohne TX-Pin** und kann deshalb nichts auf den Bus schreiben, auch nicht aus Versehen. Das Bedienteil arbeitet ganz normal weiter.

Teil des Verbunds [VanNode](../../README.md): eigene **Web-Oberfläche**, Home-Assistant-Discovery, Updates ohne USB.

## Funktionen

- 13 Werte vom Bus plus „Klimaanlage aktiv“ und „LIN-Bus“ unter `camper/alde/<key>`, Home Assistant legt die Entitäten automatisch an (Gas, Klimaanlage aktiv und LIN-Bus als Binärsensoren)
- Werte gehen bei Änderung raus, höchstens alle *Sendeabstand* Sekunden (Standard 5 s), unverändert spätestens jede Minute
- Ungültige Werte (Platzhalter `0xFFFF`/`0x7FFF`, unbekannte Gas-Codes, falsche Prüfsumme) werden verworfen, der letzte gültige Wert bleibt stehen
- Web-Oberfläche: Live-Werte, Bus-Zustand, Einstellungen, MQTT, WLAN, optionaler Login
- Setup-WLAN `Alde-Node-XXXXXX` (192.168.4.1) beim ersten Start und wenn das WLAN 60 s fehlt
- Firmware (`.bin`) und Web-Oberfläche (`index.html.gz`) im Web-UI hochladbar, dazu Arduino-OTA

## Hardware

| Teil | Hinweis |
|---|---|
| ESP32 (z. B. ESP32-WROOM-32 DevKit) | wie im Original `esp32dev`; ESP32-C3 geht auch (nutzt dann UART1) |
| LIN-Transceiver, z. B. TJA1020-Modul | **RXD → GPIO16** (`LIN_RX_PIN`), TXD bleibt frei. SLP/NSLP auf High legen oder an einen GPIO (`LIN_SLP_PIN`), sonst ist der Empfänger aus |
| RJ12-Splitter | an der Buchse **„Control panel“** der Heizung, das Original-Bedienteil bleibt dran |

LIN, Masse und 12 V kommen über das RJ12-Kabel. Das Bedienteil muss angeschlossen bleiben: Es ist der Bus-Master, ohne es ist auf dem Bus komplett Ruhe.

## Arduino IDE

**Board-Paket:** esp32 von Espressif **3.3.x**

| Einstellung | Wert |
|---|---|
| Board | ESP32 Dev Module (bzw. ESP32C3 Dev Module, dann USB CDC On Boot **Enabled**) |
| Partition Scheme | **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** |

**Bibliotheken:**

| Bibliothek | Autor | Version |
|---|---|---|
| PubSubClient | Nick O'Leary | 2.8 |
| ArduinoJson | Benoit Blanchon | 7.4.2 |
| ESPAsyncWebServer | ESP32Async | 3.12.1 |
| AsyncTCP | ESP32Async | 3.5.0 |

**Erstes Flashen (USB):**
1. `config_local.h.example` nach `config_local.h` kopieren und WLAN, MQTT und Passwörter eintragen.
2. Sketch hochladen.
3. Web-Oberfläche einmalig: im Browser `http://<ip>/` öffnen, dort steht ein Upload-Formular → `data/index.html.gz` hochladen.

## Updates ohne USB

| Was | Wie |
|---|---|
| Firmware | Arduino IDE → *Sketch → Kompilierte Binärdatei exportieren*, die `.bin` im Web-UI unter **WiFi → Firmware- & Web-Update** hochladen (Neustart), oder per Netzwerkport in der IDE (OTA-Passwort) |
| Web-Oberfläche | `data/index.html.gz` im Web-UI hochladen – ersetzt nur die Seite, ohne Neustart, Einstellungen bleiben |
| Komplettes LittleFS-Image | Nur per USB oder IDE-Netzwerkport; löscht dabei auch die Einstellungen (`config.json`) |

## MQTT / Home Assistant

| Topic | Inhalt | Quelle (Frame @ Byte) |
|---|---|---|
| `camper/alde/room` | Raumtemperatur °C | `0x47` @0–1 |
| `camper/alde/setpoint` | Sollwert °C – **nur verlässlich, solange die Klimaanlage ruht** (sonst Ziel- bzw. Raumtemperatur − 3 °C), nicht für Frostschutz verwenden | `0x03` @0–1 |
| `camper/alde/glycol` | Glykol-Temperatur °C | `0x5B` @0–1 |
| `camper/alde/water` | Warmwasser-Temperatur °C | `0xD6` @4–5 |
| `camper/alde/hotwater_target` | Warmwasser-Ziel: `0` = aus, `50` = normal, `65` = voll | `0xC4` @0–1 |
| `camper/alde/pcb` | Platinentemperatur °C | `0xD3` @0–1 |
| `camper/alde/battery` | 12-V-Batterie V | `0xD6` @6–7 |
| `camper/alde/mains_current` | Landstrom des ganzen Fahrzeugs A (nicht nur der Heizung) | `0x5B` @2–3 |
| `camper/alde/el_power` | Heizstab-Leistung W (0/1000/2000) | `0x55` @0–1 |
| `camper/alde/el_allowed` | erlaubte Heizstab-Leistung W – ein Zustand, sinkt bei Lastabwurf von selbst | `0x06` @0–1 |
| `camper/alde/burner_fan` | Brennergebläse U/min (0 = Brenner aus) | `0x5B` @6 |
| `camper/alde/gas` | `1` = Gas an, `0` = aus | `0x85` @0 |
| `camper/alde/ac_active` | `1` = Klimaanlage wach (abgeleitet: `0x08` antwortet in den letzten 20 s) | `0x08` |
| `camper/alde/ac_light` | Licht der Klimaanlage % | `0x97` @6 |
| `camper/alde/bus_ok` | `1` = gültige LIN-Frames in den letzten 5 s | – |
| `camper/alde/status` | `online` / `offline` (Last Will) | |
| `camper/alde/info` | Diagnose (retained): JSON mit `version`, `ip`, `mac`, `rssi`, `uptime`, `reset`, `heap`, `hw` | |
| `camper/alde/cmd/measure` | beliebiger Inhalt: Werte sofort senden | |

Alle Werte retained. Es gibt bewusst **keine** Steuer-Topics.

## Serieller Monitor (115200 Baud)

| Befehl | Wirkung |
|---|---|
| `read` | Bus-Statistik (gültig/verworfen/leere Slots) und alle Werte |
| `measure` | Werte sofort per MQTT senden |
| `sig` | Mitschnitt an/aus: jeder Signal-Frame, dessen Inhalt sich ändert, als Hex (`[SIG] 47 (len=8): …`) – zum Vergleichen mit dem Bedienteil |
| `noauth` | Web-Login löschen |
| `help` | Befehle anzeigen |

## Tests ohne Heizung

`tests/run.sh` übersetzt den Parser (`alde_lin.cpp`) für den PC und schickt einen künstlichen Bus-Zyklus mit den Rohwerten aus der Original-Doku durch (Byte für Byte, Platzhalter, falsche Prüfsumme, Warmwasser aus, Klimaanlage wach, Bus-Timeout). Läuft auch in der GitHub Action.

```bash
bash firmware/alde/tests/run.sh
```

## Sicheres Update, Ereignisse und Rückfall

Jede Firmware hat eine Kennung (`CAMPER-FW:alde:<version>:<chip>;`) und dieselbe Absicherung wie alle VanNode-Geräte (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`):

- **Prüfung beim Update** (Web-UI unter **WiFi → Firmware- & Web-Update**): Die installierte Version steht dabei. Eine gewählte Datei wird schon im Browser und beim Hochladen im Gerät noch einmal geprüft, bevor etwas aktiviert wird. Abgelehnt werden eine `.merged.bin` (komplettes Flash-Abbild für USB), Firmware für ein anderes Gerät oder einen anderen Chip und zu große Dateien. Fehler stehen im Klartext da statt „Update failed“.
- **Ereignisse:** Die letzten Ereignisse (Start mit Grund, WLAN und MQTT verbunden/getrennt, Uploads mit Ergebnis, Rückfall, gespeicherte Einstellungen) stehen in der Web-Oberfläche. Sie bleiben bei Neustart, Absturz und Update erhalten, nach einem Stromausfall ist das Log leer.
- **Rückfall nach Update:** Hat eine neue Firmware nach 5 Minuten kein WLAN oder startet sie mehr als dreimal neu, startet wieder die vorherige. Kam das Update über den Setup-AP (ohne WLAN), wird nur auf Neustart-Schleifen geachtet.
- **WLAN-Änderung mit Rückweg:** Werden die WLAN-Daten geändert, während das Gerät verbunden ist, merkt es sich die funktionierenden. Klappt die Verbindung mit den neuen nach dem Neustart etwa 1 Minute lang nicht, kommen die alten zurück.

## Versionen

| Version | Änderungen |
|---|---|
| 0.1.0 | Erste Version aus der VanNode-Vorlage: passiver LIN-Leser für die Alde Compact 3020 HE (Dekodierung aus alde-3020he-lin), MQTT, HA-Discovery, Web-UI, Host-Test |

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](NOTICE.md)). Fremde Teile: Die LIN-Dekodierung stammt aus [Mellowgroup-dk/alde-3020he-lin](https://github.com/Mellowgroup-dk/alde-3020he-lin) (Code MIT, siehe [LICENSE-alde-MIT](LICENSE-alde-MIT), Dokumentation CC BY 4.0). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

