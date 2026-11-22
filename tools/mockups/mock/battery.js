// base: ble
// battery-node: same core and web page as the ble-node (ble.js above), only the battery types.
M.id = 'battery';
M.hw = 'atoms3';
M.chip = 'esp32s3';
M.fw = {name: 'Battery Node', dir: 'battery'};
Object.assign(M.config, {
  version: '2.6.1', device: 'battery-node', ha_node: 'camper_battery',
  core: Object.assign({}, M.config.core, {ap_ssid: 'Battery-Node-A1B2C3', mqtt_prefix: 'camper/battery', node_name: 'battery-node'}),
  plugins: [
    P('daly', 'daly', 'Batterie: Daly BMS', 2, {name: 'Aufbaubatterie', mac: MAC(6), interval: 10}),
    P('ective', 'ective', 'Batterie: Ective BM X', 2, {name: 'Zweitbatterie', mac: MAC(7), interval: 10}),
    P('bm2', 'bm2', 'Batterie: BM2 Batteriewächter', 2, {name: 'Starterbatterie', mac: MAC(8)}),
    P('victron', 'victron', 'Batterie: Victron SmartShunt / BMV, Solar, DC-DC (Instant Readout)', 1, {name: 'SmartShunt', mac: MAC(18), key: '0123456789abcdef0123456789abcdef'})],
  types: [{type: 'daly', name: 'Batterie: Daly BMS', ble: 2}, {type: 'ective', name: 'Batterie: Ective BM X', ble: 2},
          {type: 'bm2', name: 'Batterie: BM2 Batteriewächter', ble: 2},
          {type: 'jbd', name: 'Batterie: JBD / Xiaoxiang BMS', ble: 2}, {type: 'jk', name: 'Batterie: JK-BMS', ble: 2},
          {type: 'litime', name: 'Batterie: LiTime / Redodo / PowerQueen', ble: 2},
          {type: 'victron', name: 'Batterie: Victron SmartShunt / BMV, Solar, DC-DC (Instant Readout)', ble: 1}],
  max_devices: 6,
  max_links: 3
});
