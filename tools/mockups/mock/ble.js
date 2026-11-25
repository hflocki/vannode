// ble-node: Bluetooth devices as plugins. The mockup shows every plugin type at once.
// The battery-node mockup (battery.js) builds on this file; battery types live there.
// A real node allows at most 12 devices and 3 Bluetooth connections, so the limits below
// are raised for the mockup only.
const MAC = (n) => 'C0:FF:EE:00:00:' + String(n).padStart(2, '0');
const P = (id, type, type_name, ble, cfg) => ({id, type, type_name, ble, running: true, cfg: Object.assign({enabled: true}, cfg)});
const GAS_FIELDS = [
  {k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
  {k: 'capacity', l: 'Flaschengröße', t: 'float', min: 1, max: 200, step: 0.1, u: 'L', h: 'Bruttovolumen, z. B. 11 kg = 27,2 L'},
  {k: 'fill_stop_on', l: 'Füllstopp vorhanden', t: 'bool', h: 'Tankflaschen mit 80-%-Füllstopp'},
  {k: 'fill_stop', l: 'Füllstopp', t: 'float', min: 10, max: 100, step: 1, u: '%'}];
const MOPEKA_FIELDS = GAS_FIELDS.concat([
  {k: 'empty_mm', l: 'Höhe leer', t: 'int', min: 0, max: 2000, u: 'mm', h: 'Europa 6 kg 38/336, 11 kg 38/366, 14 kg 38/467, US 20 lb 38/254 mm'},
  {k: 'full_mm', l: 'Höhe voll', t: 'int', min: 1, max: 2000, u: 'mm'}]);
const BMS_FIELDS = (l) => [{k: 'mac', l, t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
  {k: 'interval', l: 'Abfrageintervall', t: 'int', min: 5, max: 300, u: 's', h: 'Wie oft die Werte gelesen werden'}];
const GAS_CFG = {capacity: 27.2, fill_stop_on: false, fill_stop: 80};

const M = {
  id: 'ble',
  config: {
    version: '2.6.1', device: 'ble-node', reset_reason: 'Neustart (Software)', ha_node: 'camper_ble',
    core: {wifi_ssid: 'Camper-WLAN', wifi_pass_set: true, ap_ssid: 'BLE-Node-A1B2C3', wifi_tx_power: 70, ap_always: false,
           mqtt_server: '192.168.1.1', mqtt_port: 1883, mqtt_user: '', mqtt_pass_set: false, mqtt_prefix: 'camper/ble',
           web_user: 'admin', web_pass_set: true, node_name: 'ble-node', debug: true},
    plugins: [
      // devices with a Bluetooth connection
      P('rvtech', 'rvtech', 'Dachhaube RV Tech', 2, {name: 'Dachhaube', mac: MAC(1), pin: '', pos_rain: 12, pos_vent: 0, pos_fav: 0}),
      P('pump', 'pump', 'Wasserpumpe CaraWater', 2, {name: 'Wasserpumpe', mac: MAC(2), model: 'ISP 15'}),
      P('fridge', 'fridge', 'Kühlschrank WCS', 2, {name: 'Kühlschrank', mac: MAC(3), pin: '', zone2: true, name1: '', name2: '',
        service: '', notify: '', write: '', poll: '', poll_s: 5, night_auto: true, night_on: '22:00', night_off: '08:00'}),
      P('wipro', 'wipro', 'Alarm WiPro III (nur lesen)', 2, {name: 'Alarmanlage', mac: MAC(4), pin: '',
        names: 'Tür vorn,Tür hinten,Heckklappe,Fenster', service: '', notify: ''}),
      P('obd', 'obd', 'Fahrzeug: OBD2-Adapter (ELM327 / vLinker)', 2, {name: 'Fahrzeug', mac: MAC(5), pin: '', gatt: 0,
        service: '', notify: '', write: '', profile: 1, mode: 0, detect: 1, run_v: 13.2, poll_s: 5, rest_min: 10}),
      P('supersense', 'supersense', 'Tanks: SuperSense Empfängerbox (bis 4 Sensoren)', 2, {name: 'Tanks', mac: MAC(15), medium: 1, tank_size: 100, empty1: 0, full1: 0,
        kind2: 2, size2: 90, empty2: 0, full2: 0, kind3: 3, size3: 80, empty3: 0, full3: 0, kind4: 0, size4: 0, empty4: 0, full4: 0,
        show1: 0, show2: 1, show3: 1, show4: 0, poll_s: 10, drive_s: 300}),
      // passive sensors (advertisements only)
      P('dimes', 'dimes', 'Gas: DIMES / SRG WAVE', 1, Object.assign({name: 'Gasflasche', mac: MAC(9)}, GAS_CFG)),
      P('mopeka', 'mopeka', 'Gas: Mopeka Pro', 1, Object.assign({name: 'Gasflasche 2', mac: MAC(10), empty_mm: 38, full_mm: 366}, GAS_CFG)),
      P('mopeka_std', 'mopeka_std', 'Gas: Mopeka Standard', 1, Object.assign({name: 'Gasflasche 3', mac: MAC(11), empty_mm: 38, full_mm: 336, lpg_propane: 100}, GAS_CFG, {capacity: 14.6})),
      P('senso4s', 'senso4s', 'Gas: Senso4s Waage', 1, Object.assign({name: 'Gaswaage', mac: MAC(16)}, GAS_CFG, {fill_stop_on: false})),
      P('trumalc', 'trumalc', 'Gas: Truma LevelControl', 2, {name: 'Gasflasche Truma', mac: MAC(17), interval: 600}),
      P('govee', 'govee', 'Thermometer Govee', 1, {name: 'Thermometer außen', mac: MAC(12), temp_offset: 0, hum_offset: 0}),
      P('tpms', 'tpms', 'Reifendruck SuperSense TireCheck', 1, {name: 'Reifendruck', mac1: MAC(21), pos1: 0, mac2: MAC(22), pos2: 1, mac3: MAC(23), pos3: 4, mac4: MAC(24), pos4: 5,
        mac5: '', pos5: 4, mac6: '', pos6: 5, mac7: '', pos7: 6, mac8: '', pos8: 7, p_min: 0, p_max: 0,
        va_min: 3.3, va_max: 4.2, ha_min: 4.0, ha_max: 5.2}),
      P('bthome', 'bthome', 'BTHome-Sensor (ATC/pvvx, Shelly BLU …)', 1, {name: 'Thermometer Bad', mac: MAC(13), objs: '01,02,03,0C'}),
      // own BLE sensor: a thermometer with custom firmware (service data 181A)
      P('blesensor', 'blesensor', 'Eigener BLE-Sensor', 1, {name: 'Eigener Sensor', mac: MAC(14), source: 1, company: '', uuid: '181A',
        v1_name: 'Temperatur', v1_pos: 6, v1_len: 1, v1_signed: true, v1_big: false, v1_factor: 0.01, v1_add: 0, v1_dec: 1, v1_unit: '°C', v1_kind: 1,
        v2_name: 'Luftfeuchte', v2_pos: 8, v2_len: 1, v2_signed: false, v2_big: false, v2_factor: 0.01, v2_add: 0, v2_dec: 0, v2_unit: '%', v2_kind: 2,
        v3_name: 'Batterie', v3_pos: 12, v3_len: 0, v3_signed: false, v3_big: false, v3_factor: 1, v3_add: 0, v3_dec: 0, v3_unit: '%', v3_kind: 3})],
    // field descriptions once per type (as the firmware sends them, see fields() of each plugin)
    schemas: {
      rvtech: [{k: 'mac', l: 'Hauben-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
               {k: 'pin', l: 'PIN', t: 'text', max: 7, h: 'Nur Ziffern (1-6, auch 000000). Leer lassen, wenn keine nötig ist. Kein anderes Gerät (Handy) darf gleichzeitig verbunden sein.'},
               {k: 'pos_rain', l: 'Stellung Regen', t: 'int', min: 1, max: 100, u: '%', h: 'Standard 12 %'},
               {k: 'pos_vent', l: 'Stellung Lüften', t: 'int', min: 0, max: 100, u: '%', h: '0 = Lüften-Funktion der Haube (jedes Drücken verlängert die Lüftzeit)'},
               {k: 'pos_fav', l: 'Stellung Favorit', t: 'int', min: 0, max: 100, u: '%', h: '0 = in der Haube gespeicherter Favorit ("Favorit speichern")'}],
      pump: [{k: 'mac', l: 'Pumpen-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
             {k: 'model', l: 'Modell', t: 'text', max: 23, h: 'Nur Anzeige in Home Assistant, z. B. ISP 15'}],
      fridge: [{k: 'mac', l: 'Kühlschrank-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
               {k: 'pin', l: 'App-Passwort', t: 'text', max: 7, h: 'Das Passwort der WCS-App (Werkseinstellung 1234). Leer = 1234. Keine Bluetooth-Kopplung nötig.'},
               {k: 'zone2', l: 'Zweite Zone', t: 'bool', h: 'Kühlschrank mit zweitem Fach / Gefrierfach'},
               {k: 'name1', l: 'Name Zone 1', t: 'text', max: 23, h: 'Leer = Gefrierfach (bei einer Zone: Innenraum). Nur die Anzeige, die MQTT-Topics bleiben zone1 / temp1 …'},
               {k: 'name2', l: 'Name Zone 2', t: 'text', max: 23, h: 'Leer = Kühlfach'},
               {k: 'service', l: 'Service-UUID', t: 'text', max: 39, h: 'Leer = E530 (Standard), * = alle Services'},
               {k: 'notify', l: 'Notify-UUID', t: 'text', max: 39, h: 'Leer = E532 (Standard), * = jede, die senden kann'},
               {k: 'write', l: 'Write-UUID', t: 'text', max: 39, h: 'Leer = E531 (Standard)'},
               {k: 'poll', l: 'Abfragebefehl', t: 'text', max: 39, h: 'Leer = /SC0/1/-01 (Standard). \\r und \\n erlaubt'},
               {k: 'poll_s', l: 'Abfrage alle', t: 'int', min: 2, max: 3600, u: 's', h: 'Standard 5 s'},
               {k: 'night_auto', l: 'Nachtschaltung automatisch', t: 'bool', h: 'Schaltet zur Beginn-Zeit den Nachtmodus ein (die Umluft geht dabei aus) und zur Ende-Zeit wieder aus und die Umluft ein. Nur zu diesen Zeiten, von Hand Geschaltetes bleibt bis zur nächsten Zeit.'},
               {k: 'night_on', l: 'Nachtmodus ab', t: 'text', max: 5, h: 'Uhrzeit HH:MM, Standard 22:00'},
               {k: 'night_off', l: 'Nachtmodus bis', t: 'text', max: 5, h: 'Uhrzeit HH:MM, Standard 08:00 (Umluft startet)'}],
      wipro: [{k: 'mac', l: 'Alarm-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
              {k: 'pin', l: 'Bluetooth-PIN', t: 'text', max: 7, h: 'Leer lassen - das Modul koppelt nicht. Nur ausfüllen, falls es beim Verbinden doch eine Kopplung verlangt.'},
              {k: 'names', l: 'Kontaktnamen', t: 'text', max: 95, h: 'Namen der Kontakte (Bit 0-7), mit Komma getrennt, z. B. Tür vorn,Tür hinten,Fenster. Leer = Kontakt 0, Kontakt 1 ...'},
              {k: 'service', l: 'Service-UUID', t: 'text', max: 39, h: 'Leer = alle Services durchsuchen (Standard)'},
              {k: 'notify', l: 'Status-UUID', t: 'text', max: 39, h: 'Leer = Standard-Characteristic. * = jede, die senden kann'}],
      obd: [{k: 'mac', l: 'Adapter-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name z. B. vLinker, OBDII, OBDBLE)'},
            {k: 'pin', l: 'Bluetooth-PIN', t: 'text', max: 7, h: 'Meist leer. Nur ausfüllen, wenn der Adapter beim Verbinden eine PIN verlangt'},
            {k: 'gatt', l: 'Bluetooth-Kanäle', t: 'select', o: 'Automatisch|FFF0 (FFF1 / FFF2)|FFE0 (FFE1)|18F0 (2AF0 / 2AF1)|Vgate / vLinker (E7810A71)|Eigene UUIDs',
             h: 'Automatisch passt für die meisten Adapter. Im Log steht nach dem Verbinden, welche Kanäle es gibt'},
            {k: 'service', l: 'Service-UUID', t: 'text', max: 39, h: 'Nur bei "Eigene UUIDs"'},
            {k: 'notify', l: 'Notify-UUID', t: 'text', max: 39, h: 'Nur bei "Eigene UUIDs", leer = jede, die senden kann'},
            {k: 'write', l: 'Write-UUID', t: 'text', max: 39, h: 'Nur bei "Eigene UUIDs"'},
            {k: 'profile', l: 'Fahrzeug', t: 'select', o: 'Standard (alle Fahrzeuge)|Fiat Ducato (Euro 6)',
             h: 'Fiat Ducato: festes Protokoll CAN 29 Bit, AdBlue direkt vom zuständigen Steuergerät, Verbrauch aus der Kraftstoffmasse'},
            {k: 'mode', l: 'Abfragen', t: 'select', o: 'Nur bei laufendem Motor|Immer, wenn das Steuergerät antwortet',
             h: '"Nur bei laufendem Motor" schont die Starterbatterie: ohne laufenden Motor fragt der Node das Fahrzeug nicht ab und lässt den Adapter schlafen'},
            {k: 'detect', l: 'Motor erkennen an', t: 'select', o: 'Bordspannung|Drehzahl (Steuergerät jede Minute fragen)',
             h: 'Bordspannung: der Node fragt das Fahrzeug erst, wenn die Lichtmaschine lädt. Fahrzeuge mit intelligenter Lichtmaschine (z. B. Fiat Ducato, Leerlauf nur 12,3-12,6 V) brauchen "Drehzahl"'},
            {k: 'run_v', l: 'Motor läuft ab', t: 'float', min: 12, max: 15, step: 0.1, u: 'V', h: 'Nur bei "Motor erkennen an: Bordspannung": Spannung, ab der der Motor als laufend gilt. Typisch 13,2 V'},
            {k: 'poll_s', l: 'Abfrage alle', t: 'int', min: 1, max: 60, u: 's', h: 'Pause zwischen zwei Abfragerunden aller Werte'},
            {k: 'rest_min', l: 'Ruhepause', t: 'int', min: 0, max: 60, u: 'min', h: 'Motor aus: so lange trennt der Node die Verbindung, damit der Adapter schlafen kann. 0 = verbunden bleiben'}],
      daly: BMS_FIELDS('Batterie-MAC'),
      ective: BMS_FIELDS('Batterie-MAC'),
      jbd: BMS_FIELDS('Batterie-MAC'),
      jk: BMS_FIELDS('Batterie-MAC'),
      litime: BMS_FIELDS('Batterie-MAC'),
      bm2: [{k: 'mac', l: 'BM2-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name "Battery Monitor" / "BM2")'}],
      supersense: [{k: 'mac', l: 'MAC der Box', t: 'mac', h: 'SSBT… – über "Suchen" aus der Liste wählen'},
                   {k: 'medium', l: 'Sensor 1: Tank', t: 'select', o: 'nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges'},
                   {k: 'show1', l: 'Sensor 1: Anzeige', t: 'select', o: 'Inhalt|Freier Platz', h: 'Nur die Kachel in der Live-Ansicht: Inhalt oder noch freier Platz (z. B. beim Abwassertank)'},
                   {k: 'tank_size', l: 'Sensor 1: Tankgröße', t: 'float', min: 0, max: 5000, step: 0.1, u: 'L'},
                   {k: 'empty1', l: 'Sensor 1: Rohwert leer', t: 'int', min: 0, max: 9998, h: 'Leer und voll = 0: Standard (2475 = 100 %). Sonst den Rohwert aus der Live-Ansicht bei leerem bzw. vollem Tank eintragen'},
                   {k: 'full1', l: 'Sensor 1: Rohwert voll', t: 'int', min: 0, max: 9998},
                   {k: 'kind2', l: 'Sensor 2: Tank', t: 'select', o: 'nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges'},
                   {k: 'show2', l: 'Sensor 2: Anzeige', t: 'select', o: 'Inhalt|Freier Platz'},
                   {k: 'size2', l: 'Sensor 2: Tankgröße', t: 'float', min: 0, max: 5000, step: 0.1, u: 'L'},
                   {k: 'empty2', l: 'Sensor 2: Rohwert leer', t: 'int', min: 0, max: 9998},
                   {k: 'full2', l: 'Sensor 2: Rohwert voll', t: 'int', min: 0, max: 9998},
                   {k: 'kind3', l: 'Sensor 3: Tank', t: 'select', o: 'nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges'},
                   {k: 'show3', l: 'Sensor 3: Anzeige', t: 'select', o: 'Inhalt|Freier Platz'},
                   {k: 'size3', l: 'Sensor 3: Tankgröße', t: 'float', min: 0, max: 5000, step: 0.1, u: 'L'},
                   {k: 'empty3', l: 'Sensor 3: Rohwert leer', t: 'int', min: 0, max: 9998},
                   {k: 'full3', l: 'Sensor 3: Rohwert voll', t: 'int', min: 0, max: 9998},
                   {k: 'kind4', l: 'Sensor 4: Tank', t: 'select', o: 'nicht benutzt|Frischwasser|Grauwasser|Schwarzwasser|Gas|Sonstiges'},
                   {k: 'show4', l: 'Sensor 4: Anzeige', t: 'select', o: 'Inhalt|Freier Platz'},
                   {k: 'size4', l: 'Sensor 4: Tankgröße', t: 'float', min: 0, max: 5000, step: 0.1, u: 'L'},
                   {k: 'empty4', l: 'Sensor 4: Rohwert leer', t: 'int', min: 0, max: 9998},
                   {k: 'full4', l: 'Sensor 4: Rohwert voll', t: 'int', min: 0, max: 9998},
                   {k: 'poll_s', l: 'Aktualisieren im Stand alle', t: 'int', min: 1, max: 3600, step: 1, u: 's', h: 'Füllstände werden über diese Zeit gemittelt, dann angezeigt und per MQTT gesendet'},
                   {k: 'drive_s', l: 'Aktualisieren während der Fahrt alle', t: 'int', min: 1, max: 3600, step: 1, u: 's', h: 'Länger als im Stand, weil das Wasser schwappt (Warnungen am Display springen sonst). Fahrt = OBD-Adapter an diesem Node meldet Motor läuft, oder der Abfahrtscheck des Displays'}],
      dimes: GAS_FIELDS,
      senso4s: GAS_FIELDS,
      trumalc: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name beginnt mit "T", z. B. "T…Alugas 11kg")'},
                {k: 'interval', l: 'Messintervall', t: 'int', min: 60, max: 3600, u: 's', h: 'Wie oft gemessen wird. Jede Messung kostet Sensorbatterie, Standard 600 s'}],
      victron: [{k: 'mac', l: 'Geräte-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
                {k: 'key', l: 'Schlüssel', t: 'text', max: 33, h: '32 Zeichen aus der VictronConnect-App: Gerät → Einstellungen (Zahnrad) → ⋮ → Produktinfo → "Instant Readout" einschalten → "Details anzeigen"'}],
      mopeka: MOPEKA_FIELDS,
      mopeka_std: MOPEKA_FIELDS.concat([{k: 'lpg_propane', l: 'Propananteil', t: 'int', min: 0, max: 100, u: '%', h: 'Rest Butan; bestimmt die Schallgeschwindigkeit im Flüssiggas'}]),
      tpms: [{k: 'mac1', l: 'Rad 1: Sensor-MAC', t: 'mac', h: 'Nicht zugeordnete Sensoren stehen in der Live-Ansicht, MAC von dort übernehmen. Leer = Rad nicht belegt'},
             {k: 'pos1', l: 'Rad 1: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac2', l: 'Rad 2: Sensor-MAC', t: 'mac'},
             {k: 'pos2', l: 'Rad 2: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac3', l: 'Rad 3: Sensor-MAC', t: 'mac'},
             {k: 'pos3', l: 'Rad 3: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac4', l: 'Rad 4: Sensor-MAC', t: 'mac'},
             {k: 'pos4', l: 'Rad 4: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac5', l: 'Rad 5: Sensor-MAC', t: 'mac'},
             {k: 'pos5', l: 'Rad 5: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac6', l: 'Rad 6: Sensor-MAC', t: 'mac'},
             {k: 'pos6', l: 'Rad 6: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac7', l: 'Rad 7: Sensor-MAC', t: 'mac'},
             {k: 'pos7', l: 'Rad 7: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'mac8', l: 'Rad 8: Sensor-MAC', t: 'mac'},
             {k: 'pos8', l: 'Rad 8: Position', t: 'select', o: 'Vorne links|Vorne rechts|Mitte links|Mitte rechts|Hinten links|Hinten rechts|Hinten links innen|Hinten rechts innen|Anhänger vorne links|Anhänger vorne rechts|Anhänger hinten links|Anhänger hinten rechts|Reserverad'},
             {k: 'p_min', l: 'Standard: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar', h: 'Gilt für alle Achsen ohne eigene Grenze, 0 = keine Warnung'},
             {k: 'p_max', l: 'Standard: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar', h: '0 = keine Warnung'},
             {k: 'va_min', l: 'Vorderachse: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar', h: '0 = Standard von oben'},
             {k: 'va_max', l: 'Vorderachse: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ma_min', l: 'Mittelachse: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ma_max', l: 'Mittelachse: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ha_min', l: 'Hinterachse: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ha_max', l: 'Hinterachse: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'av_min', l: 'Anhänger vorne: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'av_max', l: 'Anhänger vorne: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ah_min', l: 'Anhänger hinten: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'ah_max', l: 'Anhänger hinten: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'rr_min', l: 'Reserverad: Warnung unter', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'},
             {k: 'rr_max', l: 'Reserverad: Warnung über', t: 'float', min: 0, max: 15, step: 0.05, u: 'bar'}],
      govee: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name GVH5…)'},
              {k: 'temp_offset', l: 'Korrektur Temperatur', t: 'float', min: -10, max: 10, step: 0.1, u: '°C', h: 'Wird zum Messwert addiert, z. B. -0,5'},
              {k: 'hum_offset', l: 'Korrektur Luftfeuchte', t: 'float', min: -20, max: 20, step: 0.5, u: '%'}],
      shelly: [{k: 'mac', l: 'Shelly-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name beginnt mit "Shelly")'},
               {k: 'channel', l: 'Kanal', t: 'int', min: 0, max: 3, h: '0 bei Geräten mit einem Ausgang, beim 2PM 0 oder 1'},
               {k: 'interval', l: 'Abfrage alle', t: 'int', min: 5, max: 600, u: 's', h: 'Leistung und Zustand werden so oft abgefragt'}],
      bthome: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (passt: Dienst FCD2)'},
               {k: 'key', l: 'Schlüssel (nur verschlüsselt)', t: 'text', max: 32, h: '32 Zeichen (Hex), z. B. aus der Shelly-App bei aktivierter Verschlüsselung. Leer = unverschlüsselt'},
               {k: 'objs', l: 'Erkannte Werte', t: 'text', max: 24, h: 'Füllt sich beim Empfang von selbst. Leeren und speichern, um die Werte neu zu erkennen'}],
      blesensor: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
                  {k: 'source', l: 'Datenquelle', t: 'select', o: 'Herstellerdaten|Service-Daten'},
                  {k: 'company', l: 'Hersteller-ID', t: 'text', max: 7, h: 'Nur Herstellerdaten: z. B. FFFF, leer = beliebig'},
                  {k: 'uuid', l: 'Service-UUID', t: 'text', max: 39, h: 'Nur Service-Daten: z. B. 181A, leer = die erste'},
                  ...[1, 2, 3].flatMap(n => [
                    {k: `v${n}_name`, l: `Wert ${n}: Name`, t: 'text', max: 23, h: 'Leer = Wert nicht benutzt'},
                    {k: `v${n}_pos`, l: `Wert ${n}: Stelle`, t: 'int', min: 0, max: 63, step: 1, h: 'Position im Paket, 0 = erstes Byte (siehe Rohdaten)'},
                    {k: `v${n}_len`, l: `Wert ${n}: Länge`, t: 'select', o: '1 Byte|2 Byte|4 Byte'},
                    {k: `v${n}_signed`, l: `Wert ${n}: mit Vorzeichen`, t: 'bool'},
                    {k: `v${n}_big`, l: `Wert ${n}: höchstes Byte zuerst`, t: 'bool'},
                    {k: `v${n}_factor`, l: `Wert ${n}: Faktor`, t: 'float', min: -1000000, max: 1000000, step: 0.000001},
                    {k: `v${n}_add`, l: `Wert ${n}: Offset`, t: 'float', min: -1000000, max: 1000000, step: 0.000001},
                    {k: `v${n}_dec`, l: `Wert ${n}: Nachkommastellen`, t: 'int', min: 0, max: 4, step: 1},
                    {k: `v${n}_unit`, l: `Wert ${n}: Einheit`, t: 'text', max: 9},
                    {k: `v${n}_kind`, l: `Wert ${n}: Art`, t: 'select', o: 'Allgemein|Temperatur|Luftfeuchte|Batterie|Spannung|Strom|Leistung|Druck'}])]
    },
    types: [{type: 'rvtech', name: 'Dachhaube RV Tech', ble: 2}, {type: 'pump', name: 'Wasserpumpe CaraWater', ble: 2},
            {type: 'fridge', name: 'Kühlschrank WCS', ble: 2}, {type: 'wipro', name: 'Alarm WiPro III (nur lesen)', ble: 2},
            {type: 'obd', name: 'Fahrzeug: OBD2-Adapter (ELM327 / vLinker)', ble: 2},
            {type: 'supersense', name: 'Tanks: SuperSense Empfängerbox (bis 4 Sensoren)', ble: 2},
            {type: 'dimes', name: 'Gas: DIMES / SRG WAVE', ble: 1}, {type: 'mopeka', name: 'Gas: Mopeka Pro', ble: 1}, {type: 'mopeka_std', name: 'Gas: Mopeka Standard', ble: 1},
            {type: 'senso4s', name: 'Gas: Senso4s Waage', ble: 1}, {type: 'trumalc', name: 'Gas: Truma LevelControl', ble: 2},

            {type: 'govee', name: 'Thermometer Govee', ble: 1}, {type: 'tpms', name: 'Reifendruck SuperSense TireCheck', ble: 1}, {type: 'bthome', name: 'BTHome-Sensor (ATC/pvvx, Shelly BLU …)', ble: 1}, {type: 'shelly', name: 'Schalter: Shelly Gen2+ per Bluetooth (Plus, Pro, Gen3, Gen4)', ble: 2}, {type: 'blesensor', name: 'Eigener BLE-Sensor', ble: 1}],
    // mockup only: room for one device of every type (real node: 12 devices, 3 connections)
    max_devices: 16,
    max_links: 8
  },
  // roof hatch: position moves 5 % per update towards the target
  pos: 0, target: -1,
  // per device: "Bluetooth für App freigeben"
  released: {},
  // fridge: zones bit mask (1 = freezer, 2 = fridge), set points, fan, sleep mode, main power
  fr: {zones: 3, set1: -18, set2: 4, fan: true, night: false, power: true, q1: false, q2: false, door: false},
  pump: {enable: true, level: 1, running: false},
  obdScan: '',
  // device changes are saved at once and take effect with the restart (restart_needed)
  keep(plugins) {
    M.save({plugins, restart_needed: plugins.some(p => p.pending || p.removed || p.cfg.enabled !== p.running)});
  },
  route(path, method, body) {
    if (path === '/api/config' && method === 'POST') {
      const c = M.cfg;
      if (body.core) M.save({core: Object.assign({}, c.core, body.core, {wifi_pass: undefined, mqtt_pass: undefined, web_pass: undefined})});
      else if (body.plugin) {
        const p = c.plugins.find(x => x.id === body.plugin);
        if (p) { Object.assign(p.cfg, body.cfg); M.save({plugins: c.plugins}); }
      } else if (body.enabled) {
        c.plugins.forEach(p => { if (p.id in body.enabled && !p.removed) p.cfg.enabled = body.enabled[p.id]; });
        M.keep(c.plugins);
      } else if (body.remove) {
        // pending devices disappear right away, running ones after the restart
        const p = c.plugins.find(x => x.id === body.remove);
        M.keep(p && p.pending ? c.plugins.filter(x => x !== p) : c.plugins.map(x => x === p ? Object.assign({}, x, {removed: true}) : x));
      } else if (body.restore) {
        M.keep(c.plugins.map(x => x.id === body.restore ? Object.assign({}, x, {removed: undefined}) : x));
      } else if (body.add) {
        const t = c.types.find(x => x.type === body.add);
        let n = 1;
        while (c.plugins.some(p => p.id === (n === 1 ? t.type : t.type + n))) n++;
        const id = n === 1 ? t.type : t.type + n;
        M.keep(c.plugins.concat([{id, type: t.type, type_name: t.name, ble: t.ble, running: false, pending: true,
                                  cfg: {enabled: true, name: t.name + (n > 1 ? ' ' + n : ''), mac: ''}}]));
      }
      return {status: 'ok'};
    }
    if (path === '/api/restart' && method === 'POST') {
      // the restart applies all saved device changes
      const plugins = M.cfg.plugins.filter(p => !p.removed)
        .map(p => Object.assign({}, p, {pending: undefined, running: p.cfg.enabled}));
      M.keep(plugins);
      return {status: 'restarting'};
    }
    if (path === '/api/cmd') {
      const on = body.v === 'true' || body.v === 'ON' || body.v === '1';
      const k = body.k === 'preset' || body.k === 'action' ? body.v : body.k;
      const dev = M.cfg.plugins.find(p => p.id === body.p) || {};
      if (k === 'release') { M.released[dev.id || body.p] = on; return {status: 'ok'}; }
      const POS = {auf: 100, zu: 0, regen: 12, lueften: 102};
      if (dev.type === 'rvtech' || (!dev.type && k in POS)) {
        if (k in POS) M.target = POS[k];
        else if (k === 'cover_position') M.target = +body.v;
      } else if (dev.type === 'pump') {
        if (k === 'enable') M.pump.enable = on;
        else if (k === 'pressure_level') M.pump.level = +body.v + 1;
      } else if (dev.type === 'obd') {
        if (k === 'scan') { M.obdScan = 'läuft …'; setTimeout(() => { M.obdScan = 'fertig: 14 Werte gefunden'; }, 4000); }
      } else {
        const F = M.fr;
        if (k === 'zone1') F.zones = on ? F.zones | 1 : F.zones & ~1;
        else if (k === 'zone2') F.zones = on ? F.zones | 2 : F.zones & ~2;
        else if (k === 'fan') F.fan = on;
        else if (k === 'night') { F.night = on; if (on) F.fan = false; }
        else if (k === 'power') F.power = on;
        else if (k === 'quick1') F.q1 = on;
        else if (k === 'quick2') F.q2 = on;
        else if (k === 'doorheat') F.door = on;
        else if (k === 'target1') F.set1 = +body.v;
        else if (k === 'target2') F.set2 = +body.v;
      }
      return {status: 'ok'};
    }
    if (path === '/api/log') {
      const t = Math.floor((Date.now() - M.t0) / 1000);
      return '# Beispiel-Log (Mockup)\n# Time stamps below: uptime h:mm:ss.mmm\n\n0:00:05.120 State: Verbinde\n0:00:05.880 Connected, MTU 247, 1 subscription(s)\n' +
        '0:00:06.010 Notify 0xFFE1: 5A 01 64 00 00\n0:00:' + String(6 + t % 50).padStart(2, '0') + '.400 Values updated\n';
    }
    if (path === '/api/log/clear') return { ok: true };
    if (path === '/api/scan') {
      return {active: true, devices: [
        {mac: MAC(1), name: 'RVT-Skylight', rssi: -71, mfg: '', uuid: 'ffe0', age: 2, fits: ['rvtech'], use: 'Dachhaube'},
        {mac: MAC(21), name: '', rssi: -77, mfg: '', uuid: 'faf0', age: 9, fits: ['tpms'], use: 'Reifendruck: Vorne links'},
        {mac: MAC(22), name: '', rssi: -81, mfg: '', uuid: 'faf0', age: 14, fits: ['tpms'], use: 'Reifendruck: Vorne rechts'},
        {mac: MAC(2), name: 'ISP-Pump', rssi: -74, mfg: '', uuid: '', age: 3, fits: ['pump']},
        {mac: MAC(5), name: 'vLinker MC-IOS', rssi: -80, mfg: '', uuid: 'e7810a71', age: 4, fits: ['obd']},
        {mac: MAC(6), name: 'DL-C8478C000003', rssi: -68, mfg: '0102', uuid: '0xfff0', age: 1, fits: ['daly']},
        {mac: MAC(9), name: '', rssi: -79, mfg: '', uuid: '', age: 6, fits: ['dimes']},
        {mac: MAC(13), name: 'ATC_ABCDEF', rssi: -76, mfg: '', uuid: '0xfcd2', age: 3, fits: ['bthome']},
        {mac: MAC(12), name: 'GVH5075_1A2B', rssi: -83, mfg: '88EC', uuid: '', age: 5, fits: ['govee']}]};
    }
  },
  // live data of one device, built like live() in the plugin of that type
  live(p, t) {
    const rel = !!M.released[p.id], sw = {t: 'switch', k: 'release', l: 'Bluetooth für App freigeben', v: rel};
    const wave = (base, amp, per) => Math.round((base + Math.sin(t / per) * amp) * 100) / 100;
    switch (p.type) {
      case 'rvtech': {
        if (M.target >= 0) {
          const goal = Math.min(M.target, 100);
          if (M.pos < goal) M.pos = Math.min(goal, M.pos + 5);
          else if (M.pos > goal) M.pos = Math.max(goal, M.pos - 5);
          if (M.pos === goal) { if (M.target === 102) M.pos = 102; M.target = -1; }
        }
        const shown = Math.min(M.pos, 100);
        const label = M.pos === 102 ? 'Lüften' : M.target >= 0 ? (M.target > M.pos ? 'Öffnet...' : 'Schließt...') : M.pos === 0 ? 'Geschlossen' : 'Offen';
        return {state: rel ? 'Für App freigegeben' : 'Verbunden', ok: !rel, gauge: {v: shown, unit: '%', label},
          rows: [['Gemeldet', M.pos, ''], ['Ziel', M.target >= 0 ? M.target : null, ''], ['Bluetooth', -71, 'dBm']],
          ctrl: [{t: 'buttons', k: 'preset', b: [['auf', 'Auf'], ['zu', 'Zu'], ['regen', 'Regen'], ['lueften', 'Lüften']]},
                 {t: 'slider', k: 'cover_position', l: 'Position', min: 0, max: 100, v: shown}, sw]};
      }
      case 'pump': {
        const run = M.pump.enable && Math.floor(t / 20) % 3 === 0;
        return {state: rel ? 'Für App freigegeben' : 'Verbunden', ok: !rel,
          gauge: {v: run ? wave(2.2, 0.15, 3) : 2.6, unit: 'bar', label: run ? 'Druck - läuft' : 'Druck', max: 4},
          rows: [['Durchfluss', run ? wave(5.8, 0.4, 4) : 0, 'L/min'], ['Spannung', 12.8, 'V'], ['Strom', run ? 4.6 : 0.1, 'A'],
                 ['Wassertemperatur', 14.2, '°C'], ['Heute', 37, 'L'], ['Gesamt', 1842, 'L'], ['Bluetooth', -74, 'dBm']],
          ctrl: [{t: 'switch', k: 'enable', l: 'Pumpenfreigabe', v: M.pump.enable},
                 {t: 'select', k: 'pressure_level', l: 'Konstantdruck', o: 'Stufe 1|Stufe 2|Stufe 3', v: M.pump.level - 1}, sw]};
      }
      case 'fridge': {
        const F = M.fr, z1 = F.power && (F.zones & 1), z2 = F.power && (F.zones & 2);
        return {state: 'Verbunden', ok: true,
          zones: [{l: p.cfg.name1 || 'Gefrierfach', v: z1 ? -17 : 2, unit: '°C', s: `Soll ${F.set1} °C`, k: 'zone1', on: !!(F.zones & 1),
                   step: {k: 'target1', v: F.set1, min: -22, max: 10, d: 1}},
                  {l: p.cfg.name2 || 'Kühlfach', v: z2 ? 5 : 9, unit: '°C', s: `Soll ${F.set2} °C`, k: 'zone2', on: !!(F.zones & 2),
                   step: {k: 'target2', v: F.set2, min: -22, max: 10, d: 1}}],
          rows: [['Spannung', 12.9, 'V'], ['Batterie', 96, '%'], ['Modus', F.night ? 'Nachtmodus' : 'Normal', ''], ['Nachtschaltung', '22:00 – 08:00', ''],
                 ['Grenzen laut Gerät', '-22…-10 / -10…10 °C', ''], ['Bluetooth', -66, 'dBm'], ['Nachrichten', 412, '']],
          ctrl: [{t: 'switch', k: 'power', l: 'Kühlschrank an', v: F.power, top: true},
                 {t: 'switch', k: 'fan', l: 'Umluft', v: F.fan},
                 {t: 'switch', k: 'night', l: 'Nachtmodus', v: F.night},
                 {t: 'switch', k: 'quick1', l: 'Superfrost (Gefrierfach)', v: F.q1},
                 {t: 'switch', k: 'quick2', l: 'Superkühlung (Kühlfach)', v: F.q2},
                 {t: 'switch', k: 'doorheat', l: 'Türheizung', v: F.door},
                 {t: 'select', k: 'bpd', l: 'Batterieschutz', o: 'Stufe 1|Stufe 2|Stufe 3', v: 1}, sw],
          raw: '/SC0/2,1,1,-17,5,-18,4,2,129,22,96,3,1,0,0,-10,-22,10,-10'};
      }
      case 'wipro': {
        const names = (p.cfg.names || '').split(',');
        const open = Math.floor(t / 30) % 2 === 1;   // the rear door opens now and then
        const rows = [['Scharf', 'Nein', ''], ['Alarmgrund', 'Kein Alarm', ''], ['Offen', open ? (names[1] || 'Kontakt 1') : 'nichts', '']];
        for (let i = 0; i < 8; i++) rows.push([(names[i] || '').trim() || 'Kontakt ' + i, i === 1 && open ? 'offen' : 'zu', '']);
        rows.push(['Bluetooth', -78, 'dBm']);
        return {state: rel ? 'Für App freigegeben' : 'Verbunden', ok: !rel, rows, ctrl: [sw],
                raw: open ? '01 00 00 00 00 00 02 00' : '01 00 00 00 00 00 00 00'};
      }
      case 'obd': {
        const rows = [['Motor', 'läuft', ''], ['Bordspannung', wave(14.1, 0.05, 7), 'V'], ['Drehzahl', Math.round(wave(1650, 250, 6)), 'U/min'],
          ['Geschwindigkeit', Math.round(wave(82, 6, 9)), 'km/h'], ['Kühlwasser', 88, '°C'], ['Tankfüllstand', 63, '%'],
          ['Verbrauch', wave(7.9, 1.2, 5), 'l/h'], ['AdBlue', 71, '%'], ['Motorlast', Math.round(wave(38, 8, 5)), '%'],
          ['Außentemperatur', 6, '°C'], ['Kilometerstand', 84213.4, 'km'],
          ['Motorkontrollleuchte', 'aus', ''], ['Fehlercodes', 'keine', ''],
          ['Protokoll', 'ISO 15765-4 CAN (29 bit, 500 kbaud)', ''], ['Adapter', 'ELM327 v2.3', ''], ['Bluetooth', -80, 'dBm']];
        if (M.obdScan) rows.push(['Fahrzeug-Scan', M.obdScan, '']);
        return {state: 'Verbunden', ok: true, rows,
                ctrl: [{t: 'buttons', k: 'action', b: [['read_dtc', 'Fehlercodes lesen'], ['scan', 'Fahrzeug scannen']]}, sw],
                raw: '41 0C 19 C8'};
      }
      case 'daly': case 'ective': case 'bm2': case 'jbd': case 'jk': case 'litime': {
        const B = {daly: [78, 13.31, -4.2, 78, 1038, 18.6, 14.5, 41], ective: [92, 13.38, 1.6, 92, 1224, null, 15.1, 12],
                   bm2: [86, 12.62, null, null, null, null, null, null],
                   jbd: [72, 13.28, -5.8, 72, 956, 12.4, 19.9, 37], jk: [68, 26.51, -7.1, 142, 3776, 20.1, 29.2, 21],
                   litime: [65, 13.27, -1.4, 68.9, 914, 49.2, 23, 3]}[p.type];
        const rows = [['Spannung', B[1], 'V']];
        if (B[2] !== null) rows.push(['Strom', B[2], 'A'], ['Leistung', Math.round(B[1] * B[2]), 'W']);
        if (B[3] !== null) rows.push(['Restkapazität', B[3], 'Ah'], ['Energie', B[4], 'Wh']);
        if (B[5] !== null) rows.push(['Restlaufzeit', B[5], 'h']);
        if (B[6] !== null) rows.push(['Temperatur', B[6], '°C']);
        if (['daly', 'jbd', 'jk', 'litime'].includes(p.type)) rows.push(['Zellen', '4 × 3.326 … 3.331 V', ''], ['Zelldifferenz', 5, 'mV']);
        if (B[7] !== null) rows.push(['Zyklen', B[7], '']);
        rows.push(['Zustand', B[2] === null ? 'Ruhe' : B[2] < 0 ? 'Entladen' : 'Laden', ''], ['Alter', 3, 's'], ['Bluetooth', -68, 'dBm']);
        return {state: rel ? 'Für App freigegeben' : 'OK', ok: !rel, gauge: {v: B[0], unit: '%', label: 'Ladestand', max: 100}, rows, ctrl: [sw]};
      }
      case 'trumalc':
        return {state: 'OK', ok: true, gauge: {v: 58, unit: '%', label: 'Füllstand'},
                rows: [['Gasmenge', 6.4, 'kg'], ['Batterie', 87, '%'], ['Temperatur', 9, '°C'], ['Messqualität', 3, '/ 3'], ['Bluetooth', -74, 'dBm'], ['Alter', 212, 's']],
                ctrl: [{t: 'buttons', k: 'action', b: [['measure', 'Jetzt messen']]}, sw], raw: '3A 40 00 57 09 00 C0'};
      case 'victron':   // battery-node (mock/battery.js): SmartShunt
        return {state: 'OK', ok: true, gauge: {v: 81, unit: '%', label: 'Ladestand', max: 100},
                rows: [['Spannung', 13.21, 'V'], ['Strom', -4.6, 'A'], ['Leistung', -61, 'W'], ['Entnommen', -38.0, 'Ah'],
                       ['Restlaufzeit', 21.3, 'h'], ['Alter', 2, 's'], ['Bluetooth', -71, 'dBm']]};
      case 'dimes': case 'mopeka': case 'mopeka_std': case 'senso4s': {
        const lvl = {dimes: 64.5, mopeka: 41.2, mopeka_std: 87.0, senso4s: 52.0}[p.type], cap = +p.cfg.capacity || 27.2;
        const rows = [['Inhalt', Math.round(cap * lvl / 10) / 10, 'L'], ['Rohwert', lvl, '%']];
        if (p.type.startsWith('mopeka')) rows.push(['Füllhöhe', Math.round(38 + (366 - 38) * lvl / 100), 'mm'], ['Temperatur', 7, '°C'], ['Messqualität', 3, '/ 3']);
        rows.push(['Batterie', p.type === 'dimes' ? 90 : 75, '%'], ['Bluetooth', -79, 'dBm'], ['Alter', 8, 's']);
        return {state: 'OK', ok: true, gauge: {v: lvl, unit: '%', label: 'Füllstand'}, rows,
                raw: p.type === 'dimes' ? '02 01 06 11 FF 4C 52 00 0A 5D 03' : '59 00 03 60 2F 4A 81 AB CD EF 00 00'};
      }
      case 'supersense': {
        // Box with 4 channels: raw 2475 = 100 %, 9999 = no sensor; tiles like the device sends them
        const c = p.cfg, raw = [Math.round(wave(1820, 6, 11)), 640, 300, 9999];
        const KIND = ['Sensor', 'Frischwasser', 'Grauwasser', 'Schwarzwasser', 'Gas', 'Tank'];
        const kinds = [c.medium, c.kind2, c.kind3, c.kind4].map(Number), sizes = [c.tank_size, c.size2, c.size3, c.size4].map(Number);
        const shows = [c.show1, c.show2, c.show3, c.show4].map(Number), temps = [9.3, 10.2, 10.8];
        const tanks = raw.map((r, i) => {
          const t = {l: kinds[i] ? KIND[kinds[i]] : 'Sensor ' + (i + 1)};
          if (r !== 9999) t.r = r;
          if (!kinds[i] || r === 9999) return Object.assign(t, {off: true, s: !kinds[i] ? (r !== 9999 ? 'nicht zugeordnet' : 'nicht benutzt') : 'kein Sensor'});
          const pct = Math.min(100, Math.round(r / 24.75)), l = pct * sizes[i] / 100, free = shows[i] === 1;
          return Object.assign(t, {p: pct, v: free ? 100 - pct : pct, u: '%', free: free || undefined,
            s: (free ? 'frei ' + (sizes[i] - l).toFixed(1) : l.toFixed(1)) + ' L · ' + temps[i] + ' °C'});
        });
        return {state: rel ? 'Für App freigegeben' : 'OK', ok: !rel, tanks,
          rows: [['Aktualisierung', 'alle ' + (c.poll_s || 10) + ' s (Stand)', ''], ['Bluetooth', -76, 'dBm']],
          ctrl: [sw], raw: String(raw[0]).padStart(4, '0').replace(/(..)(..)/, '$1 $2') + ' 06 40 03 00 99 99'};
      }
      case 'tpms': {
        const p = [3.62, 3.21, 4.41, Math.round(wave(4.35, 0.03, 40) * 100) / 100];
        const pos = ['Vorne links', 'Vorne rechts', 'Hinten links', 'Hinten rechts'];
        return {state: 'Vorne rechts: 3.21 bar zu niedrig', ok: false,
          rows: pos.map((n, i) => [n, p[i].toFixed(2) + ' bar · ' + (9 + i) + ' °C · ' + [100, 100, 62, 100][i] + ' % · ' + (20 + 7 * i) + ' s' + (i === 1 ? ' · ⚠' : ''), '']).concat(
                [[MAC(31), '2.10 bar · -88 dBm · nicht zugeordnet', '']])};
      }
      case 'govee':
        return {state: 'OK', ok: true,
          rows: [['Temperatur', 4.3, '°C'], ['Luftfeuchte', 71, '%'], ['Batterie', 100, '%'], ['Modell', 'H5072/H5075', ''], ['Bluetooth', -83, 'dBm'], ['Alter', 12, 's']]};
      case 'bthome':
        return {state: 'OK', ok: true,
          rows: [['Batterie', 87, '%'], ['Temperatur', 19.8, '°C'], ['Luftfeuchte', 54.2, '%'], ['Spannung', 2.91, 'V'], ['Bluetooth', -76, 'dBm'], ['Alter', 6, 's']],
          raw: '40 00 A5 01 57 02 BC 07 03 2C 15 0C 5E 0B'};
      case 'shelly':
        return {state: 'An', ok: true, gauge: {v: Math.round(wave(42, 3, 30)), unit: 'W', label: 'Leistung - an', max: 2000},
          rows: [['Spannung', 12.6, 'V'], ['Strom', 3.33, 'A'], ['Energie', 1.27, 'kWh'], ['Gerätetemperatur', 31, '°C'], ['Bluetooth', -71, 'dBm'], ['Gerät', 'Plus1PM', '']],
          ctrl: [{t: 'switch', k: 'output', l: 'Schalter', v: true}]};
      case 'blesensor':
        return {state: 'OK', ok: true,
          rows: [['Temperatur', Math.round(wave(21.4, 0.3, 60) * 10) / 10, '°C'], ['Luftfeuchte', 58, '%'], ['Batterie', 87, '%'], ['Bluetooth', -77, 'dBm'], ['Alter', 4, 's']],
          raw: '0: 56 34 12 38  4: C1 A4 5C 08  8: A8 16 1E 0B  12: 57 C2 0F'};
    }
    return {state: 'Warte auf Daten', ok: false, rows: []};
  },
  state() {
    const t = (Date.now() - M.t0) / 1000;
    // live data: only what runs in this boot (pending devices start with the restart)
    const runs = p => p.running && !p.removed;
    const plugins = M.cfg.plugins.filter(p => !p.pending && !p.removed).map(p => {
      const o = {id: p.id, name: p.cfg.name, type: p.type, enabled: runs(p)};
      if (!o.enabled) return o;
      if (!p.cfg.mac) return Object.assign(o, {state: 'Keine MAC eingestellt', ok: false, rows: []});
      return Object.assign(o, M.live(p, t));
    });
    return {rssi: -58, uptime: M.up(), heap: 98000, wifi_drops: 0, wifi_reason: '', wifi: 'Verbunden', mqtt: true,
            ble: {adv: 1200 + Math.floor((Date.now() - M.t0) / 100), scanning: true}, plugins};
  }
};
