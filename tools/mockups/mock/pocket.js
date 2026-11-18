// pocket-node: M5Stack Core2. In the browser it only has the update page (webota.cpp);
// the screens on the device itself are drawn by the firmware.
const M = {
  id: 'pocket',
  chip: 'esp32',
  config: {version: '1.6.0'},
  route(path) {
    if (path === '/status') {
      const s = M.up();
      return {version: M.cfg.version, wifi: true, rssi: -64, mqtt: true, broker: '192.168.1.1:1883', battery: 81,
              charging: false, uptime: Math.floor(s / 3600) + ' h ' + Math.floor(s / 60) % 60 + ' min', awake: '4 min 30 s'};
    }
  }
};
