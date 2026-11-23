# Änderungen – maxxfan-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `maxxfan-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 1.11.0 | **Ersatz-Broker:** neues Feld *Ersatz-Broker* in den MQTT-Einstellungen (`IP` oder `IP:Port`, leer = aus). Ist der Broker 2 Minuten nicht erreichbar, verbindet sich das Gerät mit dem Ersatz-Broker (z. B. dem Display mit eingeschaltetem Broker), prüft alle 5 Minuten, ob der Broker wieder da ist, und wechselt dann zurück. Wechsel stehen bei den Ereignissen, solange der Ersatz-Broker benutzt wird, zeigt die Karte *Ereignisse* einen Hinweis; `/api/system` meldet `mqttfb`. |
| 1.10.0 | **Webseite auf Englisch:** `#define UI_LANG "en"` in `config_local.h` (vor dem Flashen) zeigt die Seite auf Englisch, auch die Texte, die das Gerät schickt; ohne Eintrag bleibt alles Deutsch (die Release-.bin ist Deutsch). Lizenz- und Haftungstext der Über-Karte bleiben im deutschen Original. `/api/system` meldet die Sprache als `fw.lang`. |
| 1.9.7 | Gemeinsame System-Karten aktualisiert (Uhrzeit-Karte für Geräte mit Uhr, hier ohne Wirkung). |
| 1.9.6 | **Werks-Login jetzt admin / admin** (statt `vannode2026`). Ein selbst gesetztes Passwort bleibt beim Update unverändert; nur Geräte ohne Login oder noch mit dem Werks-Passwort `vannode2026` bekommen admin / admin. |
| 1.9.5 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. Vergessen: mit Erase All Flash neu flashen. |
| 1.9.4 | **Gleiche System-Karten auf allen Geräten:** Firmware- & Web-Update, Einstellungen sichern, Ereignisse und Gerät (Neustart, Werkseinstellungen) kommen jetzt aus einem gemeinsamen Baustein (`firmware/_common/web/vannode-system.js`) und sehen überall gleich aus: Dateiauswahl als Feld mit Dateiname, Fortschrittsbalken, Prüfung der Datei wie bisher (gleiche Regeln). Sicherung: *Sichern* und *Einspielen* nebeneinander, *Einspielen* erst nach der Dateiwahl. |
| 1.9.3 | Neue Karte **Einstellungen sichern** unter dem Firmware-Update (gemeinsamer Baustein `firmware/_common/web/vannode-backup.js`): alle Einstellungen als Datei sichern und wieder einspielen. Passwörter und die Anmeldung sind nicht enthalten und bleiben beim Einspielen erhalten; WLAN, IP, Gerätename und MQTT-Server nur mit Häkchen. Läuft nur in der Web-Seite über die vorhandene `/api/config`, die Firmware selbst ist unverändert |
| 1.9.2 | Über-Karte kennt jetzt auch den Pocket-Node (gemeinsame Karte auf allen Geräte-Webseiten); keine sichtbare Änderung |
| 1.9.1 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 1.9.0 | Optionaler Haubenkontakt: Reedkontakt an einem freien GPIO (in der Web-UI wählbar, Standard aus) meldet `…/lid` = `open`/`closed`, Anzeige im Reiter Live, HA-Fenster-Sensor „Lid“; Eintrag im Ereignis-Log, wenn die Haube 90 s nach „Schließen“ noch offen ist |
| 1.8.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 1.8.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 1.8.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 1.8.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 1.8.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 1.8.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.8.0 | Neue Namen: Standard-Hostname `maxxfan-node` (ein gespeicherter Hostname bleibt), Setup-WLAN, Gerätename und Release-Datei `maxxfan-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.7.1 | Update-Prüfung vergleicht auch den Chip in der Kennung (gleich wie im Browser); ein Update über den Setup-AP löst keinen WLAN-Rückfall mehr aus |
| 1.7.0 | Sichere Updates: Firmware-Kennung, Prüfung der Datei im Browser und im Gerät (falsches Gerät, falscher Chip, .merged.bin, zu groß), Fehlergrund im Klartext, installierte Version und Ereignis-Log im Web-UI |
| 1.6.0 | Absicherung gegen Aussperren: Rückfall auf die vorherige Firmware, wenn ein Update kein WLAN bekommt; geänderte WLAN-Daten werden zurückgesetzt, wenn sie nicht funktionieren |
| 1.5.0 | Stufe setzen wieder wie in 1.3.0 (von unten), Stufe 0 schließt die Haube (Web-UI, MQTT, HA-Regler) |
| 1.4.0 | „Schließen“ (Web-UI, MQTT `cmd/p<n>/close`, HA-Taste); Stufe setzen von oben (10× Schneller), damit die Haube dabei nicht zugeht; Web-Oberfläche und LittleFS-Image per Web-UI oder OTA hochladbar (kein USB nötig) |
| 1.3.0 | Web-Oberfläche aus LittleFS (`data/index.html.gz`) wie bei tanklevel und temp, Notseite mit Firmware-Update, falls das Image fehlt |
| 1.2.0 | Web-Oberfläche im gleichen Stil wie die anderen Camper-Geräte (Statuspunkte, Reiter Live / MaxxFan / MQTT / WiFi) |
| 1.1.0 | Stufe 1–10 setzen (MQTT, HA-Regler, Web-UI) |
| 1.0.0 | Erste Arduino/MQTT-Version, ersetzt die ESPHome-Konfiguration |
