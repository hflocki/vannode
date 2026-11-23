# Änderungen – example-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `template-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
|---|---|
| 1.4.0 | **Ersatz-Broker:** neues Feld *Ersatz-Broker* in den MQTT-Einstellungen (`IP` oder `IP:Port`, leer = aus). Ist der Broker 2 Minuten nicht erreichbar, verbindet sich das Gerät mit dem Ersatz-Broker (z. B. dem Display mit eingeschaltetem Broker), prüft alle 5 Minuten, ob der Broker wieder da ist, und wechselt dann zurück. Wechsel stehen bei den Ereignissen, solange der Ersatz-Broker benutzt wird, zeigt die Karte *Ereignisse* einen Hinweis; `/api/system` meldet `mqttfb`. |
| 1.3.0 | **Webseite auf Englisch:** `#define UI_LANG "en"` in `config_local.h` (vor dem Flashen) zeigt die Seite auf Englisch, auch die Texte, die das Gerät schickt; ohne Eintrag bleibt alles Deutsch (die Release-.bin ist Deutsch). Lizenz- und Haftungstext der Über-Karte bleiben im deutschen Original. `/api/system` meldet die Sprache als `fw.lang`. |
| 1.2.14 | **Behoben: Seite blieb auf iPhone/iPad (Safari) nach dem Login leer bzw. ohne Live-Werte.** Safari schickt die Anmeldung beim WebSocket nicht mit. Die Seite holt sich jetzt nach dem Login einen Schlüssel (`/api/wstoken`, gemeinsamer Baustein `wsauth.h`) und öffnet den WebSocket damit; andere Browser gehen wie bisher. |
| 1.2.13 | Gemeinsame System-Karten aktualisiert (Uhrzeit-Karte für Geräte mit Uhr, hier ohne Wirkung). |
| 1.2.12 | **Werks-Login jetzt admin / admin** (statt `vannode2026`). Ein selbst gesetztes Passwort bleibt beim Update unverändert; nur Geräte ohne Login oder noch mit dem Werks-Passwort `vannode2026` bekommen admin / admin. |
| 1.2.11 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. USB-Befehl `noauth` setzt auf den Werks-Login zurück (statt ihn zu löschen). |
| 1.2.10 | **Gleiche System-Karten auf allen Geräten:** Firmware- & Web-Update, Einstellungen sichern, Ereignisse und Gerät (Neustart) kommen jetzt aus einem gemeinsamen Baustein (`firmware/_common/web/vannode-system.js`) und sehen überall gleich aus: Dateiauswahl als Feld mit Dateiname, Fortschrittsbalken, Prüfung der Datei wie bisher (gleiche Regeln). Sicherung: *Sichern* und *Einspielen* nebeneinander, *Einspielen* erst nach der Dateiwahl. |
| 1.2.9 | Neue Karte **Einstellungen sichern** unter dem Firmware-Update (gemeinsamer Baustein `firmware/_common/web/vannode-backup.js`): alle Einstellungen als Datei sichern und wieder einspielen. Passwörter und die Anmeldung sind nicht enthalten und bleiben beim Einspielen erhalten; WLAN, IP, Gerätename und MQTT-Server nur mit Häkchen. Läuft nur in der Web-Seite über die vorhandene `/api/config`, die Firmware selbst ist unverändert |
| 1.2.8 | Über-Karte kennt jetzt auch den Pocket-Node (gemeinsame Karte auf allen Geräte-Webseiten); keine sichtbare Änderung |
| 1.2.7 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 1.2.6 | Haftungsausschluss: Über-Karte zeigt „Ohne Gewähr – Nutzung auf eigene Gefahr“ und den Markenhinweis; Lizenzköpfe (SPDX) in allen Quelldateien |
| 1.2.5 | Lizenz jetzt GNU GPL v3 mit Zusatzbedingungen (Urhebervermerk bleibt, Forks kennzeichnen, Name/Logo nicht lizenziert); Über-Karte nennt Lizenz und fremde Teile |
| 1.2.4 | Webseite: Karte **Über VanNode** unter dem Update (Logo, Version, Copyright, Lizenz, Link zum Projekt) |
| 1.2.3 | Meldet in `<prefix>/info` die Hardware (`hw`, z. B. `esp32c3`, `atoms3`), das Display zeigt sie in der Geräteliste. Die Update-Seite nennt die passende Release-Datei (`…-node-<hw>-v….bin`), auch in der Fehlermeldung bei falschem Chip |
| 1.2.2 | Gewollte Neustarts (Einstellungen speichern, Neustart-Knopf, WLAN-Rückfall) zählen in der Update-Probezeit nicht mehr als Absturz – vorher konnten drei davon ein frisches Update auf die alte Version zurückrollen |
| 1.2.1 | VanNode-Symbol links neben der Überschrift der Webseite und als Favicon |
| 1.2.0 | Neue Namen: Hostname `example-node`, Setup-WLAN, Gerätename und Release-Datei `example-node-….bin`. Topics `camper/…` und Home-Assistant-IDs bleiben |
| 1.1.0 | Sicheres Update (Kennung, Prüfung im Browser und im Gerät, Fehler im Klartext), Ereignis-Log, Rückfall auf die vorherige Firmware und WLAN-Änderung mit Rückweg |
| 1.0.0 | Erste Version aus der VanNode-Vorlage |
