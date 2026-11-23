# Änderungen – heater-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `heater-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
| --- | --- |
| 1.8.7 | **Werks-Login jetzt admin / admin** (statt `vannode2026`). Ein selbst gesetztes Passwort bleibt beim Update unverändert; nur Geräte ohne Login oder noch mit dem Werks-Passwort `vannode2026` bekommen admin / admin. |
| 1.8.6 | **Web-Login immer an:** ab Werk Benutzer `admin`, Passwort `vannode2026`. Geräte, die bisher ohne Login liefen, bekommen ihn mit diesem Update; leere Felder behalten den gespeicherten Wert, abschalten lässt sich die Anmeldung nicht mehr. Der Schalter *Web-Login entfernen* heißt jetzt *Web-Login auf Werks-Login zurücksetzen*. |
| 1.8.5 | **Gleiche System-Karten auf allen Geräten:** Firmware- & Web-Update, Einstellungen sichern, Ereignisse und Gerät (Neustart) kommen jetzt aus einem gemeinsamen Baustein (`firmware/_common/web/vannode-system.js`) und sehen überall gleich aus: Dateiauswahl als Feld mit Dateiname, Fortschrittsbalken, Prüfung der Datei wie bisher (gleiche Regeln). Sicherung: *Sichern* und *Einspielen* nebeneinander, *Einspielen* erst nach der Dateiwahl.  Neustart zusätzlich weiter unter WLAN. |
| 1.8.4 | Neue Karte **Einstellungen sichern** unter dem Firmware-Update (gemeinsamer Baustein `firmware/_common/web/vannode-backup.js`): alle Einstellungen als Datei sichern und wieder einspielen. Passwörter und die Anmeldung sind nicht enthalten und bleiben beim Einspielen erhalten; WLAN, IP, Gerätename und MQTT-Server nur mit Häkchen. Läuft nur in der Web-Seite über die vorhandene `/api/config` (Seite steckt in der Firmware, `web_ui.h`) |
| 1.8.3 | Über-Karte kennt jetzt auch den Pocket-Node (gemeinsame Karte auf allen Geräte-Webseiten); keine sichtbare Änderung |
| 1.8.2 | Über-Karte: keine eigene Liste fremder Teile mehr, „Hinweise und fremde Teile“ verlinkt die zentrale NOTICE.md des Projekts |
| 1.8.1 | LIN-Sniffer mit Modus **Alle Frames (Mitschnitt)**: jeder Frame aller PIDs mit Zeitstempel, ungefiltert (Debug-Reiter oder MQTT `set/sniffer` = `ALL`). Verlorene Frames werden gemeldet, das Log-Fenster hält im Mitschnitt bis zu 5000 Zeilen. `tests/log2replay.py` wandelt solche Logs in Replay-Dateien um |
| 1.8.0 | Neuer Reiter **Debug**, einschaltbar unter System → Debug: LIN-Bus-Status, Sniffer, Rohdaten und das Log-Fenster sind dorthin umgezogen. „Herunterladen“ speichert das Log als `.log`-Datei mit Version, Einstellungen (ohne Zugangsdaten) und LIN-Status |

Frühere Versionen sind hier nicht aufgeführt; ihre Änderungen stehen in der Git-Historie.
