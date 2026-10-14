# Hardware & Verdrahtung

## Prinzip

```
 Truma Heizung ── LIN ── CP Plus (Master)
                   │
                   └── LIN-Transceiver ── UART ── ESP32 (Slave, iNet-Box-Emulation)
```

Der ESP hängt **parallel** am LIN-Bus, so wie eine echte iNet Box. Er ersetzt nichts und schaltet nichts um. Es gibt keinen Umschalter und keine Unterbrechung der Verbindung CP Plus ↔ Heizung.

## Variante A: smartavan heater controller

Fertige Platine, nichts zu verdrahten:

1. RJ12-Kabel vom CP Plus abziehen → in **IN** am smartavan-Board.
2. Kabel von **OUT** → zum CP Plus.
3. Genauso das JST-Stromkabel des CP Plus: IN ans Board, OUT zum CP Plus.
4. In `config.h` ist `BOARD_SMARTAVAN` aktiv (Standard): UART2, TX = GPIO16, RX = GPIO17.

Vorher prüfen: Das Board muss an **12–14 V** stabil laufen, nicht nur an USB. Bei 12 V die 3,3 V am ESP messen, auch während WLAN sendet. Läuft es nur an USB, liegt ein Versorgungsproblem auf der Platine vor (Spannungsregler, Lötstelle, Masse).

## Variante B: eigener Aufbau mit ESP32 + LIN-Modul

### Teile

- ESP32-Board (ESP32, ESP32-S3 oder ESP32-C3)
- LIN-Transceiver-Modul mit **TJA1020** oder **TJA1021**. Üblich sind kleine Module mit Klemmen „GND / LIN / INH / 12V“ und „GND / RX / SLP / TX“.
- Step-down-Wandler 12 V → 5 V für den ESP (mind. 500 mA, für WLAN-Spitzen)
- RJ12-Verteiler (6P6C-Y-Adapter) oder RJ12-Kabel für den freien LIN-Anschluss der Heizung

### Anschluss

| LIN-Modul | an | Hinweis |
|---|---|---|
| 12V (VBAT) | Bordnetz +12 V (abgesichert, z. B. 1 A) | Versorgung des LIN-Treibers |
| GND (Busseite) | Fahrzeugmasse / Masse des LIN-Kabels | |
| LIN | LIN-Ader des RJ12 | |
| TX (TXD) | ESP `LIN_TX_PIN` | |
| RX (RXD) | ESP `LIN_RX_PIN` | Open Drain; der interne Pull-up des ESP wird von der Firmware eingeschaltet |
| SLP (NSLP / EN) | 3,3 V **oder** ESP-GPIO als `LIN_CS_PIN` | Muss HIGH sein, sonst schläft der Transceiver |
| GND (Logikseite) | ESP GND | |
| INH | nicht anschließen | |

Der Step-down hängt ebenfalls an +12 V und GND und versorgt den ESP über 5V/VIN. **Alle Massen verbinden**: Fahrzeug, LIN-Modul, Step-down, ESP.

### Standard-Pins der Profile

| Profil | UART | TX | RX | LEDs |
|---|---|---|---|---|
| `BOARD_SMARTAVAN` | 2 | 16 | 17 | – |
| `BOARD_ESP32_GENERIC` | 2 | 17 | 16 | MQTT 4, LIN 15 |
| `BOARD_ESP32S3_GENERIC` | 1 | 17 | 18 | – |
| `BOARD_ESP32C3_GENERIC` | 1 | 21 | 20 | – |

Andere Pins: In `config.h` im jeweiligen Profilblock ändern. Zusätzlich gibt es optional `LIN_CS_PIN` (SLP/EN), `LIN_FAULT_PIN`, `MQTT_LED_PIN`, `LIN_LED_PIN` und `LED_ACTIVE_LOW`.

Nicht verwenden:
- **ESP32:** GPIO 6–11 (Flash), 34–39 nur als Eingang (für RX ok, nicht für TX), Strapping-Pins 0/2/12/15 möglichst nicht für TX.
- **ESP32-S3:** GPIO 19/20 (USB), 26–37 je nach Modul (Flash/PSRAM).
- **ESP32-C3:** GPIO 18/19 (USB), Strapping-Pins 2/8/9 möglichst nicht.

### Wo an den Bus?

- **Freie RJ12-Buchse an der Heizung:** Die Combi hat zwei LIN-Buchsen, eine geht zum CP Plus, die andere ist für Zubehör (dort steckt sonst die iNet Box).
- **Oder** ein RJ12-Y-Adapter zwischen CP Plus und Heizung.

Vom RJ12-Kabel werden nur **LIN** und **GND** gebraucht. Die Aderfarben und die Zählweise der Pins unterscheiden sich je nach Kabel. Deshalb **messen statt raten**, bei eingeschaltetem CP Plus:
- **GND:** 0 V gegen Fahrzeugmasse.
- **LIN:** im Ruhezustand etwa Bordspannung (≈ 12 V). Mit dem Multimeter springt der Wert leicht, sobald das CP Plus sendet.

## USB am Fahrzeug

USB nur zum ersten Flashen verwenden und dabei die **12 V abziehen**. Hängt das Board gleichzeitig an 12 V und am Mac:
- kann Strom über den 5-V-Pfad zurück in den Mac fließen (viele ESP32-Boards haben dort keine Diode),
- wird die Fahrzeugmasse mit der Mac-Masse verbunden – mit Netzteil am Landstrom/Wechselrichter drohen Ausgleichsströme.

Danach Updates per **Weboberfläche (OTA)** und Logs über **Weboberfläche oder MQTT** (siehe README).

## Fallstricke

1. **Master-Widerstand am LIN-Modul:** Manche Module haben einen 1-kΩ-Pull-up mit Diode für den Master-Betrieb, oft per Jumper oder Lötbrücke. Der ESP ist **Slave**, das CP Plus hat den Master-Widerstand schon. Diesen Jumper am Modul **offen lassen**.
2. **SLP/EN nicht angeschlossen:** Beim TJA1020 zieht ein interner Widerstand NSLP nach LOW, dann schläft der Chip. Typisches Symptom: „LIN-Verkehr: nein“.
3. **TX/RX vertauscht:** Häufigster Fehler. Ebenfalls „LIN-Verkehr: nein“.
4. **Masse fehlt:** CRC-Fehler, sporadische Aussetzer oder gar nichts.
5. **Nur ein Gerät als iNet Box:** Keine echte iNet Box parallel und kein iNet-X-Bedienteil.
6. **CP Plus zu alt:** Unter C4.xx lassen sich Werte nur lesen, nicht schalten.
7. **Anmeldung (INIT):** Am CP Plus RESET → PR SET, einmalig. Danach bleibt die Anmeldung, auch wenn der ESP zwischendurch aus ist.
8. **Versorgung:** WLAN zieht Spitzen bis ca. 400 mA. Schwache Wandler führen zu Neustarts (Brownout).

## Erste Prüfung

Weboberfläche → **System**:

| Anzeige | Bedeutung |
|---|---|
| LIN-Verkehr: ja | Verkabelung und Transceiver funktionieren, der ESP sieht den Bus |
| CP Plus angemeldet: ja | INIT war erfolgreich, das CP Plus fragt den ESP ab |
| CP Plus / Heizung mit Versionsnummer | Gerätedaten empfangen, Steuerung möglich |
| Frames ok steigt, CRC-Fehler bleibt (fast) 0 | Saubere Verbindung |

Zusätzlich gibt der serielle Monitor (115200 Baud) die Meldungen aus. Für Details das Log-Level auf „Debug“ stellen.
