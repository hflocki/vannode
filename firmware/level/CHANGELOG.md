# Änderungen – level-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `level-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
|---|---|
| 2.4.14 | **Behoben: Seite blieb auf iPhone/iPad (Safari) nach dem Login leer bzw. ohne Live-Werte.** Safari schickt die Anmeldung beim WebSocket nicht mit. Die Seite holt sich jetzt nach dem Login einen Schlüssel (`/api/wstoken`, gemeinsamer Baustein `wsauth.h`) und öffnet den WebSocket damit; andere Browser gehen wie bisher. |
| 2.4.13 | Gemeinsame System-Karten aktualisiert (Uhrzeit-Karte für Geräte mit Uhr, hier ohne Wirkung). |
| 2.4.12 | **Werks-Login jetzt admin / admin** (statt `vannode2026`). Ein selbst gesetztes Passwort bleibt beim Update unverändert; nur Geräte ohne Login oder noch mit dem Werks-Passwort `vannode2026` bekommen admin / admin. |
| 2.4.11 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. USB-Befehl `noauth` setzt auf den Werks-Login zurück (statt ihn zu löschen). |
| 2.4.10 | **Gleiche System-Karten auf allen Geräten:** Firmware- & Web-Update, Einstellungen sichern, Ereignisse und Gerät (Neustart) kommen jetzt aus einem gemeinsamen Baustein (`firmware/_common/web/vannode-system.js`) und sehen überall gleich aus: Dateiauswahl als Feld mit Dateiname, Fortschrittsbalken, Prüfung der Datei wie bisher (gleiche Regeln). Sicherung: *Sichern* und *Einspielen* nebeneinander, *Einspielen* erst nach der Dateiwahl. |
| 2.4.9 | Neue Karte **Einstellungen sichern** unter dem Firmware-Update (gemeinsamer Baustein `firmware/_common/web/vannode-backup.js`): alle Einstellungen als Datei sichern und wieder einspielen. Passwörter und die Anmeldung sind nicht enthalten und bleiben beim Einspielen erhalten; WLAN, IP, Gerätename und MQTT-Server nur mit Häkchen. Läuft nur in der Web-Seite über die vorhandene `/api/config`, die Firmware selbst ist unverändert |
| 2.4.8 | Über-Karte kennt jetzt auch den Pocket-Node (gemeinsame Karte auf allen Geräte-Webseiten); keine sichtbare Änderung |
| 2.4.7 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 2.4.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 2.4.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 2.4.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 2.4.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 2.4.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 2.4.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 2.4.0 | Neue Namen: Hostname `level-node`, Setup-WLAN, Gerätename und Release-Datei `level-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 2.3.1 | Update-Prüfung vergleicht auch den Chip in der Kennung (gleich wie im Browser); ein Update über den Setup-AP löst keinen WLAN-Rückfall mehr aus |
| 2.3.0 | Sichere Updates: Firmware-Kennung, Prüfung der Datei im Browser und im Gerät (falsches Gerät, falscher Chip, .merged.bin, zu groß), Fehlergrund im Klartext, installierte Version und Ereignis-Log im Web-UI |
| 2.2.0 | Absicherung gegen Aussperren: Rückfall auf die vorherige Firmware, wenn ein Update kein WLAN bekommt; geänderte WLAN-Daten werden zurückgesetzt, wenn sie nicht funktionieren |
| 2.1.2 | Standard-Pins SDA GPIO2 / SCL GPIO1, damit die Release-.bin ohne eigene `config_local.h` den Sensor findet |
| 2.1.0 | Firmware (.bin) und Web-Oberfläche (index.html.gz) im Web-UI unter WiFi hochladbar, ohne USB; Arduino-OTA-Dateisystem-Upload hängt LittleFS vorher aus |
| 2.0.0 | Fork als camper-level: nur Wohnmobil mit vier Keilen, korrigierte Keilberechnung, Sicherheitslücken geschlossen, Filter funktioniert, Topics und Oberfläche wie die anderen VanNode-Geräte |
| 1.x | Original [WoWa-Level](https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT) von klehnst |
