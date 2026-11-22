// base: ble
// battery-node: same core and web page as the ble-node (ble.js above), only the battery types.
M.id = 'battery';
M.hw = 'atoms3';
M.chip = 'esp32s3';
M.fw = {name: 'Battery Node', dir: 'battery'};
Object.assign(M.config, {
  version: '2.5.1', device: 'battery-node', ha_node: 'camper_battery',
  core: Object.assign({}, M.config.core, {ap_ssid: 'Battery-Node-A1B2C3', mqtt_prefix: 'camper/battery', node_name: 'battery-node'}),
  plugins: [
    P('daly', 'daly', 'Batterie: Daly BMS', 2, {name: 'Aufbaubatterie', mac: MAC(6), interval: 10}),
    P('ective', 'ective', 'Batterie: Ective BM X', 2, {name: 'Zweitbatterie', mac: MAC(7), interval: 10}),
    P('bm2', 'bm2', 'Batterie: BM2 Batteriewächter', 2, {name: 'Starterbatterie', mac: MAC(8)})],
  types: [{type: 'daly', name: 'Batterie: Daly BMS', ble: 2}, {type: 'ective', name: 'Batterie: Ective BM X', ble: 2},
          {type: 'bm2', name: 'Batterie: BM2 Batteriewächter', ble: 2}],
  max_devices: 6,
  max_links: 3
});
