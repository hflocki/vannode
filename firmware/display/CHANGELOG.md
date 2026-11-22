# Änderungen – display-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `display-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 2.22.0 | **Topic-Finder mit Discovery:** Knopf *Discovery* sucht Geräte, die sich für Home Assistant anmelden (ESPHome, Tasmota, Shelly, zigbee2mqtt …, Topic `homeassistant/…/config`), und legt mit einem Klick eine fertige Kachel an: Wert mit Einheit, Zustand mit deutschem Text (offen/zu, Bewegung …) oder Schalter mit den Befehlen des Geräts – ohne Home Assistant. VanNode-Geräte sind ausgeblendet. Sonst keine Änderung |
| 2.21.5 | BLE-Node heißt wieder **ble** (Firmware ble 2.5.1): Standard-Topic `camper/ble`, gespeicherte `camper/tool`, `camper/tool2`, `camper/tool3` (und `camper/toolbox`) werden beim Start auf `camper/ble`, `camper/ble2`, `camper/ble3` umgestellt. Einstellungen und Geräteliste: „BLE-Node“, „BLE-Node 2/3“ und „Battery-Node“ statt „BLE-Node Tool …“ / „BLE-Node Battery“ |
| 2.21.4 | Über VanNode: keine eigene Liste fremder Teile mehr, nur noch der Link auf die zentrale NOTICE.md (nach Node sortiert) |
| 2.21.3 | Geräteliste: der Road Buddy heißt jetzt Pocket-Node (`camper/pocket`); ältere Geräte unter `camper/roadbuddy` werden weiter als „Road Buddy“ angezeigt |

Frühere Versionen sind hier nicht aufgeführt; ihre Änderungen stehen in der Git-Historie.
