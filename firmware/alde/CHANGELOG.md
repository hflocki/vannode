# Änderungen – alde-node

Neueste Version oben. Der Abschnitt einer Version erscheint beim Tag `alde-v<version>` automatisch als Text im GitHub-Release.

| Version | Änderungen |
|---|---|
| 0.4.1 | Web-Seite: Titel und Kopfzeile „Alde Node“ statt „Example“ aus der Vorlage, Mithör-Tabelle mit lesbaren Spaltenköpfen. Klickbares Mockup unter `docs/mockups/alde.html` |
| 0.4.0 | Neuer Modus „Nur mithören (Diagnose)“ am Bedienteil-Bus: Der ESP sendet nichts und zeigt je LIN-ID, welche Frames andere Geräte (Bedienteil, iNet-Box) schicken – Tabelle im Reiter Live, `/api/sniff`, neue IDs im Ereignis-Log. Damit lässt sich klären, wer auf der gelben Buchse Master ist |
| 0.3.1 | README: Pegelwandler je nach LIN-Modul (Messung an RX), Verdrahtungsbeispiel mit Pegelwandler und Pins 16/17/25, passende `config_local.h`-Zeilen. Keine Änderung an der Funktion |
| 0.3.0 | Alde 3030 und 3030 Plus neben der Compact 3020 (Modellname in HA nach gewählter Heizung). Heizbus abschaltbar: Ein Transceiver am Bedienteil-Bus reicht zum Steuern, am ESP32-C3 dann auch mit Steuerung. Befehle: Anmeldung direkt vor jedem Befehl, nichts senden, wenn der Wert schon stimmt, kein Befehl, solange das Bedienteil beschäftigt ist oder eine Störung meldet. Neue Einstellung Heizstab-Höchststufe (Werkseinstellung 1 kW). Unbekannter Warmwasser-Modus wird nicht mehr als „an“ gemeldet. Standard-Pins für ESP32-S3 und -C3, README mit Verdrahtung |
| 0.2.0 | Optionaler Bedienteil-Bus (CI-Bus, zweiter LIN-Transceiver, ESP als Master): Sollwert, Außentemperatur, Gas, Heizstab-Stufe, Warmwasser, Pumpe, Störung lesen und – nur im Modus „Lesen und steuern“ – setzen, per MQTT, Web-UI und Home Assistant. Ab Werk aus |
| 0.1.0 | Erste Version aus der VanNode-Vorlage: passiver LIN-Leser für die Alde Compact 3020 HE (Dekodierung aus alde-3020he-lin), MQTT, HA-Discovery, Web-UI, Host-Test |
