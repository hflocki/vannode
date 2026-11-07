// ble-node: Bluetooth devices as plugins (here: one RV Tech roof hatch)
const M = {
  id: 'ble',
  config: {
    version: '1.4.0', device: 'BLE Node', reset_reason: 'Neustart (Software)', ha_node: 'camper_ble',
    core: {wifi_ssid: 'Camper-WLAN', wifi_pass_set: true, ap_ssid: 'BLE-Node-A1B2C3', wifi_tx_power: 70, ap_always: false,
           mqtt_server: '192.168.1.1', mqtt_port: 1883, mqtt_user: '', mqtt_pass_set: false, mqtt_prefix: 'camper/ble',
           web_user: '', web_pass_set: false},
    plugins: [{id: 'rvtech', type: 'rvtech', type_name: 'RV Tech Dachhaube', ble: 2,
               fields: [{k: 'mac', l: 'Hauben-MAC', t: 'mac', h: 'Über "Suchen" aus der Liste wählen'},
                        {k: 'pin', l: 'PIN', t: 'text', max: 7, h: 'Nur Ziffern (1-6, auch 000000). Leer lassen, wenn keine nötig ist.'}],
               cfg: {enabled: true, name: 'Dachhaube', mac: 'F0:F8:F2:00:00:01', pin: ''}},
              // further slots, all off (every plugin is in the release; switched on in System)
              ...[['dimes', 'dimes', 'DIMES / SRG WAVE', 'Gas (DIMES)', 1], ['dimes2', 'dimes', 'DIMES / SRG WAVE', 'Gas (DIMES) 2', 1],
                  ['mopeka', 'mopeka', 'Mopeka Pro', 'Gas (Mopeka)', 1], ['mopeka2', 'mopeka', 'Mopeka Pro', 'Gas (Mopeka) 2', 1],
                  ['mopeka_std', 'mopeka_std', 'Mopeka Standard', 'Gas (Mopeka Std)', 1],
                  ['rvtech2', 'rvtech', 'RV Tech Dachhaube', 'Dachhaube 2', 2], ['pump', 'pump', 'CaraWater Pumpe', 'Wasserpumpe', 2],
                  ['fridge', 'fridge', 'WCS Kühlschrank', 'Kühlschrank', 2]]
                .map(([id, type, tn, name, ble]) => ({id, type, type_name: tn, ble, fields: [{k: 'mac', l: 'MAC', t: 'mac'}], cfg: {enabled: false, name, mac: ''}}))],
    max_links: 3
  },
  pos: 0, target: -1, release: false,
  route(path, method, body) {
    if (path === '/api/config' && method === 'POST') {
      const c = M.cfg;
      if (body.core) M.save({core: Object.assign({}, c.core, body.core, {wifi_pass: undefined, mqtt_pass: undefined, web_pass: undefined})});
      else if (body.plugin) {
        const p = c.plugins.find(x => x.id === body.plugin);
        if (p) { Object.assign(p.cfg, body.cfg); M.save({plugins: c.plugins}); }
      } else if (body.enabled) {
        c.plugins.forEach(p => { if (p.id in body.enabled) p.cfg.enabled = body.enabled[p.id]; });
        M.save({plugins: c.plugins});
        return {status: 'ok', restart: true};
      }
      return {status: 'ok'};
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
        {mac: 'A4:C1:38:00:00:02', name: 'GVH5075', rssi: -83, mfg: '88EC', uuid: '', age: 5, fits: []}]};
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
    const on = M.cfg.plugins[0].cfg.enabled;
    const off = M.cfg.plugins.slice(1).map(p => ({id: p.id, name: p.cfg.name, type: p.type, enabled: p.cfg.enabled}));
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
            ble: {adv: 1200 + Math.floor((Date.now() - M.t0) / 100), scanning: true}, plugins: [plug, ...off]};
  }
};
