// VanNode "Über" card: logo, author, license and project link, placed right below the
// firmware update card of every device page. Shared copy from firmware/_common/web
// (tools/common/sync_common.py) - edit it there.
// The copyright line and the license notice are required by the licenses and must stay in
// every copy and fork (see AGENTS.md).
(function () {
  const REPO = 'https://github.com/hflocki/vannode';
  const COPY = 'Copyright © 2026 hflocki';
  const MIT = 'Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.';
  const GPL = 'Dieses Programm ist freie Software: Du kannst es unter den Bedingungen der GNU General Public License Version 3, wie von der Free Software Foundation veröffentlicht, weitergeben und/oder verändern. Es wird in der Hoffnung bereitgestellt, dass es nützlich ist, aber OHNE JEDE GEWÄHRLEISTUNG – sogar ohne die implizite Gewährleistung der MARKTREIFE oder der EIGNUNG FÜR EINEN BESTIMMTEN ZWECK. Den vollständigen Lizenztext findest du unter dem Link unten.';
  // firmware id (FW_DEVICE) -> name, repo folder, license, note on third-party parts
  const DEV = {
    tanklevel: {n: 'Tanklevel-Node', d: 'tanklevel', l: 'MIT'},
    temp:      {n: 'Temp-Node', d: 'temp', l: 'MIT'},
    maxxfan:   {n: 'MaxxFan-Node', d: 'maxxfan', l: 'MIT'},
    example:   {n: 'Vorlage', d: '_template', l: 'MIT'},
    toolbox:   {n: 'BLE-Node', d: 'ble', l: 'MIT',
                x: 'Die BMS-Protokolle (Daly, Ective, BM2) sind aus <a href="https://github.com/patman15/aiobmsble" target="_blank" rel="noopener">aiobmsble</a> portiert und stehen unter der Apache-Lizenz 2.0 (<a href="' + REPO + '/blob/main/firmware/ble/LICENSE-Apache-2.0" target="_blank" rel="noopener">Lizenztext</a>). Dachhaube nach der HA-Integration rvtech-hacs.'},
    heater:    {n: 'Heater-Node', d: 'heater', l: 'GPL-3.0',
                x: 'Die Protokoll-Logik ist aus <a href="https://github.com/skrebber/esphome-truma_inetbox" target="_blank" rel="noopener">skrebber/esphome-truma_inetbox</a> (Fork von Fabian-Schmidt/esphome-truma_inetbox) portiert, das Setzen des Timers aus <a href="https://github.com/havanti/esphome-truma" target="_blank" rel="noopener">havanti/esphome-truma</a>, beide GPL-3.0.'},
    level:     {n: 'Level-Node', d: 'level', l: '',
                x: 'Basiert auf dem <a href="https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT" target="_blank" rel="noopener">BNO085-Levelsensor von klehnst</a>. Die Rechte an den übernommenen Teilen (Sensor-Grundlogik, Einbaulagen, Wasserwaagen-Oberfläche) liegen bei ihm; das Original enthält keine Lizenzangabe.'},
  };
  DEV.ble = DEV.toolbox;   // mockups use the folder name
  DEV.template = DEV.example;
  const HW = {esp32: 'ESP32', esp32c3: 'ESP32-C3', esp32s3: 'ESP32-S3', esp32c6: 'ESP32-C6', esp8266: 'ESP8266', atoms3: 'AtomS3', core2: 'Core2'};
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"]/g, (c) => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));

  function card(upd, fw) {
    const info = DEV[fw.dev] || {n: fw.dev || 'VanNode', d: '', l: 'MIT'};
    const c = document.createElement('div');
    c.className = upd.className;
    c.id = 'vn-about';
    c.style.marginTop = '12px';
    const t = upd.firstElementChild;
    const title = document.createElement(t ? t.tagName : 'h2');
    if (t) title.className = t.className;
    title.textContent = 'Über VanNode';
    const lic = info.l === 'MIT'
      ? '<p style="margin:6px 0">Lizenz: <b>MIT</b>. Nutzen, ändern und weitergeben ist erlaubt, solange dieser Copyright-Vermerk und der Lizenztext erhalten bleiben.</p>' +
        '<details><summary style="cursor:pointer">Lizenztext (MIT)</summary><pre style="white-space:pre-wrap;font-size:12px;line-height:1.45;opacity:.75;margin:6px 0 0">MIT License\n\n' +
        COPY.replace('©', '(c)') + '\n\n' + esc(MIT) + '</pre></details>'
      : info.l === 'GPL-3.0'
        ? '<p style="margin:6px 0">Lizenz: <b>GNU GPL Version 3</b>. ' + GPL + '</p>'
        : '';
    const file = info.d ? REPO + '/blob/main/firmware/' + info.d + '/' + (info.l ? 'LICENSE' : 'README.md#lizenz') : REPO;
    c.append(title);
    c.insertAdjacentHTML('beforeend',
      '<i class="vnlogo" role="img" aria-label="VanNode" style="margin:4px 0 10px"></i>' +
      '<p style="margin:6px 0"><b>' + esc(info.n) + '</b> · Firmware ' + esc(fw.ver || '--') + (fw.hw ? ' · ' + esc(HW[fw.hw] || fw.hw) : '') + '</p>' +
      '<p style="margin:6px 0">' + COPY + '<br>Teil von VanNode – Camper-Elektronik über MQTT<br>' +
      '<a href="' + REPO + '" target="_blank" rel="noopener">github.com/hflocki/vannode</a></p>' +
      lic + (info.x ? '<p style="margin:6px 0;font-size:.85em;opacity:.8">' + info.x + '</p>' : '') +
      '<p style="margin:6px 0;font-size:.85em"><a href="' + file + '" target="_blank" rel="noopener">Lizenz im Projekt ansehen</a></p>');
    upd.insertAdjacentElement('afterend', c);
  }

  let fw = null, busy = false;
  function place() {
    const cur = document.getElementById('fwu-cur');
    const upd = cur && cur.closest('.card');
    if (!upd || document.getElementById('vn-about')) return;
    if (fw) return card(upd, fw);
    if (busy) return;
    busy = true;
    fetch('/api/system', {cache: 'no-store'}).then((r) => r.json()).then((j) => { fw = j.fw || {}; })
      .catch(() => { fw = {}; }).finally(() => { busy = false; place(); });
  }
  // The update card is built by the page (sometimes only when its tab opens): watch for it
  new MutationObserver(place).observe(document.documentElement, {childList: true, subtree: true});
  document.addEventListener('DOMContentLoaded', place);
})();
