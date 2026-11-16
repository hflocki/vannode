<!-- SPDX-License-Identifier: GPL-3.0-only -->
# Arduino IDE – gemeinsame Einrichtung

Gilt für alle VanNode-Geräte. Was nur ein Gerät betrifft (Board, zusätzliche Bibliotheken, Besonderheiten), steht im Abschnitt **Arduino IDE** seiner README. Die Versionen hier sind die, mit denen GitHub Actions baut (`.github/workflows/build.yml`); `tools/docs/check_arduino.py` prüft, dass beide übereinstimmen.

Fertige `.bin`-Dateien gibt es unter [Releases](https://github.com/hflocki/vannode/releases) – selbst bauen ist nur nötig, wenn du etwas ändern willst.

## Arduino IDE und Board-Pakete

- **Arduino IDE 2.x** (für das LittleFS-Plugin mindestens 2.2.1).
- **Board-Paket esp32** von Espressif Systems, **3.3.x** (CI: 3.3.2). Eintrag unter *Einstellungen → Zusätzliche Boardverwalter-URLs*: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- **Board-Paket esp8266** von ESP8266 Community, **3.1.x** (CI: 3.1.2) – nur für temp-node auf ESP8266. URL: `https://arduino.esp8266.com/stable/package_esp8266com_index.json`

## Bibliotheken

Über den Bibliotheksverwalter installieren. Die Spalte *Quelle* ist das GitHub-Repo, aus dem die CI die Version holt.

**Alle Geräte**

| Bibliothek | Autor | Version | Quelle |
| --- | --- | --- | --- |
| PubSubClient | Nick O'Leary | 2.8 | `knolleary/pubsubclient` |
| ArduinoJson | Benoit Blanchon | 7.4.2 | `bblanchon/ArduinoJson` |
| ESP Async WebServer | ESP32Async | 3.12.1 | `ESP32Async/ESPAsyncWebServer` |
| Async TCP (ESP32) | ESP32Async | 3.5.0 | `ESP32Async/AsyncTCP` |

**Zusätzlich je Gerät**

| Bibliothek | Autor | Version | Quelle | Gerät |
| --- | --- | --- | --- | --- |
| NimBLE-Arduino | h2zero | 2.5.1 | `h2zero/NimBLE-Arduino` | ble, tanklevel |
| OneWire | Paul Stoffregen | 2.3.8 | `PaulStoffregen/OneWire` | temp |
| DallasTemperature | Miles Burton | 4.0.6 | `milesburton/Arduino-Temperature-Control-Library` | temp |
| ESP Async TCP (ESP8266) | ESP32Async | 2.0.0 | `ESP32Async/ESPAsyncTCP` | temp auf ESP8266 |
| SparkFun BNO08x Cortex Based IMU | SparkFun | 1.0.6 | `sparkfun/SparkFun_BNO08x_Arduino_Library` | level |
| GFX Library for Arduino | moononournation | 1.6.0 | `moononournation/Arduino_GFX` | display |
| lvgl | LVGL | 9.2.2 | `lvgl/lvgl` | display |
| M5GFX | M5Stack | 0.2.34 | `m5stack/M5GFX` | pocket |
| M5Unified | M5Stack | 0.2.27 | `m5stack/M5Unified` | pocket |

Typische falsche Versionen: AsyncWebServer/AsyncTCP von *me-no-dev* statt *ESP32Async*, NimBLE-Arduino 1.x statt 2.x, ArduinoJson 6.x statt 7.x, Board-Paket esp32 2.x statt 3.x (siehe *Probleme beim Hochladen*).

## Board-Einstellungen

Das Display (ESP32-S3 mit 16 MB und PSRAM) hat eigene Werte, siehe [Display-README](../firmware/display/README.md#arduino-ide).

| Einstellung | ESP32-C3 Super Mini | ESP32-S3 (DevKitC-1 o. Ä.) | ESP32 DevKit (WROOM-32) | M5Stack AtomS3 Lite | M5Stack Core2 | ESP8266 (NodeMCU, D1 mini) |
| --- | --- | --- | --- | --- | --- | --- |
| Board | ESP32C3 Dev Module | ESP32S3 Dev Module | ESP32 Dev Module | M5AtomS3 | M5Core2 (unter *esp32*, nicht unter „M5Stack“) | NodeMCU 1.0 (ESP-12E Module) bzw. LOLIN(WEMOS) D1 R2 & mini |
| USB CDC On Boot | **Enabled** | **Enabled** | – | **Enabled** | – | – |
| Partition Scheme / Flash Size | **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** | **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** | **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** | **8M with spiffs (3MB APP/1.5MB SPIFFS)** | Default (2 x 6.5 MB app, 3.6 MB SPIFFS) – hat OTA, nichts ändern | **4MB (FS:1MB OTA:~1019KB)** |
| PSRAM | – | aus (falsche Einstellung führt zu Abstürzen) | – | – | – | – |
| Anschluss | USB-C | Buchse „USB“ (nicht „UART“), dort kommt auch das serielle Log | USB | USB-C | USB-C | USB |
| Besonderheit | Port wechselt nach dem Upload (Neustart), neu wählen | – | – | Wird er beim Hochladen nicht gefunden: Knopf am Gehäuse beim Einstecken ca. 2 s halten (Download-Modus) | – | – |

Immer ein Schema **mit OTA** wählen: WLAN und Bluetooth brauchen zusammen gut 1,4 MB und passen nicht in die Standard-Partition, und ohne OTA-Partition schlägt das Update über die Web-Oberfläche mit „Partition Could Not be Found“ fehl.

Die Release-Dateien heißen `<gerät>-node-<board>-v<version>.bin` (z. B. `ble-node-esp32c3-v2.1.21.bin`). Beim Update über die Web-Oberfläche die Datei für **dein** Board nehmen – eine AtomS3-Datei läuft nicht auf einem anderen S3-Board.

## Eigene Einstellungen

Passwörter, WLAN und Broker kommen in `config_local.h` im Sketch-Ordner (Vorlage `config_local.h.example` kopieren). Die Datei geht nicht nach GitHub. Details in der [Haupt-README](../README.de.md#eigene-einstellungen-config_localh). Das gilt auch für die Pocket-Node (ab 1.5.8).

## Erstes Flashen (USB)

1. `config_local.h` anlegen (siehe oben).
2. Den Sketch öffnen (`<ordner>/<ordner>.ino`). Der Ordner muss genauso heißen wie die `.ino`, sonst will die IDE den Sketch verschieben.
3. Board-Einstellungen prüfen, Port wählen, hochladen.
4. **Web-Oberfläche** einmalig auf das Gerät bringen. Je nach Gerät auf eine von zwei Arten (steht in der README):
   - **Upload-Formular:** `http://<ip>/` öffnen (ohne WLAN: Setup-WLAN des Geräts, `http://192.168.4.1`). Solange die Seite fehlt, steht dort ein Formular → `data/index.html.gz` hochladen.
   - **LittleFS-Image:** Serial Monitor schließen, dann `Cmd+Shift+P` (Windows/Linux `Strg+Shift+P`) → **Upload LittleFS to Pico/ESP8266/ESP32**. Braucht das Plugin unten. Das Image ersetzt das ganze Dateisystem – liegen die Einstellungen dort (temp, tanklevel), sind sie danach weg.
5. Serieller Monitor: 115200 Baud.

## LittleFS-Plugin installieren (einmalig)

Die Arduino IDE 2.x kann von sich aus kein Dateisystem hochladen. Dafür braucht es [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) (IDE 2.2.1 oder neuer, nicht für IDE 1.8).

1. Unter [Releases](https://github.com/earlephilhower/arduino-littlefs-upload/releases) die Datei `arduino-littlefs-upload-x.y.z.vsix` laden. **Nicht entpacken.**
2. In den Plugin-Ordner legen (vorher anlegen, falls er fehlt):

   | System | Ordner |
   | --- | --- |
   | macOS / Linux | `~/.arduinoIDE/plugins/` |
   | Windows | `C:\Users\<Benutzername>\.arduinoIDE\plugins\` |

   ```bash
   mkdir -p ~/.arduinoIDE/plugins
   mv ~/Downloads/arduino-littlefs-upload-*.vsix ~/.arduinoIDE/plugins/
   ```
   Der Ordner beginnt mit einem Punkt und ist im Finder versteckt (`Cmd+Shift+.` blendet ihn ein).
3. Arduino IDE **komplett beenden** und neu starten.

## Updates ohne USB

- **Web-Oberfläche des Geräts** (Tab System bzw. WiFi → Firmware-Update): Release-`.bin` oder selbst gebaute Datei (*Sketch → Kompilierte Binärdatei exportieren*) hochladen. `data/index.html.gz` ersetzt dort nur die Web-Oberfläche.
- **Netzwerk-Port der IDE** (Arduino-OTA, `<gerät>-node` unter *Werkzeuge → Port*), Passwort `DEFAULT_OTA_PASSWORD`.

Ändert sich nur die Firmware, muss die Web-Oberfläche nicht neu hochgeladen werden.

## Web-Oberfläche ändern

Quelle ist `web/index.html` im Geräteordner, auf das Gerät kommt die gepackte `data/index.html.gz`. Nach einer Änderung im Repo-Hauptordner:

```
python3 tools/web/pack_web.py <gerät>      # z. B. ble, temp, tanklevel
```

Ohne das Repo-Werkzeug (im Sketch-Ordner): `gzip -9 -n -c web/index.html > data/index.html.gz`. Die GitHub Action prüft bei jedem Push, ob die gepackte Seite zur `index.html` passt. Display und Heizung betten ihre Seite stattdessen in die Firmware ein (`embed_web.py`, siehe deren README).

## Probleme beim Hochladen

**Firmware**

| Problem | Lösung |
| --- | --- |
| `Failed to connect to ESP32…` / `No serial data received` | Flash-Modus: **BOOT** gedrückt halten, kurz **RESET**, BOOT loslassen, nochmal hochladen. AtomS3: Knopf beim Einstecken halten. |
| Kein Port in der IDE | Anderes USB-Kabel (viele laden nur). C3 und S3 brauchen keinen Treiber (Mac: `/dev/cu.usbmodem…`). |
| Port verschwindet nach dem Upload | Normal bei C3/S3 mit USB CDC: der ESP startet neu. Port neu wählen. |
| `Sketch too big` / `text section exceeds available space` | Partition Scheme steht auf Default → Schema mit OTA wählen (siehe Tabelle). |
| Update über die Web-Oberfläche: „Partition Could Not be Found“ | Gerät wurde mit einem Schema ohne OTA geflasht. Einmal per USB mit dem richtigen Schema neu flashen. |
| Keine Ausgabe im Serial Monitor | *USB CDC On Boot* auf **Enabled**, 115200 Baud, neu hochladen. |
| Kompilierfehler in `AsyncWebServer` / `AsyncTCP` | Version von **ESP32Async** nötig, die alte von me-no-dev entfernen. |
| Kompilierfehler mit `NimBLE`, z. B. `setScanCallbacks` | NimBLE-Arduino 1.x installiert, nötig ist 2.x. |
| Kompilierfehler mit `JsonDocument` | ArduinoJson 6.x installiert, nötig ist 7.x. |
| Viele Fehler in WiFi-/ESP-Funktionen | Board-Paket esp32 2.x installiert, nötig ist 3.x. |
| `'ESP_RST_USB' was not declared in this scope` | Board aus einem anderen Board-Paket gewählt (z. B. „M5Stack“ von M5Stack) oder esp32 älter als 3.x. Board unter *esp32* wählen, esp32 von Espressif 3.3.x installieren. |
| Fehler rund um `LittleFS` | Eine zusätzlich installierte LittleFS-Bibliothek in `~/Documents/Arduino/libraries` löschen – das Board-Paket bringt LittleFS mit. |
| „Multiple libraries were found for WiFi.h“ | Nur eine Warnung; eine zusätzliche `WiFi`-Bibliothek in `~/Documents/Arduino/libraries` kann weg. |

**LittleFS-Upload**

| Problem | Lösung |
| --- | --- |
| Befehl „Upload LittleFS…“ fehlt | Plugin im falschen Ordner, entpackt, oder IDE nicht komplett neu gestartet. IDE 1.8 wird nicht unterstützt. |
| `Could not open <port>, the port doesn't exist` | Serial Monitor noch offen, oder der Port hat sich nach dem Sketch-Upload geändert. |
| Upload bricht ab / falsches Board | Bekannter IDE-Fehler beim zuerst geöffneten Sketch: Board einmal umstellen und zurück, dann erneut. |
| Seite zeigt nur „Please upload the LittleFS image“ | LittleFS fehlt, oder das Partition Scheme wurde danach geändert. Sketch und LittleFS mit demselben Schema neu hochladen. |
| Alte Web-Oberfläche nach Änderung | `web/index.html` geändert, aber nicht neu gepackt (siehe oben). |

**Ohne Plugin:** Im Plugin gibt es auch **Build LittleFS image in sketch directory**, das erzeugt `mklittlefs.bin`. Die Datei lässt sich mit [esptool](https://github.com/espressif/esptool) direkt schreiben (`pip install esptool`, am besten in einer venv):

```bash
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX write-flash 0x3D0000 mklittlefs.bin
```

`0x3D0000` ist die Startadresse des Dateisystems im Partition Scheme *Minimal SPIFFS* (ESP32-C3/S3/ESP32 mit 4 MB).

**Update per WLAN (OTA)**

| Problem | Lösung |
| --- | --- |
| Netzwerk-Port `<gerät>-node` erscheint nicht | Rechner und ESP müssen im selben Netz sein. mDNS wird manchmal von Routern oder Gastnetzen blockiert. Unter macOS beim ersten Mal die Firewall-Abfrage für die Arduino IDE erlauben. |
| Passwortabfrage schlägt fehl | Es gilt `DEFAULT_OTA_PASSWORD` der Firmware, die **gerade auf dem ESP läuft**. |
| LittleFS per WLAN | Geht mit dem Arduino-Plugin nicht. Die Web-Oberfläche (`data/index.html.gz`) lässt sich aber über die Web-Oberfläche des Geräts (Firmware- & Web-Update) hochladen. |
