// VanNode system cards, the same on every device page: "Firmware- & Web-Update", "Ereignisse",
// "Uhrzeit" (only devices whose /api/system has "clock", see clockset.h) and "Gerät" (restart,
// factory reset). Shared copy from firmware/_common/web
// (tools/common/sync_common.py) - edit it there.
// The page only puts <div id="vn-system"></div> where the cards belong (data-log="0": no event
// card, the page shows the log elsewhere). The "Einstellungen sichern" card (vannode-backup.js)
// goes right below the update card, "Über VanNode" (vannode-about.js) at the end.
// The file checks (fwInspect) are the same rules as fwcheck.cpp on the devices: change both together.
(function () {
  // Device differences, by firmware id (FW_DEVICE)
  const DEVCFG = {
    heater:  {upd: '/update', field: 'firmware', web: false, restart: '/api/reboot'},
    display: {upd: '/update', field: 'firmware', web: false, restart: '/api/reboot', factory: '/api/factory'},
    maxxfan: {field: 'update', fs: true, restart: '/api/reboot', factory: '/api/factory'},
    ble:     {factory: '/api/factory'},
    toolbox: {factory: '/api/factory'},
    battery: {factory: '/api/factory'},
  };
  const DEF = {upd: '/api/update', field: 'f', web: true, fs: false, restart: '/api/restart', factory: ''};
  const FW_CHIPS = {'-1': 'ESP8266', 0: 'ESP32', 2: 'ESP32-S2', 5: 'ESP32-C3', 9: 'ESP32-S3', 12: 'ESP32-C2', 13: 'ESP32-C6', 16: 'ESP32-H2'};
  const esc = (s) => String(s == null ? '' : s).replace(/[&<>"']/g, (c) => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
  const ICON = {
    up: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 16V4M6 10l6-6 6 6M4 20h16"/></svg>',
    down: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4v12M6 10l6 6 6-6M4 20h16"/></svg>',
    file: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M14 3H6v18h12V7z"/><path d="M14 3v4h4"/></svg>',
    restart: '<svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 12a8 8 0 1 1-2.3-5.7M20 4v5h-5"/></svg>',
  };
  // Own look, independent of the page's classes; colours from the page's variables (VanNode theme
  // or display), with the dark theme values as fallback
  const CSS = `
.vnc{background:var(--card,var(--bg-card,#151B21));color:var(--tx,var(--text-primary,#E7ECF0));border:1px solid var(--line,var(--border,#222B34));border-radius:14px;padding:16px;margin:0 0 12px;text-align:left;box-sizing:border-box}
.vnc *{box-sizing:border-box}
.vnc .vnc-t{font-size:.85rem;font-weight:600;margin:0 0 12px;color:var(--tx2,var(--text-secondary,#B4BEC8));text-transform:none;letter-spacing:0;padding:0;border:0}
.vnc .vnc-cur{font-size:.85rem;margin:0 0 10px;line-height:1.45}
.vnc .vnc-hint{font-size:.78rem;color:var(--mu,var(--text-muted,#8B98A5));margin:0 0 12px;line-height:1.5}
.vnc .vnc-hint code{font-size:.95em}
.vnc .vnc-file{display:flex;align-items:center;gap:10px;min-height:46px;padding:6px 6px 6px 12px;margin:0 0 10px;border:1px dashed var(--line2,var(--cm-line2,#2F3A45));border-radius:10px;background:var(--bg-input,var(--bg,#0D1115));color:var(--mu,var(--text-muted,#8B98A5));cursor:pointer}
.vnc .vnc-file:hover{border-color:var(--ac,var(--accent,#4DA3F0))}
.vnc .vnc-file input{display:none}
.vnc .vnc-file .nm{flex:1;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;font-size:.9rem}
.vnc .vnc-file.has .nm{color:var(--tx,var(--text-primary,#E7ECF0))}
.vnc .vnc-file .pick{flex:none;display:inline-flex;align-items:center;gap:6px;height:34px;padding:0 12px;border-radius:8px;font-size:.85rem;font-weight:600;background:var(--hi,var(--cm-hi,#1C242C));color:var(--tx,var(--text-primary,#E7ECF0));border:1px solid var(--line2,var(--cm-line2,#2F3A45))}
.vnc .vnc-row{display:flex;gap:10px;flex-wrap:wrap}
.vnc .vnc-b{flex:1 1 0;min-width:120px;min-height:44px;margin:0;padding:0 14px;border-radius:10px;font:inherit;font-size:.95rem;font-weight:600;cursor:pointer;display:inline-flex;align-items:center;justify-content:center;gap:8px;background:var(--hi,var(--cm-hi,#1C242C));color:var(--tx,var(--text-primary,#E7ECF0));border:1px solid var(--line2,var(--cm-line2,#2F3A45))}
.vnc .vnc-b.p{background:var(--ac,var(--accent,#4DA3F0));border-color:var(--ac,var(--accent,#4DA3F0));color:var(--acx,var(--cm-acx,#06131F))}
.vnc .vnc-b.d{background:transparent;color:#E5534B;border-color:rgba(229,83,75,.45)}
.vnc .vnc-b:disabled{opacity:.45;cursor:default}
.vnc .vnc-chk{font-size:.82rem;margin:0 0 10px;line-height:1.45}
.vnc .vnc-chk:empty{display:none}
.vnc .ok{color:#46B97C}.vnc .warn{color:#E5A23B}.vnc .bad{color:#E5534B}
.vnc .vnc-opt{display:flex;gap:8px;align-items:center;font-size:.85rem;margin:0 0 10px;cursor:pointer}
.vnc .vnc-opt input{width:18px;height:18px;margin:0;accent-color:var(--ac,var(--accent,#4DA3F0))}
.vnc .vnc-bar{height:6px;border-radius:3px;background:var(--line,var(--border,#222B34));overflow:hidden;margin:0 0 10px}
.vnc .vnc-bar i{display:block;height:100%;width:0;background:var(--ac,var(--accent,#4DA3F0));transition:width .2s}
.vnc .vnc-msg{font-size:.82rem;margin:10px 0 0;line-height:1.45;white-space:pre-line;color:var(--mu,var(--text-muted,#8B98A5))}
.vnc .vnc-msg:empty{display:none}
.vnc .fwu-log{font-size:.75rem;line-height:1.4;max-height:320px;overflow-y:auto}
.vnc .fwu-ev{padding:4px 0;border-bottom:1px solid var(--line,var(--border,#222B34))}
.vnc .fwu-ev span{color:var(--mu,var(--text-muted,#8B98A5));margin-right:8px}
.vnc .fwu-ev.bad{color:#E5534B}
.vnc .vnc-in{flex:1 1 180px;min-width:0;min-height:44px;padding:0 12px;border-radius:10px;font:inherit;font-size:.95rem;background:var(--bg-input,var(--bg,#0D1115));color:var(--tx,var(--text-primary,#E7ECF0));border:1px solid var(--line2,var(--cm-line2,#2F3A45));color-scheme:dark}`;

  let FWI = null, LOG = null, UP = 0, cfg = DEF, pick = null, CLK = null, clkAuto = false;
  const $ = (id) => document.getElementById(id);

  function verCmp(a, b) {
    const x = String(a).split('.').map((n) => parseInt(n) || 0), y = String(b).split('.').map((n) => parseInt(n) || 0);
    for (let i = 0; i < Math.max(x.length, y.length); i++) { const d = (x[i] || 0) - (y[i] || 0); if (d) return d; }
    return 0;
  }
  const ago = (s) => s < 90 ? s + ' s' : s < 5400 ? Math.round(s / 60) + ' min' : s < 172800 ? Math.round(s / 3600) + ' h' : Math.round(s / 86400) + ' d';
  const hwHint = () => FWI && FWI.hw ? ` – passende Datei: …-node-${FWI.hw}-v….bin` : '';

  // Checks the chosen file in the browser: {ok, kind, msg, warn, force} (rules as in fwcheck.cpp)
  async function inspect(file) {
    const n = file.name.toLowerCase(), kb = Math.round(file.size / 1024) + ' kB';
    if (n.endsWith('.gz')) {
      if (!cfg.web) return {ok: false, msg: 'Die Web-Oberfläche steckt bei diesem Gerät in der Firmware – nur die .bin hochladen'};
      return {ok: true, kind: 'web', msg: 'Web-Oberfläche (' + kb + '), ersetzt nur die Seite, ohne Neustart'};
    }
    if (cfg.fs && /littlefs|spiffs/.test(n)) return {ok: true, kind: 'fs', warn: true, msg: 'Dateisystem-Abbild (' + kb + ') – ersetzt die Web-Oberfläche, danach Neustart'};
    if (!n.endsWith('.bin')) return {ok: false, msg: cfg.web ? 'Nur .bin (Firmware) oder .gz (Web-Oberfläche)' : 'Nur .bin (Firmware)'};
    const merged = 'Das ist die .merged.bin (komplettes Flash-Abbild für USB). Bitte die .bin ohne „merged“ nehmen.';
    if (n.includes('merged')) return {ok: false, msg: merged};
    const u = new Uint8Array(await file.arrayBuffer());
    if (u.length < 64 || u[0] !== 0xE9) return {ok: false, msg: 'Keine ESP-Firmware'};
    const esp32 = !FWI || FWI.chip_id >= 0;   // ESP8266 images have no chip id and no .merged.bin
    if (esp32 && u.length > 0x8001 && u[0x8000] === 0xAA && u[0x8001] === 0x50) return {ok: false, msg: merged};
    const chip = u[12] | (u[13] << 8);
    if (FWI && esp32 && chip !== FWI.chip_id) return {ok: false, msg: `Firmware für ${FW_CHIPS[chip] || 'einen unbekannten Chip'}, dieses Gerät ist ein ${FW_CHIPS[FWI.chip_id] || FWI.chip}${hwHint()}`};
    if (FWI && FWI.max && u.length > FWI.max) return {ok: false, msg: `Datei zu groß (${kb}, Platz ${Math.round(FWI.max / 1024)} kB)`};
    const m = new TextDecoder('latin1').decode(u).match(/CAMPER-FW:([A-Za-z0-9_.-]{1,24}):([A-Za-z0-9_.-]{1,24}):([A-Za-z0-9_.-]{1,24});/);
    if (!m) return {ok: false, force: true, msg: 'Keine Gerätekennung in der Datei: fremde Firmware oder eine Version vor der Kennung. Nur hochladen, wenn sicher ist, dass sie für dieses Gerät ist.'};
    if (FWI && m[1] !== FWI.dev) return {ok: false, msg: `Firmware für „${m[1]}“, dieses Gerät ist „${FWI.dev}“`};
    if (FWI && m[3] !== FWI.chip) return {ok: false, msg: `Firmware für ${m[3]}, dieses Gerät ist ein ${FWI.chip}${hwHint()}`};
    const c = FWI ? verCmp(m[2], FWI.ver) : 1;
    return {ok: true, kind: 'fw', warn: c <= 0, msg: `Firmware ${m[1]} v${m[2]} (${kb}) – ` +
      (c > 0 ? 'neuer als die installierte' : c === 0 ? 'gleiche Version wie installiert' : 'ÄLTER als die installierte (Downgrade)')};
  }

  function showInfo() {
    const cur = $('fwu-cur');
    if (cur && FWI) cur.innerHTML = `Installiert: <b>${esc(FWI.dev)} v${esc(FWI.ver)}</b> · ${FW_CHIPS[FWI.chip_id] || esc(FWI.chip)}` +
      (FWI.max ? ` · Platz ${Math.round(FWI.max / 1024)} kB` : '');
    const hint = $('vnc-uhint');
    if (hint && FWI) {
      const dev = esc(FWI.dev || 'gerät'), hw = esc(FWI.hw || '<board>');
      hint.innerHTML = `Firmware: die <code>${dev}-node-${hw}-v….bin</code> aus dem Release oder aus der Arduino IDE ` +
        `(Sketch → Kompilierte Binärdatei exportieren → <code>….ino.bin</code>, <b>nicht</b> die <code>.merged.bin</code>), danach Neustart.` +
        (cfg.web ? `<br>Web-Oberfläche: die <code>${dev}-node-web-v….html.gz</code> aus dem Release oder <code>data/index.html.gz</code>, ohne Neustart.` : '') +
        (cfg.fs ? ' Ein LittleFS-Abbild (Dateiname mit „littlefs“) wird ebenfalls erkannt.' : '') +
        ' Die Datei wird vor dem Hochladen geprüft; die Einstellungen bleiben erhalten.';
    }
    // Event log: every .fwu-log on the page (the display shows it on its system page)
    document.querySelectorAll('.fwu-log').forEach((box) => {
      if (!LOG) return;
      box.innerHTML = LOG.length ? LOG.map((e) => {
        const when = e.b === 0 ? 'vor ' + ago(Math.max(0, UP - e.t)) : (e.b === 1 ? 'vorheriger Start' : e.b + ' Starts zuvor') + ', nach ' + ago(e.t);
        const bad = /abgelehnt|fehlgeschlagen|nicht gespeichert|zurück auf|Absturz|Watchdog|Spannungseinbruch/.test(e.m);
        return `<div class="fwu-ev${bad ? ' bad' : ''}"><span>${esc(when)}</span>${esc(e.m)}</div>`;
      }).join('') : '<div class="fwu-ev">Noch keine Einträge.</div>';
    });
  }

  async function load() {
    try {
      const r = await fetch('/api/system', {cache: 'no-store'});
      if (!r.ok) return;
      const d = await r.json();
      FWI = d.fw || null;
      LOG = d.log || [];
      UP = d.up || 0;
      cfg = Object.assign({}, DEF, FWI && DEVCFG[FWI.dev] || {});
      const inp = $('vnc-ufile');
      if (inp) inp.accept = cfg.web ? '.bin,.gz' : '.bin';
      showInfo();
      CLK = d.clock ? Object.assign({at: Date.now()}, d.clock) : null;
      showClock();
      // No valid time on the device (no internet, no NTP at the router): take the browser's once
      if (CLK && !CLK.ok && !clkAuto) {
        clkAuto = true;
        setClock(Date.now(), false, 'Uhrzeit automatisch vom Browser übernommen (das Gerät hatte keine).');
      }
    } catch (e) {}
  }

  // ---- Clock card ----
  const SRC = {ntp: 'über das Internet (NTP)', browser: 'vom Browser übernommen', manual: 'von Hand gesetzt'};
  const two = (n) => String(n).padStart(2, '0');
  const fmtTime = (d) => ['So', 'Mo', 'Di', 'Mi', 'Do', 'Fr', 'Sa'][d.getDay()] + ' ' + two(d.getDate()) + '.' + two(d.getMonth() + 1) + '.' +
    d.getFullYear() + ' ' + two(d.getHours()) + ':' + two(d.getMinutes());
  function showClock() {
    const c = $('vn-clock');
    if (!c) return;
    c.style.display = CLK ? '' : 'none';
    if (!CLK) return;
    const cur = $('vnc-ccur');
    if (!CLK.ok) {
      cur.innerHTML = '<span class="warn">Keine Uhrzeit gesetzt</span>';
    } else {
      const dev = new Date((CLK.t * 1000) + (Date.now() - CLK.at));
      const off = Math.round((dev.getTime() - Date.now()) / 60000);
      cur.innerHTML = 'Gerät: <b>' + fmtTime(dev) + '</b> · ' + esc(SRC[CLK.src] || 'Quelle unbekannt') +
        (Math.abs(off) >= 2 ? ' · <span class="warn">weicht ' + Math.abs(off) + ' min vom Browser ab</span>' : '');
    }
    const inp = $('vnc-cin');
    if (inp && !inp.value) {
      const n = new Date();
      inp.value = n.getFullYear() + '-' + two(n.getMonth() + 1) + '-' + two(n.getDate()) + 'T' + two(n.getHours()) + ':' + two(n.getMinutes());
    }
  }
  async function setClock(ms, manual, okText) {
    const out = $('vnc-cmsg');
    try {
      const r = await fetch('/api/time?t=' + Math.round(ms / 1000) + (manual ? '&m=1' : ''), {method: 'POST'});
      let j = {};
      try { j = await r.json(); } catch (e) {}
      if (!r.ok || j.ok === false) throw new Error(j.error || j.msg || ('HTTP ' + r.status));
      if (out) out.textContent = okText;
      load();
    } catch (e) { if (out) out.textContent = 'Nicht gesetzt: ' + e.message; }
  }

  function setChk(text, cls) {
    const chk = $('fwu-chk');
    if (!chk) return;
    chk.textContent = text;
    chk.className = 'vnc-chk ' + (cls || '');
  }

  async function chosen() {
    const f = $('vnc-ufile').files[0], btn = $('fwu-btn'), lab = $('vnc-ulabel');
    pick = null;
    $('vnc-uforce-row').style.display = 'none';
    $('vnc-uforce').checked = false;
    lab.classList.toggle('has', !!f);
    lab.querySelector('.nm').textContent = f ? f.name : (cfg.web ? '.bin oder .html.gz wählen …' : '.bin wählen …');
    if (!f) { setChk(''); btn.disabled = true; return; }
    setChk('Prüfe …');
    const r = await inspect(f);
    pick = r;
    setChk((r.ok ? (r.warn ? '⚠ ' : '✓ ') : '✕ ') + r.msg, r.ok ? (r.warn ? 'warn' : 'ok') : 'bad');
    if (r.force) $('vnc-uforce-row').style.display = '';
    btn.disabled = !r.ok;
  }

  function upload() {
    const f = $('vnc-ufile').files[0], btn = $('fwu-btn'), bar = $('vnc-ubar');
    if (!f || !pick) return setChk('Erst eine Datei wählen', 'bad');
    if (pick.warn && pick.kind === 'fw' && !confirm(pick.msg + '\nTrotzdem installieren?')) return;
    const force = !pick.ok && pick.force && $('vnc-uforce').checked;
    const fd = new FormData();
    fd.append(cfg.field, f);
    const x = new XMLHttpRequest();
    bar.style.display = '';
    bar.firstChild.style.width = '0%';
    btn.disabled = true;
    x.upload.onprogress = (e) => {
      if (!e.lengthComputable) return;
      const p = Math.round(e.loaded / e.total * 100);
      bar.firstChild.style.width = p + '%';
      setChk(p < 100 ? `Übertrage … ${p} %` : 'Wird geschrieben …');
    };
    x.onload = () => {
      let j = null;
      try { j = JSON.parse(x.responseText); } catch (e) {}
      const text = (j && j.msg) || x.responseText || ('Fehler HTTP ' + x.status);
      const ok = x.status === 200 && (j ? j.ok !== false : /^OK/i.test(x.responseText) || x.responseText === '');
      if (ok && /^OK web/.test(x.responseText)) { setChk('✓ Web-Oberfläche ersetzt – Seite lädt neu …', 'ok'); setTimeout(() => location.reload(), 1500); return; }
      if (ok) { setChk('✓ Update ok – das Gerät startet neu, die Seite lädt gleich neu …', 'ok'); setTimeout(() => location.reload(), 15000); return; }
      bar.style.display = 'none';
      setChk('✕ ' + text, 'bad');
      btn.disabled = false;
      load();
    };
    x.onerror = () => { bar.style.display = 'none'; setChk('✕ Verbindung abgebrochen', 'bad'); btn.disabled = false; load(); };
    x.open('POST', cfg.upd + (force ? '?force=1' : ''));
    x.send(fd);
  }

  async function devAction(url, question, msg) {
    if (!confirm(question)) return;
    const out = $('vnc-dmsg');
    try {
      await fetch(url, {method: 'POST'});
      out.textContent = msg;
      setTimeout(() => location.reload(), 10000);
    } catch (e) { out.textContent = 'Keine Verbindung zum Gerät'; }
  }

  function card(title) {
    const c = document.createElement('div');
    c.className = 'card vnc';
    c.innerHTML = `<h3 class="vnc-t">${title}</h3>`;
    return c;
  }

  function build(box) {
    box.dataset.vnBuilt = '1';
    if (!$('vn-sys-css')) {
      const st = document.createElement('style');
      st.id = 'vn-sys-css';
      st.textContent = CSS;
      document.head.appendChild(st);
    }
    const upd = card('Firmware- &amp; Web-Update');
    upd.id = 'vn-update';
    upd.insertAdjacentHTML('beforeend',
      '<div class="vnc-cur" id="fwu-cur">Installiert: …</div>' +
      '<div class="vnc-hint" id="vnc-uhint">Die Datei wird vor dem Hochladen geprüft.</div>' +
      '<label class="vnc-file" id="vnc-ulabel">' + ICON.file + '<span class="nm">Datei wählen …</span>' +
      '<span class="pick">Durchsuchen</span><input type="file" id="vnc-ufile" accept=".bin,.gz"></label>' +
      '<div class="vnc-chk" id="fwu-chk"></div>' +
      '<label class="vnc-opt" id="vnc-uforce-row" style="display:none"><input type="checkbox" id="vnc-uforce"> Ohne Kennung hochladen</label>' +
      '<div class="vnc-bar" id="vnc-ubar" style="display:none"><i></i></div>' +
      '<div class="vnc-row"><button type="button" class="vnc-b p" id="fwu-btn" disabled>' + ICON.up + 'Hochladen</button></div>');
    box.appendChild(upd);

    if (box.dataset.log !== '0') {
      const log = card('Ereignisse');
      log.insertAdjacentHTML('beforeend', '<div class="fwu-log">Lade …</div>' +
        '<div class="vnc-hint" style="margin:8px 0 0">Bleibt bei Neustart, Absturz und Update erhalten, nicht bei Stromausfall.</div>');
      box.appendChild(log);
    }

    const clk = card('Uhrzeit');
    clk.id = 'vn-clock';
    clk.style.display = 'none';
    clk.insertAdjacentHTML('beforeend',
      '<div class="vnc-cur" id="vnc-ccur">…</div>' +
      '<div class="vnc-hint">Ohne Internet übernimmt diese Seite die Uhrzeit automatisch vom Handy bzw. Rechner, sobald das Gerät keine hat. ' +
      'Die Zeit bleibt bei einem Neustart erhalten, nicht bei Stromausfall; ein späterer Abgleich über das Internet ersetzt sie.</div>' +
      '<div class="vnc-row" style="margin-bottom:10px"><button type="button" class="vnc-b p" id="vnc-cnow">Uhrzeit vom Handy übernehmen</button></div>' +
      '<div class="vnc-row"><input type="datetime-local" class="vnc-in" id="vnc-cin">' +
      '<button type="button" class="vnc-b" id="vnc-cset" style="flex:0 1 auto">Von Hand setzen</button></div>' +
      '<div class="vnc-msg" id="vnc-cmsg"></div>');
    box.appendChild(clk);

    const dev = card('Gerät');
    dev.insertAdjacentHTML('beforeend',
      '<div class="vnc-row"><button type="button" class="vnc-b" id="vnc-restart">' + ICON.restart + 'Neustart</button>' +
      '<button type="button" class="vnc-b d" id="vnc-factory" style="display:none">Werkseinstellungen</button></div>' +
      '<div class="vnc-msg" id="vnc-dmsg"></div>');
    box.appendChild(dev);

    $('vnc-ufile').addEventListener('change', chosen);
    $('vnc-uforce').addEventListener('change', () => {
      $('fwu-btn').disabled = !(pick && (pick.ok || (pick.force && $('vnc-uforce').checked)));
    });
    $('fwu-btn').addEventListener('click', upload);
    $('vnc-cnow').addEventListener('click', () => setClock(Date.now(), false, 'Uhrzeit vom Browser übernommen.'));
    $('vnc-cset').addEventListener('click', () => {
      const v = $('vnc-cin').value, t = v ? new Date(v).getTime() : NaN;
      if (isNaN(t)) { $('vnc-cmsg').textContent = 'Erst Datum und Uhrzeit eintragen'; return; }
      setClock(t, true, 'Uhrzeit gesetzt.');
    });
    $('vnc-restart').addEventListener('click', () => devAction(cfg.restart, 'Gerät neu starten?', 'Startet neu – die Seite lädt gleich neu …'));
    $('vnc-factory').addEventListener('click', () => devAction(cfg.factory,
      'Alle Einstellungen löschen?\nDas Gerät startet danach mit den Werkseinstellungen und öffnet sein Setup-WLAN.',
      'Zurückgesetzt – das Gerät startet neu und öffnet sein Setup-WLAN.'));
    load().then(() => {
      $('vnc-factory').style.display = cfg.factory ? '' : 'none';
      chosen();
    });
  }

  function place() {
    const box = $('vn-system');
    if (box && !box.dataset.vnBuilt) build(box);
    // "Über VanNode" (vannode-about.js) puts itself below the update card: move it to the end
    const about = $('vn-about');
    if (box && about && box.lastElementChild !== about && box.contains($('vn-update'))) box.appendChild(about);
    else if (!box && LOG === null && document.querySelector('.fwu-log')) { LOG = []; load(); }
  }
  window.vnSystemRefresh = load;   // pages may call it, e.g. after their own actions
  // The page may build the container later or again (tabs): watch for it
  new MutationObserver(place).observe(document.documentElement, {childList: true, subtree: true});
  document.addEventListener('DOMContentLoaded', place);
  // Once at the start on every tab: a device without a valid clock gets the browser's (see load)
  setTimeout(() => { if (FWI === null) load(); }, 1500);
  setInterval(() => { if ($('vn-system') || document.querySelector('.fwu-log')) load(); }, 15000);
})();
