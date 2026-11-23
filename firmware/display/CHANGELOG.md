# Änderungen – display-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `display-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 2.23.1 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. Startseite, Verlauf, Automationen-Status, unbekannte Sensoren und die WebSocket-Verbindung (nimmt Befehle an) waren bisher ohne Anmeldung erreichbar und sind jetzt geschützt. Neu: USB-Befehl `noauth` setzt auf den Werks-Login zurück. |
| 2.23.0 | **Gleiche System-Karten auf allen Geräten:** Firmware- & Web-Update, Einstellungen sichern, Ereignisse und Gerät (Neustart, Werkseinstellungen) kommen jetzt aus einem gemeinsamen Baustein (`firmware/_common/web/vannode-system.js`) und sehen überall gleich aus: Dateiauswahl als Feld mit Dateiname, Fortschrittsbalken, Prüfung der Datei wie bisher (gleiche Regeln). Sicherung: *Sichern* und *Einspielen* nebeneinander, *Einspielen* erst nach der Dateiwahl.  Beim Display stehen sie im neuen Abschnitt *System → Update & Sicherung* statt im langen System-Abschnitt; die Sicherung enthält weiter Seiten, Kacheln, Automationen und Sensoren, ältere Sicherungsdateien gehen weiter. Die Ereignisse bleiben auf der Systemseite. |
| 2.22.0 | **Topic-Finder mit Discovery:** Knopf *Discovery* sucht Geräte, die sich für Home Assistant anmelden (ESPHome, Tasmota, Shelly, zigbee2mqtt …, Topic `homeassistant/…/config`), und legt mit einem Klick eine fertige Kachel an: Wert mit Einheit, Zustand mit deutschem Text (offen/zu, Bewegung …) oder Schalter mit den Befehlen des Geräts – ohne Home Assistant. VanNode-Geräte sind ausgeblendet. Sonst keine Änderung |
| 2.21.5 | BLE-Node heißt wieder **ble** (Firmware ble 2.5.1): Standard-Topic `camper/ble`, gespeicherte `camper/tool`, `camper/tool2`, `camper/tool3` (und `camper/toolbox`) werden beim Start auf `camper/ble`, `camper/ble2`, `camper/ble3` umgestellt. Einstellungen und Geräteliste: „BLE-Node“, „BLE-Node 2/3“ und „Battery-Node“ statt „BLE-Node Tool …“ / „BLE-Node Battery“ |
| 2.21.4 | Über VanNode: keine eigene Liste fremder Teile mehr, nur noch der Link auf die zentrale NOTICE.md (nach Node sortiert) |
| 2.21.3 | Geräteliste: der Road Buddy heißt jetzt Pocket-Node (`camper/pocket`); ältere Geräte unter `camper/roadbuddy` werden weiter als „Road Buddy“ angezeigt |

Frühere Versionen sind hier nicht aufgeführt; ihre Änderungen stehen in der Git-Historie.
