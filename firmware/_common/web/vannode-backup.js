// VanNode "Einstellungen sichern" card, placed right below the firmware update card of every
// device page (vannode-system.js). Shared copy from firmware/_common/web
// (tools/common/sync_common.py) - edit it there.
// Works only in the browser with the existing /api/config of the device: the backup is the
// answer of GET /api/config without passwords, login and read-only values; restoring takes the
// device's current settings and overwrites only the saved values, then sends them the way the
// page itself does (JSON, or form fields on the MaxxFan and heater nodes). Network settings
// (WiFi, IP, host name, MQTT server) are only taken over when the box is ticked.
(function () {
  // Keys never put into a backup (secrets, login, read-only information)
  const SECRET = /pass|pwd|token|^wpw$|^web_user$|^wuser$/i;
  const READONLY = ['version', 'reset_reason', 'guard', 'sensor_def', 'pins', 'pin', 'chip', 'max_sensors',
    'tare_active', 'ci_ok', 'ci_possible', 'lin_active', 'one_uart', 'name', 'apssid', 'restart_needed', 'safe',
    'types', 'schemas', 'max_links', 'max_devices', 'ha_node', 'device', 'values', 'status'];
  // Network and identity: only with "auch Netzwerk übernehmen"
  const NET = ['wifi_ssid', 'ssid', 'wifi_tx_power', 'ap_ssid', 'ap_always', 'static_ip', 'ip', 'gateway', 'gw',
    'subnet', 'sn', 'dns', 'hostname', 'node_name', 'mqtt_server', 'mqtt_host', 'mhost', 'mqtt_port', 'mport',
    'mqtt_user', 'muser', 'mqtt_prefix', 'prefix', 'mqtt_base', 'mqtt_fb', 'mfb'];
  // Nodes whose /api/config takes form fields instead of JSON
  const FORM = {maxxfan: 1, heater: 1};
  const BLE = {ble: 1, toolbox: 1, battery: 1};
  // Display: settings, pages and tiles, automations and sensors (own API, camelCase keys)
  const D_SECRET = ['wifiPass', 'mqttPass', 'webPass', 'apPass', 'poToken', 'poUser'];
  const D_READONLY = ['fw', 'mac', 'poStatus'];
  const D_NET = ['wifiSsid', 'staticIp', 'ip', 'gateway', 'subnet', 'dns', 'hostname'];
  const IC_UP = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 16V4M6 10l6-6 6 6M4 20h16"/></svg>';
  const IC_DOWN = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4v12M6 10l6 6 6-6M4 20h16"/></svg>';
  const IC_FILE = '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 3H6v18h12V7z"/><path d="M14 3v4h4"/></svg>';

  const keep = (k) => !SECRET.test(k) && READONLY.indexOf(k) < 0 && !/_set$/.test(k);
  const strip = (o) => { const r = {}; for (const k in o) if (keep(k)) r[k] = o[k]; return r; };
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

  async function getJson(url) {
    const r = await fetch(url, {cache: 'no-store'});
    if (!r.ok) throw new Error(url + ': ' + r.status);
    return r.json();
  }
  // POST with retries while the device is busy saving the previous request (503)
  async function post(url, body, form) {
    for (let i = 0; i < 15; i++) {
      const opt = {method: 'POST'};
      if (form) {
        const f = new URLSearchParams();
        for (const k in body) {
          const v = body[k];
          if (v === null || v === undefined || typeof v === 'object') continue;
          f.append(k, v === true ? '1' : v === false ? '0' : String(v));
        }
        opt.body = f;
      } else {
        opt.headers = {'Content-Type': 'application/json'};
        opt.body = JSON.stringify(body);
      }
      const r = await fetch(url, opt);
      if (r.status === 503) { await sleep(400); continue; }
      let j = {};
      try { j = await r.json(); } catch (e) {}
      if (!r.ok) throw new Error(j.error || j.msg || ('HTTP ' + r.status));
      return j;
    }
    throw new Error('Gerät beschäftigt');
  }

  async function makeBackup(fw) {
    if (fw.dev === 'display') {
      const st = await getJson('/api/settings');
      D_SECRET.concat(D_READONLY).forEach((k) => delete st[k]);
      const b = {type: 'vannode-backup', version: 1, device: 'display', fw: fw.ver || '', date: new Date().toISOString(),
                 settings: st, pages: await getJson('/api/pages')};
      try { b.rules = await getJson('/api/rules'); } catch (e) {}
      try { b.sensors = await getJson('/api/sensors'); } catch (e) {}
      return b;
    }
    const cfg = await getJson('/api/config');
    const b = {type: 'vannode-backup', version: 1, device: fw.dev || '', fw: fw.ver || '', date: new Date().toISOString()};
    if (BLE[fw.dev]) {
      b.core = strip(cfg.core || {});
      b.plugins = (cfg.plugins || []).filter((p) => !p.removed).map((p) => ({id: p.id, type: p.type, cfg: p.cfg}));
    } else {
      b.config = strip(cfg);
    }
    return b;
  }

  // All current values of the device (login name included: some nodes clear what is missing)
  // + the allowed values of the backup
  function merged(cur, saved, net) {
    const out = {};
    for (const k in cur) if (READONLY.indexOf(k) < 0 && !/_set$/.test(k)) out[k] = cur[k];
    for (const k in saved) {
      if (!keep(k)) continue;
      if (!net && NET.indexOf(k) >= 0) continue;
      out[k] = saved[k];
    }
    return out;
  }

  // Display: pages, automations, sensors, then the settings (they may restart the display)
  async function restoreDisplay(b, net, log) {
    if (b.pages && b.pages.pages) { await post('/api/pages', b.pages); log('Seiten und Kacheln übernommen'); }
    if (b.rules && b.rules.rules) { await post('/api/rules', b.rules); log('Automationen übernommen'); }
    if (b.sensors && b.sensors.devices) { await post('/api/sensors', b.sensors); log('Sensoren übernommen'); }
    const s = Object.assign({}, b.settings || {});
    D_SECRET.concat(D_READONLY).forEach((k) => delete s[k]);
    if (!net) D_NET.forEach((k) => delete s[k]);
    await post('/api/settings', s);
    log('Einstellungen übernommen');
  }

  async function restoreBle(b, net, log) {
    let cfg = await getJson('/api/config');
    const have = {};
    (cfg.plugins || []).forEach((p) => { if (!p.removed) have[p.id] = p; });
    for (const p of b.plugins || []) {
      let id = p.id;
      if (!have[id] || have[id].type !== p.type) {
        // not on this device: add a new one of that type, it gets the next free id
        if (!(cfg.types || []).some((t) => t.type === p.type)) { log('Übersprungen (Typ fehlt in dieser Firmware): ' + p.id); continue; }
        try { await post('/api/config', {add: p.type}); } catch (e) { log('Nicht angelegt: ' + p.id + ' – ' + e.message); continue; }
        cfg = await getJson('/api/config');
        const fresh = (cfg.plugins || []).filter((x) => x.type === p.type && !x.removed && !have[x.id]);
        if (!fresh.length) { log('Nicht angelegt: ' + p.id); continue; }
        id = fresh[fresh.length - 1].id;
        have[id] = fresh[fresh.length - 1];
      }
      try { await post('/api/config', {plugin: id, cfg: p.cfg || {}}); log('Übernommen: ' + id); }
      catch (e) { log('Fehler bei ' + id + ': ' + e.message); }
    }
    // core last: a network change restarts the node right away
    const core = merged((await getJson('/api/config')).core || {}, b.core || {}, net);
    return post('/api/config', {core});
  }

  function card(upd, fw) {
    const c = document.createElement('div');
    c.className = upd.className;
    c.id = 'vn-backup';
    const t = upd.firstElementChild;
    const title = document.createElement(t ? t.tagName : 'h2');
    if (t) title.className = t.className;
    title.textContent = 'Einstellungen sichern';
    c.append(title);
    const disp = fw.dev === 'display';
    const what = disp ? 'Alle Einstellungen, Seiten, Kacheln, Automationen und Sensoren' : BLE[fw.dev] ? 'Alle Einstellungen und Geräte' : 'Alle Einstellungen';
    c.insertAdjacentHTML('beforeend',
      '<div class="vnc-hint">' + what + ' als Datei. Passwörter und die Anmeldung sind nicht enthalten, beim Einspielen bleiben die vorhandenen erhalten.</div>' +
      '<label class="vnc-file" id="vnbk-label">' + IC_FILE + '<span class="nm">Sicherung zum Einspielen wählen …</span>' +
      '<span class="pick">Durchsuchen</span><input type="file" id="vnbk-file" accept=".json,application/json"></label>' +
      '<label class="vnc-opt"><input type="checkbox" id="vnbk-net"> Auch WLAN, IP und Gerätename' + (disp ? '' : ' sowie MQTT-Server') + ' übernehmen</label>' +
      '<div class="vnc-row"><button type="button" class="vnc-b" id="vnbk-exp">' + IC_DOWN + 'Sichern</button>' +
      '<button type="button" class="vnc-b p" id="vnbk-imp" disabled>' + IC_UP + 'Einspielen</button></div>' +
      '<div class="vnc-msg" id="vnbk-msg"></div>');
    const vu = document.getElementById('vn-update');
    (vu || upd).insertAdjacentElement('afterend', c);
    const msg = c.querySelector('#vnbk-msg');
    const say = (s) => { msg.textContent = s; };
    const file = c.querySelector('#vnbk-file');
    file.addEventListener('change', () => {
      const f = file.files[0];
      c.querySelector('#vnbk-label').classList.toggle('has', !!f);
      c.querySelector('#vnbk-label .nm').textContent = f ? f.name : 'Sicherung zum Einspielen wählen …';
      c.querySelector('#vnbk-imp').disabled = !f;
    });

    c.querySelector('#vnbk-exp').onclick = async () => {
      try {
        const b = await makeBackup(fw);
        const name = (fw.dev || 'vannode') + '-' + new Date().toISOString().slice(0, 10) + '.json';
        const a = document.createElement('a');
        a.href = URL.createObjectURL(new Blob([JSON.stringify(b, null, 1)], {type: 'application/json'}));
        a.download = name;
        document.body.appendChild(a);
        a.click();
        a.remove();
        say('Gespeichert: ' + name);
      } catch (e) { say('Sichern fehlgeschlagen: ' + e.message); }
    };

    c.querySelector('#vnbk-imp').onclick = async () => {
      const f = file.files[0];
      if (!f) return say('Erst eine Sicherungsdatei wählen');
      let b;
      try { b = JSON.parse(await f.text()); } catch (e) { return say('Keine gültige Datei'); }
      // older display backups have their own type
      if (b && b.type === 'camper-display-backup') b = Object.assign({}, b, {type: 'vannode-backup', device: 'display'});
      if (!b || b.type !== 'vannode-backup') return say('Keine VanNode-Sicherung');
      const same = b.device === fw.dev || (BLE[b.device] && BLE[fw.dev] && (b.device === 'battery') === (fw.dev === 'battery'));
      if (!same) return say('Die Sicherung gehört zu „' + b.device + '“, nicht zu diesem Gerät („' + fw.dev + '“)');
      const net = c.querySelector('#vnbk-net').checked;
      if (!confirm('Einstellungen aus der Sicherung vom ' + String(b.date || '').slice(0, 10) + ' übernehmen?' +
        (net ? '\nWLAN, IP und Gerätename werden mit übernommen.' : '') + '\nDas Gerät startet danach eventuell neu.')) return;
      const lines = [];
      const log = (s) => { lines.push(s); say(lines.join('\n')); };
      try {
        if (disp) {
          await restoreDisplay(b, net, log);
          log('Fertig – Seite lädt gleich neu.');
          setTimeout(() => location.reload(), 4000);
        } else if (BLE[fw.dev]) {
          await restoreBle(b, net, log);
          log('Fertig. Geänderte Geräte gelten nach einem Neustart (Karte Gerät → Neustart).');
        } else {
          const cur = await getJson('/api/config');
          await post('/api/config', merged(cur, b.config || {}, net), !!FORM[fw.dev]);
          log('Fertig – Seite lädt gleich neu.');
          setTimeout(() => location.reload(), 6000);
        }
      } catch (e) { log('Einspielen fehlgeschlagen: ' + e.message); }
    };
  }

  let fw = null, busy = false;
  function place() {
    const cur = document.getElementById('fwu-cur');
    const upd = cur && cur.closest('.card');
    // with the shared system cards right below the update card; on pages without them (older
    // layout) only once the "Über VanNode" card exists, so the order stays update, backup, about
    const shared = !!document.getElementById('vn-update');
    if (!upd || (!shared && !document.getElementById('vn-about')) || document.getElementById('vn-backup')) return;
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
