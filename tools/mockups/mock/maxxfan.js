// maxxfan-node: VanTurtle board, buttons of the MaxxFan remote via port expander
const M = {
  id: 'maxxfan',
  config: {hostname: 'maxxfan-node', ssid: 'Camper-WLAN', static_ip: false, ip: '', gw: '', sn: '255.255.255.0', dns: '',
           mhost: '192.168.1.1', mport: 1883, muser: '', prefix: 'camper/maxxfan', disc: true, wuser: 'admin',
           p0: false, p1: true, pulse: 150, syncport: 1, lidpin: 4, lidinv: false, apssid: 'MaxxFan-Node-A1B2C3'},
  ports: [{label: 'RJ11', hold: false, led: false}, {label: 'RJ45', hold: false, led: true}],
  sync: false,
  lid: 1,   // reed contact on the lid: 1 open, 0 closed
  route(path, method, b) {
    const pt = M.ports[+b.port || 0];
    if (path === '/api/state') {
      const on = v => v === true || v === '1' || v === 1;
      return {fw: '1.5.0', expander: true, sync: M.sync, syncPort: +M.cfg.syncport, lid: +M.cfg.lidpin < 0 ? -1 : M.lid,
              ports: M.ports.map((p, i) => Object.assign({enabled: on(M.cfg['p' + i])}, p)),
              sta: true, ap: false, ip: '192.168.1.50', rssi: -58, mqtt: true, uptime: M.up()};
    }
    if (path === '/api/press' && b.btn === 'auto') pt.hold = !pt.hold;
    if (path === '/api/press' && b.btn === 'on_off') M.lid = 1;
    if (path === '/api/led') pt.led = b.on === '1';
    if (path === '/api/close' || path === '/api/speed') {
      M.sync = true;
      setTimeout(() => { M.sync = false; if (path === '/api/close' || b.level === '0') M.lid = 0; }, 3000);
    }
    if (path === '/api/state' || path === '/api/press' || path === '/api/led' || path === '/api/close' || path === '/api/speed') return {ok: true};
  }
};
