// alde-node: heater bus (passive) and external panel bus of an Alde Compact 3020 / 3030
const M = {
  id: 'alde',
  config: {
    version: '0.4.1', name: 'Alde Node', hostname: 'alde-node', reset_reason: 'Einschalten',
    wifi_ssid: 'Camper-WLAN', wifi_tx_power: 70, ap_ssid: 'Alde-Node-A1B2C3', ap_always: false,
    mqtt_server: '192.168.1.1', mqtt_port: 1883, mqtt_user: '', mqtt_prefix: 'camper/alde', web_user: '',
    interval: 10, ci_mode: 2, ci_function: 1, el_max: 1, lin_enabled: true, lin_active: true,
    ci_ok: true, ci_possible: true, one_uart: false
  },
  // panel state, changed by /api/control
  panel: {target: 21.0, gas: 1, el: 1, water: 1},
  reads: 0,
  route(path, method, b, q) {
    if (path === '/api/config' && method === 'POST') {
      // the device answers lin_enabled changes with a restart, the mockup applies them at once
      if ('lin_enabled' in b) M.save({lin_active: b.lin_enabled});
      return undefined;                         // default handling in core.js
    }
    if (path === '/api/control') {
      if (q.has('target')) M.panel.target = Math.min(30, Math.max(5, Math.round(+q.get('target') * 2) / 2));
      if (q.has('gas')) M.panel.gas = +q.get('gas') ? 1 : 0;
      if (q.has('el_level')) M.panel.el = Math.min(+q.get('el_level'), +M.cfg.el_max);
      if (q.has('water')) M.panel.water = Math.min(2, +q.get('water'));
      return {status: 'ok'};
    }
    if (path === '/api/measure') return {status: 'ok'};
    if (path === '/api/sniff_reset') { M.sniff0 = Date.now(); return {status: 'ok'}; }
    if (path === '/api/sniff') {
      // example: the panel answers info headers, nobody answers the diagnostic header
      const s = Math.max(1, Math.floor((Date.now() - (M.sniff0 || M.t0)) / 1000));
      const n = Math.floor(s / 1);
      return {active: +M.cfg.ci_mode === 3, bytes: n * 23, frames: n * 2, junk: 0, ids: [
        {id: 0x1B, pid: 0x5B, headers: n, answers: n, bad: 0, len: 8, lin2: true,
         data: '7E FE 5E DE 9E 29 00 00', age: 0},
        {id: 0x3C, pid: 0x3C, headers: n, answers: 0, bad: 0, len: 0, age: 0}]};
    }
  },
  state() {
    const t = (Date.now() - M.t0) / 1000;
    const w = (base, amp, per) => +(base + Math.sin(t / per) * amp).toFixed(2);
    const p = M.panel;
    const room = w(19.6, 0.3, 60);
    M.reads += 3;
    const vals = [
      ['room', 'Raumtemperatur', '°C', 1, room], ['setpoint', 'Sollwert (Heizbus)', '°C', 1, p.target],
      ['glycol', 'Glykol', '°C', 1, w(52, 3, 45)], ['water', 'Warmwasser', '°C', 1, w(48, 1, 90)],
      ['hotwater_target', 'Warmwasser Ziel', '°C', 0, p.water ? (p.water === 2 ? 65 : 50) : 0],
      ['pcb', 'Platine', '°C', 1, w(31, 0.5, 120)], ['battery', 'Batterie', 'V', 2, w(13.1, 0.05, 30)],
      ['mains_current', 'Landstrom', 'A', 2, p.el ? w(4.3 * p.el, 0.1, 20) : 0.12],
      ['el_power', 'Heizstab', 'W', 0, p.el * 1000], ['el_allowed', 'Heizstab erlaubt', 'W', 0, +M.cfg.el_max * 1000],
      ['burner_fan', 'Brennergebläse', 'rpm', 0, p.gas ? Math.round(w(2900, 80, 15)) : 0],
      ['gas', 'Gas', '', 0, p.gas], ['ac_active', 'Klimaanlage aktiv', '', 0, 0],
      ['ac_light', 'Klimaanlage Licht', '%', 0, 0], ['bus_ok', 'LIN-Bus', '', 0, 1],
      ['target', 'Sollwert', '°C', 1, p.target], ['panel_room', 'Raum (Bedienteil)', '°C', 1, +room.toFixed(1)],
      ['outdoor', 'Außentemperatur', '°C', 1, w(4.5, 1, 200)], ['gas_enabled', 'Gasbetrieb', '', 0, p.gas],
      ['el_level', 'Heizstab Stufe', 'kW', 0, p.el], ['water_mode', 'Warmwasser Modus', '', 0, p.water],
      ['pump', 'Umwälzpumpe', '', 0, 1], ['panel_on', 'Bedienteil an', '', 0, 1], ['error', 'Störung', '', 0, 0],
      ['ac_input', '230 V', '', 0, 1], ['ci_ok', 'Bedienteil-Bus', '', 0, +M.cfg.ci_mode === 3 ? null : 1]];
    const lin = M.cfg.lin_active !== false && M.cfg.lin_active !== 'false';
    return {values: vals.map(([key, name, unit, dec, v]) => ({key, name, unit, dec, v})),
            ok: lin || +M.cfg.ci_mode === 1 || +M.cfg.ci_mode === 2, age: 0, reads: M.reads, errors: 2,
            wifi: 'Verbunden', rssi: -61, mqtt: true, uptime: M.up(), heap: 182000, wifi_drops: 0, wifi_reason: '--'};
  }
};
