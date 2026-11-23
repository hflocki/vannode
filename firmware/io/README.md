# IO Node

> **Achtung:** Ohne Gewähr, Nutzung auf eigene Gefahr – siehe [Haftungsausschluss](../../README.md#haftungsausschluss--nutzung-auf-eigene-gefahr).

Relais, Eingänge, Temperaturfühler und Analogwerte für das Wohnmobil auf einem ESP32-C3 Super Mini. Was an welchem Pin hängt, stellst du komplett in der Web-Oberfläche ein, im Code ist nichts fest verdrahtet.

ESP32-Firmware im Verbund [VanNode](../../README.md): Werte und Befehle per **MQTT** mit Home-Assistant-Discovery, eigene **Web-Oberfläche** zum Einrichten, Updates ohne USB.

## Funktionen

- **Relais** an einem GPIO, an einer **74HC595**-Kette (bis 4 Chips = 32 Ausgänge) oder an **I2C-Expandern** (MCP23017 mit 16 bzw. PCF8574 mit 8 Pins, bis 8 Chips)
  - *Relais schaltet bei* LOW oder HIGH (je nach Relaismodul)
  - Zustand nach dem Start: aus, an oder letzter Zustand
  - optional Impuls: schaltet nach x ms von selbst wieder aus (Stromstoßrelais, Taster-Ersatz)
  - beim Start wird erst der Ruhepegel gesetzt, dann der Pin zum Ausgang: kein kurzes Klacken
- **Eingänge** an einem GPIO oder am Expander, mit *Umkehren* (LOW = an), Pull-up und Entprellen
- **DS18B20** an einem 1-Wire-Pin, Fühler per *Suchen* finden und übernehmen, Offset je Fühler
- **Analog** (GPIO 0–4 beim C3) mit Zweipunkt-Umrechnung mV → Wert und eigener Einheit, z. B. Batteriespannung über Spannungsteiler
- **Pin-Belegung** in der Web-Oberfläche, mit Warnung bei doppelt belegten Pins und bei Boot-Pins
- Web-Oberfläche: Live mit Schaltflächen, Kanäle, Hardware, MQTT, WLAN, Login (ab Werk `admin` / `admin`)
- Setup-WLAN `IO-Node-XXXXXX` (192.168.4.1) beim ersten Start und wenn das WLAN 60 s fehlt
- Ersatz-Broker, Englisch (`UI_LANG`), Updates im Web-UI, Ereignisse und Rückfall wie bei allen VanNode-Geräten

## Hardware

| Teil | Hinweis |
|---|---|
| ESP32-C3 Super Mini | auch ESP32-S3 möglich |
| Relaismodule | die üblichen 5-V-Module mit Optokoppler schalten mit 3,3 V; ob bei LOW oder HIGH, steht pro Relais in der Web-Oberfläche |
| 74HC595 (optional) | 3 Pins (Daten, Takt, Latch), optional OE mit 10 kΩ nach 3,3 V; dazu ein Treiber (z. B. ULN2803) oder ein fertiges 595-Relaisboard |
| MCP23017 / PCF8574 (optional) | 2 Pins (SDA, SCL), Adressen der Chips hintereinander (A0–A2) |
| DS18B20 (optional) | alle an einem Pin, 4,7 kΩ Pull-up nach 3,3 V |

### Pins beim ESP32-C3 Super Mini

| GPIO | Nutzbar für | Hinweis |
|---|---|---|
| 0, 1, 3, 4 | alles, auch Analog | ADC1 |
| 5, 6, 7, 10 | Relais, Eingänge, Bus | |
| 20, 21 | Relais, Eingänge | RX/TX der seriellen Schnittstelle |
| 2, 8, 9 | nur mit Vorsicht | Boot-Pins (8 = LED, 9 = BOOT-Taste): ein Relaismodul daran kann den Start verhindern |

Beispiel: 1-Wire an GPIO 3, 74HC595 an 5/6/7 (16 Relais), I2C an 0/1 mit einem MCP23017 (16 Eingänge), Analog an GPIO 4 – bleiben 10, 20, 21 frei.

## Einrichten

1. Tab **Hardware**: 1-Wire-Pin, 74HC595 (Anzahl und Pins) und I2C-Expander (Typ, Anzahl, Pins, Adresse) einstellen, **Speichern & Neustart**.
2. Tab **Hardware → Suchen**: zeigt die gefundenen DS18B20 und I2C-Adressen; neue Fühler lassen sich direkt als Kanal anlegen.
3. Tab **Kanäle**: Relais, Eingänge, Temperaturen und Analogwerte anlegen. Die ID ist das MQTT-Topic, **Speichern & Neustart**.
4. Tab **Live**: Relais antippen zum Schalten.

Einstellungen der Kanäle liegen in `/io.json` (LittleFS), der letzte Zustand der Relais im NVS.

## Arduino IDE

Board-Paket, gemeinsame Bibliotheken, Board-Einstellungen, erstes Flashen, LittleFS-Plugin und Hilfe bei Upload-Problemen: **[docs/ARDUINO.md](../../docs/ARDUINO.md)**. Hier nur, was für dieses Gerät gilt.

| Einstellung | Wert |
|---|---|
| Board | ESP32-C3 Super Mini (bzw. ESP32-S3), Einstellungen siehe docs/ARDUINO.md |
| Zusätzliche Bibliotheken | OneWire, DallasTemperature (wie beim temp-node) |
| Web-Oberfläche | einmalig über das Upload-Formular (`http://<ip>/` → `data/index.html.gz`) |

## Updates ohne USB

| Was | Wie |
|---|---|
| Firmware | Arduino IDE → *Sketch → Kompilierte Binärdatei exportieren*, die `.bin` im Web-UI unter **WiFi → Firmware- & Web-Update** hochladen (Neustart), oder per Netzwerkport in der IDE (OTA-Passwort) |
| Web-Oberfläche | `data/index.html.gz` im Web-UI hochladen – ersetzt nur die Seite, ohne Neustart, Einstellungen bleiben |
| Komplettes LittleFS-Image | Nur per USB oder IDE-Netzwerkport; löscht dabei auch die Einstellungen (`config.json`, `io.json`) |

## MQTT / Home Assistant

Basis-Topic `camper/io`: `camper/io/<id>` je Kanal (Relais und Eingänge `ON` / `OFF`, Temperatur und Analog als Zahl), Relais schalten über `camper/io/cmd/<id>` mit `ON`, `OFF` oder `TOGGLE`. Home Assistant: Relais als Schalter, Eingänge als Binärsensor, Temperatur und Analog als Sensor. Gelöschte Kanäle verschwinden nach dem Neustart auch aus Home Assistant. Alle Topics: **[docs/TOPICS.md](../../docs/TOPICS.md#io-node-camperio)**.

## Serieller Monitor (115200 Baud)

`read` (alle Kanäle anzeigen), `<id> on|off|toggle` (Relais schalten), `noauth` (Web-Login auf Werks-Login zurücksetzen), `help`.

## Sicheres Update, Ereignisse und Rückfall

Wie bei allen VanNode-Geräten (`fwcheck.*`, `evlog.*`, `safeguard.*` aus `firmware/_common`): Prüfung der Datei beim Update, Ereignis-Log, Rückfall auf die vorherige Firmware, wenn eine neue kein WLAN bekommt oder in einer Neustart-Schleife hängt.

## Lizenz

GNU GPL Version 3 mit zusätzlichen Bedingungen (Urhebervermerk bleibt erhalten, veränderte Fassungen sind zu kennzeichnen), siehe [LICENSE](LICENSE) und [NOTICE.md](../../NOTICE.md). Name und Logo „VanNode“ sind nicht Teil der Lizenz: [NAME-UND-LOGO.md](../../NAME-UND-LOGO.md).

Ohne Gewähr, Nutzung auf eigene Gefahr; die Haftung ist ausgeschlossen, soweit gesetzlich zulässig (siehe [NOTICE.md](../../NOTICE.md)). Produkt- und Firmennamen sind Marken ihrer Inhaber, VanNode hat keine Verbindung zu ihnen.

