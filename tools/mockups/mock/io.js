// io-node: relays, inputs, DS18B20 and analog values, channels set up in the web page
const M = {
  id: 'io',
  config: {version: '1.0.0', reset_reason: 'Einschalten', wifi_ssid: 'Camper-WLAN', ap_ssid: 'IO-Node-A1B2C3',
           wifi_tx_power: 70, ap_always: false, mqtt_server: '192.168.1.1', mqtt_port: 1883, mqtt_user: '',
           mqtt_prefix: 'camper/io', web_user: 'admin'},
  io: {hw: {ow: 3, sr_data: 5, sr_clock: 6, sr_latch: 7, sr_oe: -1, sr_chips: 2, sda: 0, scl: 1,
            exp_type: 'mcp23017', exp_addr: 32, exp_chips: 1, interval: 10},
       ch: [{id: 'wasserpumpe', name: 'Wasserpumpe', type: 'relay', bus: 'sr', chip: 0, pin: 0, low: true, boot: 0, pulse: 0},
            {id: 'licht_aussen', name: 'Licht außen', type: 'relay', bus: 'sr', chip: 0, pin: 1, low: true, boot: 2, pulse: 0},
            {id: 'boiler', name: 'Boiler', type: 'relay', bus: 'gpio', chip: 0, pin: 10, low: true, boot: 0, pulse: 0},
            {id: 'tuer', name: 'Schiebetür', type: 'input', bus: 'exp', chip: 0, pin: 0, low: true, pullup: true, debounce: 50},
            {id: 'klappe', name: 'Serviceklappe', type: 'input', bus: 'exp', chip: 0, pin: 1, low: true, pullup: true, debounce: 50},
            {id: 'innen', name: 'Innen', type: 'temp', rom: '28FF4A1B65160301', offset: 0},
            {id: 'boden', name: 'Boden', type: 'temp', rom: '28FF9C2A70160411', offset: 0},
            {id: 'starter', name: 'Starterbatterie', type: 'analog', pin: 4, raw1: 0, val1: 0, raw2: 2500, val2: 15, unit: 'V', dec: 2}],
       max_ch: 48, sr_max: 4, exp_max: 8, chip: 'c3'},
  on: {wasserpumpe: false, licht_aussen: true, boiler: false},
  route(path, method, body, q) {
    if (path === '/api/io' && method === 'GET') return M.io;
    if (path === '/api/io' && method === 'POST') { M.io = Object.assign({}, M.io, body); return {status: 'ok'}; }
    if (path === '/api/io/set') { const id = q.get('id'); if (id in M.on) M.on[id] = q.get('v') === 'toggle' ? !M.on[id] : q.get('v') === 'on'; return {status: 'ok'}; }
    if (path === '/api/io/scan' && method === 'POST') return {status: 'ok'};
    if (path === '/api/io/scan') return {done: true, roms: [{rom: '28FF4A1B65160301', t: 19.4}, {rom: '28FF9C2A70160411', t: 11.2}, {rom: '28FF11D3A2160522', t: 21.0}], i2c: [32]};
  },
  state() {
    const t = (Date.now() - M.t0) / 1000;
    const ch = M.io.ch.map((c) => {
      const o = {id: c.id, name: c.name, type: c.type};
      if (c.type === 'relay') o.on = !!M.on[c.id];
      if (c.type === 'input') o.on = c.id === 'tuer' ? Math.floor(t / 20) % 2 === 1 : false;
      if (c.type === 'temp') { o.v = Math.round((c.id === 'innen' ? 19.4 : 11.2) * 10 + Math.sin(t / 30) * 3) / 10; o.unit = '°C'; o.dec = 1; }
      if (c.type === 'analog') { const mv = Math.round(2110 + Math.sin(t / 25) * 12); o.mv = mv; o.v = Math.round((c.val1 + (mv - c.raw1) * (c.val2 - c.val1) / (c.raw2 - c.raw1)) * 100) / 100; o.unit = c.unit; o.dec = c.dec; }
      return o;
    });
    return {ch, wifi: 'Verbunden', mqtt: true, rssi: -57, uptime: M.up(), heap: 182000, wifi_drops: 0, wifi_reason: '-'};
  }
};
