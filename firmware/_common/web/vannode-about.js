// VanNode "Über" card: logo, author, license, third-party parts and project link, placed right
// below the firmware update card of every device page. Shared copy from firmware/_common/web
// (tools/common/sync_common.py) - edit it there.
// This card is part of the Appropriate Legal Notices of VanNode. Under the additional terms of
// its GNU GPL v3 license (section 7 b/c, see NOTICE.md) the author attribution, the project link
// and the license information must stay in every copy and modified version.
(function () {
  const REPO = 'https://github.com/hflocki/vannode';
  const COPY = 'Copyright © 2026 hflocki';
  const A = (href, text) => '<a href="' + href + '" target="_blank" rel="noopener">' + text + '</a>';
  const GPL = 'Dieses Programm ist freie Software: Du kannst es unter den Bedingungen der GNU General Public License Version 3, wie von der Free Software Foundation veröffentlicht, weitergeben und/oder verändern. Es wird in der Hoffnung bereitgestellt, dass es nützlich ist, aber OHNE JEDE GEWÄHRLEISTUNG – sogar ohne die implizite Gewährleistung der MARKTREIFE oder der EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.';
  const TERMS = 'Zusätzliche Bedingungen nach Abschnitt 7 der GPL: Dieser Urhebervermerk, der Link zum Projekt und die Lizenzangaben müssen in allen Kopien und veränderten Fassungen erhalten bleiben. Veränderte Fassungen müssen als solche gekennzeichnet sein und dürfen nicht als eigenes Werk ausgegeben werden. Name und Logo „VanNode“ sind nicht Teil der Lizenz: Forks brauchen einen eigenen Namen und ein eigenes Logo.';
  const RISK = 'Nutzung auf eigene Gefahr: VanNode ist ein privates, unentgeltliches Hobbyprojekt und keine Sicherheitseinrichtung. Es ersetzt weder die Schutzfunktionen der Hersteller noch Gas-, CO- oder Rauchmelder; angezeigte Werte und Warnungen können falsch sein oder ausbleiben. Soweit gesetzlich zulässig, ist jede Haftung für Schäden ausgeschlossen. Produkt- und Firmennamen gehören ihren Inhabern, VanNode hat keine Verbindung zu ihnen.';
  // firmware id (FW_DEVICE) -> name, repo folder, third-party parts in this firmware
  const DEV = {
    tanklevel: {n: 'Tanklevel-Node', d: 'tanklevel', x: [
      'Mopeka-Dekodierung nach ' + A('https://github.com/Bluetooth-Devices/mopeka-iot-ble', 'mopeka-iot-ble') + ' (MIT, eigene Umsetzung)']},
    temp:      {n: 'Temp-Node', d: 'temp'},
    maxxfan:   {n: 'MaxxFan-Node', d: 'maxxfan'},
    example:   {n: 'Vorlage', d: '_template'},
    toolbox:   {n: 'BLE-Node', d: 'ble', x: [
      'Batterie-Protokolle (Daly, Ective, BM2) portiert aus ' + A('https://github.com/patman15/aiobmsble', 'aiobmsble') + ' (Apache-2.0)',
      'Dachhauben-Protokoll portiert aus ' + A('https://github.com/C64Axel/rvtech-hacs', 'rvtech-hacs') + ', © 2026 C64Axel (MIT)',
      'Pumpen-Protokoll nach der HA-Integration carawater_isp15 eines Community-Mitglieds (eigene Umsetzung)',
      'BM2-Format nach bm2-esphome und ha_bm2monitor, Govee und Mopeka nach govee-ble und mopeka-iot-ble (alle MIT, eigene Umsetzung)']},
    heater:    {n: 'Heater-Node', d: 'heater', x: [
      'Protokoll-Logik portiert aus ' + A('https://github.com/skrebber/esphome-truma_inetbox', 'skrebber/esphome-truma_inetbox') + ' (Fork von Fabian-Schmidt, GPL-3.0)',
      'Timer aus ' + A('https://github.com/havanti/esphome-truma', 'havanti/esphome-truma') + ' (GPL-3.0); Vorarbeit ' + A('https://github.com/danielfett/inetbox.py', 'inetbox.py') + ' und ' + A('https://github.com/muccc/WomoLIN', 'WomoLIN') + ' (GPL-3.0)',
      'Fehlercodes nach dem Truma-Diagnosekatalog und den CP-Plus-Codelisten, Texte eigenständig formuliert']},
    level:     {n: 'Level-Node', d: 'level', x: [
      'Basiert auf dem ' + A('https://github.com/klehnst/ESP32-Adrafruit-BNO085-HomeAssistant-Caravan-Levelsensor-MQTT', 'BNO085-Levelsensor von klehnst') + ', © 2026 klehnst (MIT)']},
  };
  DEV.ble = DEV.toolbox;   // mockups use the folder name
  DEV.template = DEV.example;
  const HW = {esp32: 'ESP32', esp32c3: 'ESP32-C3', esp32s3: 'ESP32-S3', esp32c6: 'ESP32-C6', esp8266: 'ESP8266', atoms3: 'AtomS3', core2: 'Core2'};
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"]/g, (c) => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[c]));
  const small = 'margin:6px 0;font-size:.85em;opacity:.8';

  function card(upd, fw) {
    const info = DEV[fw.dev] || {n: fw.dev || 'VanNode', d: ''};
    const c = document.createElement('div');
    c.className = upd.className;
    c.id = 'vn-about';
    c.style.marginTop = '12px';
    const t = upd.firstElementChild;
    const title = document.createElement(t ? t.tagName : 'h2');
    if (t) title.className = t.className;
    title.textContent = 'Über VanNode';
    const base = info.d ? REPO + '/blob/main/firmware/' + info.d + '/' : REPO + '/blob/main/';
    c.append(title);
    c.insertAdjacentHTML('beforeend',
      '<i class="vnlogo" role="img" aria-label="VanNode" style="margin:4px 0 10px"></i>' +
      '<p style="margin:6px 0"><b>' + esc(info.n) + '</b> · Firmware ' + esc(fw.ver || '--') + (fw.hw ? ' · ' + esc(HW[fw.hw] || fw.hw) : '') + '</p>' +
      '<p style="margin:6px 0">' + COPY + '<br>Teil von VanNode – Camper-Elektronik über MQTT<br>' + A(REPO, 'github.com/hflocki/vannode') + '</p>' +
      '<p style="margin:6px 0">Lizenz: <b>GNU GPL Version 3</b>. ' + GPL + '</p>' +
      '<p style="' + small + '">' + TERMS + '</p>' +
      '<p style="margin:6px 0;font-size:.85em"><b>Ohne Gewähr.</b> ' + RISK + '</p>' +
      (info.x ? '<p style="' + small + '">Enthält fremde Teile:<br>' + info.x.join('<br>') + '</p>' : '') +
      '<p style="margin:6px 0;font-size:.85em">' + A(base + 'LICENSE', 'Lizenztext (GPL-3.0)') + ' · ' + A(base + 'NOTICE.md', 'Hinweise und fremde Teile') + '</p>');
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
