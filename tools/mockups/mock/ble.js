// ble-node: Bluetooth devices as plugins (here: an RV Tech roof hatch and an own BLE sensor)
const M = {
  id: 'ble',
  config: {
    version: '1.5.4', device: 'BLE Node', reset_reason: 'Neustart (Software)', ha_node: 'camper_ble',
    core: {wifi_ssid: 'Camper-WLAN', wifi_pass_set: true, ap_ssid: 'BLE-Node-A1B2C3', wifi_tx_power: 70, ap_always: false,
           mqtt_server: '192.168.1.1', mqtt_port: 1883, mqtt_user: '', mqtt_pass_set: false, mqtt_prefix: 'camper/ble',
           web_user: '', web_pass_set: false},
    plugins: [{id: 'rvtech', type: 'rvtech', type_name: 'RV Tech Dachhaube', ble: 2, running: true,
               cfg: {enabled: true, name: 'Dachhaube', mac: 'F0:F8:F2:00:00:01', pin: ''}},
              // own BLE sensor: a Xiaomi thermometer with custom firmware (service data 181A)
              {id: 'blesensor', type: 'blesensor', running: true, type_name: 'Eigener BLE-Sensor', ble: 1,
               cfg: {enabled: true, name: 'Thermometer Bad', mac: 'A4:C1:38:12:34:56', source: 1, company: '', uuid: '181A',
                     v1_name: 'Temperatur', v1_pos: 6, v1_len: 1, v1_signed: true, v1_big: false, v1_factor: 0.01, v1_add: 0, v1_dec: 1, v1_unit: '°C', v1_kind: 1,
                     v2_name: 'Luftfeuchte', v2_pos: 8, v2_len: 1, v2_signed: false, v2_big: false, v2_factor: 0.01, v2_add: 0, v2_dec: 0, v2_unit: '%', v2_kind: 2,
                     v3_name: 'Batterie', v3_pos: 12, v3_len: 0, v3_signed: false, v3_big: false, v3_factor: 1, v3_add: 0, v3_dec: 0, v3_unit: '%', v3_kind: 3}}],
    // field descriptions once per type (as the firmware sends them since 1.5.2)
    schemas: {
      rvtech: [{k: 'mac', l: 'Hauben-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
                        {k: 'pin', l: 'PIN', t: 'text', max: 7, h: 'Nur Ziffern (1-6, auch 000000). Leer lassen, wenn keine nötig ist.'}],
      govee: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (Name GVH5…)'},
              {k: 'temp_offset', l: 'Korrektur Temperatur', t: 'float', min: -10, max: 10, step: 0.1, u: '°C', h: 'Wird zum Messwert addiert, z. B. -0,5'},
              {k: 'hum_offset', l: 'Korrektur Luftfeuchte', t: 'float', min: -20, max: 20, step: 0.5, u: '%'}],
      bthome: [{k: 'mac', l: 'Sensor-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen (passt: Dienst FCD2)'},
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
            {type: 'fridge', name: 'Kühlschrank WCS', ble: 2}, {type: 'dimes', name: 'Gas: DIMES / SRG WAVE', ble: 1},
            {type: 'mopeka', name: 'Gas: Mopeka Pro', ble: 1}, {type: 'mopeka_std', name: 'Gas: Mopeka Standard', ble: 1},
            {type: 'govee', name: 'Thermometer Govee', ble: 1}, {type: 'bthome', name: 'BTHome-Sensor (ATC/pvvx, Shelly BLU …)', ble: 1}, {type: 'blesensor', name: 'Eigener BLE-Sensor', ble: 1}],
    max_devices: 12,
    max_links: 3
  },
  pos: 0, target: -1, release: false,
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
        if (!c.schemas[t.type]) M.save({schemas: Object.assign({}, c.schemas, {[t.type]: [{k: 'mac', l: 'MAC', t: 'mac'}]})});
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
      const P = {auf: 100, zu: 0, regen: 12, lueften: 102};
      const k = body.k === 'preset' ? body.v : body.k;
      if (k === 'release') M.release = body.v === 'true' || body.v === 'ON' || body.v === '1';
      else if (k in P) M.target = P[k];
      else if (k === 'cover_position') M.target = +body.v;
      return {status: 'ok'};
    }
    if (path === '/api/scan') {
      return {active: true, devices: [
        {mac: 'F0:F8:F2:00:00:01', name: 'RVT-Skylight', rssi: -71, mfg: '', uuid: 'ffe0', age: 2, fits: ['rvtech']},
        {mac: 'A4:C1:38:AB:CD:EF', name: 'ATC_ABCDEF', rssi: -76, mfg: '', uuid: '0xfcd2', age: 3, fits: ['bthome']},
        {mac: 'A4:C1:38:00:00:02', name: 'GVH5075_1A2B', rssi: -83, mfg: '88EC', uuid: '', age: 5, fits: ['govee']}]};
    }
  },
  state() {
    // the hatch moves 5 % per update towards the target
    if (M.target >= 0) {
      const goal = Math.min(M.target, 100);
      if (M.pos < goal) M.pos = Math.min(goal, M.pos + 5);
      else if (M.pos > goal) M.pos = Math.max(goal, M.pos - 5);
      if (M.pos === goal) { if (M.target === 102) M.pos = 102; M.target = -1; }
    }
    const shown = Math.min(M.pos, 100);
    const label = M.pos === 102 ? 'Lüften' : M.target >= 0 ? (M.target > M.pos ? 'Öffnet...' : 'Schließt...') : M.pos === 0 ? 'Geschlossen' : 'Offen';
    // live data: only what runs in this boot (pending devices start with the restart)
    const runs = p => p.running && !p.removed;
    const on = runs(M.cfg.plugins[0]);
    const others = M.cfg.plugins.slice(1).filter(p => !p.pending && !p.removed).map(p => {
      const o = {id: p.id, name: p.cfg.name, type: p.type, enabled: runs(p)};
      if (!o.enabled) return o;
      if (p.type === 'bthome' && p.cfg.mac) return Object.assign(o, {state: 'OK', ok: true,
        rows: [['Batterie', 87, '%'], ['Temperatur', 19.8, '°C'], ['Luftfeuchte', 54.2, '%'], ['Spannung', 2.91, 'V'], ['Bluetooth', -76, 'dBm'], ['Alter', 6, 's']],
        raw: '40 00 A5 01 57 02 BC 07 03 2C 15 0C 5E 0B'});
      if (p.type === 'govee' && p.cfg.mac) return Object.assign(o, {state: 'OK', ok: true,
        rows: [['Temperatur', 4.3, '°C'], ['Luftfeuchte', 71, '%'], ['Batterie', 100, '%'], ['Modell', 'H5072/H5075', ''], ['Bluetooth', -83, 'dBm'], ['Alter', 12, 's']]});
      if (p.type !== 'blesensor') return Object.assign(o, {state: 'Warte auf Daten', ok: false, rows: []});
      const t = 21.4 + Math.sin((Date.now() - M.t0) / 60000) * 0.3;
      return Object.assign(o, {state: 'OK', ok: true,
        rows: [['Temperatur', Math.round(t * 10) / 10, '°C'], ['Luftfeuchte', 58, '%'], ['Batterie', 87, '%'], ['Bluetooth', -77, 'dBm'], ['Alter', 4, 's']],
        raw: '0: 56 34 12 38  4: C1 A4 5C 08  8: A8 16 1E 0B  12: 57 C2 0F'});
    });
    const plug = {id: 'rvtech', name: M.cfg.plugins[0].cfg.name, type: 'rvtech', enabled: on};
    if (on) Object.assign(plug, {
      state: M.release ? 'Für App freigegeben' : 'Verbunden', ok: !M.release,
      gauge: {v: shown, unit: '%', label},
      rows: [['Gemeldet', M.pos, ''], ['Ziel', M.target >= 0 ? M.target : null, ''], ['Bluetooth', -71, 'dBm']],
      ctrl: [{t: 'buttons', k: 'preset', b: [['auf', 'Auf'], ['zu', 'Zu'], ['regen', 'Regen'], ['lueften', 'Lüften']]},
             {t: 'slider', k: 'cover_position', l: 'Position', min: 0, max: 100, v: shown},
             {t: 'switch', k: 'release', l: 'Bluetooth für App freigeben', v: M.release}]
    });
    return {rssi: -58, uptime: M.up(), heap: 98000, wifi_drops: 0, wifi_reason: '', wifi: 'Verbunden', mqtt: true,
            ble: {adv: 1200 + Math.floor((Date.now() - M.t0) / 100), scanning: true}, plugins: [plug, ...others]};
  }
};
