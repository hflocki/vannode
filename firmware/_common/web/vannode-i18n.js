// Language of the device web pages (shared copy from firmware/_common/web, see sync_common.py).
// The pages are written in German. When the firmware reports another language in
// /api/system (fw.lang, set with UI_LANG in config_local.h), this block replaces the German
// texts in the browser, also texts that arrive later (live values, messages from the device).
// Lookup: whole text, then each part around " · " / ": " / " – ", then longer phrases inside
// the text, then patterns with * (any text). Not found = stays German. Nothing is sent back to
// the device, so settings and MQTT stay as they are.
// Each page adds its own words with vnI18nAdd('en', {...}); the shared cards are below.
var VN_I18N = VN_I18N || {};
function vnI18nAdd(lang, words) {
  if (!VN_I18N) VN_I18N = {};   // called by a page before this block ran
  var d = VN_I18N[lang] || (VN_I18N[lang] = {});
  for (var k in words) d[k] = words[k];
}
var vnLang = (function () {
  var lang = 'de', dict = null, phrases = [], patterns = [], busy = false;
  try { lang = localStorage.getItem('vnLang') || 'de'; } catch (e) {}
  var SKIP = {SCRIPT: 1, STYLE: 1, TEXTAREA: 1, CODE: 1, PRE: 1};
  var ATTRS = ['placeholder', 'title', 'aria-label'];

  function prepare() {
    dict = VN_I18N[lang] || null;
    phrases = []; patterns = [];
    if (!dict) return;
    for (var k in dict) {
      if (k.indexOf('*') >= 0) {
        var re = '^' + k.replace(/[.+?^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '(.+?)') + '$';
        patterns.push([new RegExp(re), dict[k]]);
      } else if (k.length >= 12 && k.indexOf(' ') > 0) {
        phrases.push(k);
      }
    }
    phrases.sort(function (a, b) { return b.length - a.length; });
  }
  function word(t) {
    if (Object.prototype.hasOwnProperty.call(dict, t)) return dict[t];
    var c = t.replace(/[:…]+$/, '');   // "Label:" / "Lade …"
    if (c !== t && Object.prototype.hasOwnProperty.call(dict, c.trim())) return dict[c.trim()] + t.slice(c.length);
    return null;
  }
  function tr(s) {
    if (!dict || !s) return s;
    var t = s.trim();
    if (!t || !/[A-Za-zÄÖÜäöüß]/.test(t)) return s;
    var lead = s.slice(0, s.indexOf(t)), trail = s.slice(s.indexOf(t) + t.length);
    var w = word(t);
    if (w === null) {
      for (var i = 0; i < patterns.length; i++) {
        var m = patterns[i][0].exec(t);
        if (m) { var n = 1, out = patterns[i][1]; w = out.replace(/\*/g, function () { return tr(m[n++] || ''); }); break; }
      }
    }
    if (w === null && /( · | – |: |\|)/.test(t)) {
      var parts = t.split(/( · | – |: |\|)/), changed = false;
      for (var j = 0; j < parts.length; j += 2) {
        var p = word(parts[j].trim());
        if (p !== null) { parts[j] = parts[j].replace(parts[j].trim(), p); changed = true; }
      }
      if (changed) w = parts.join('');
    }
    if (w === null) {
      var x = t, hit = false;
      for (var k = 0; k < phrases.length; k++) {
        if (x.indexOf(phrases[k]) >= 0) { x = x.split(phrases[k]).join(dict[phrases[k]]); hit = true; }
      }
      if (hit) w = x;
    }
    return w === null ? s : lead + w + trail;
  }
  function walk(node) {
    if (node.nodeType === 3) {
      var p = node.parentNode;
      if (p && !SKIP[p.nodeName] && !(p.closest && p.closest('[translate=no]'))) {
        var v = tr(node.nodeValue);
        if (v !== node.nodeValue) node.nodeValue = v;
      }
      return;
    }
    if (node.nodeType !== 1 || SKIP[node.nodeName] || node.getAttribute('translate') === 'no') return;
    for (var a = 0; a < ATTRS.length; a++) {
      var at = node.getAttribute(ATTRS[a]);
      if (at) { var t = tr(at); if (t !== at) node.setAttribute(ATTRS[a], t); }
    }
    if (node.nodeName === 'INPUT' && (node.type === 'button' || node.type === 'submit') && node.value) {
      var bv = tr(node.value); if (bv !== node.value) node.value = bv;
    }
    for (var c = node.firstChild; c; c = c.nextSibling) walk(c);
  }
  function start() {
    prepare();
    if (!dict) return;
    document.documentElement.lang = lang;
    walk(document.body);
    new MutationObserver(function (list) {
      if (busy) return;
      busy = true;
      try {
        list.forEach(function (m) {
          if (m.type === 'characterData') walk(m.target);
          else if (m.type === 'attributes') walk(m.target);
          else m.addedNodes.forEach(walk);
        });
      } finally { busy = false; }
    }).observe(document.body, {childList: true, subtree: true, characterData: true, attributes: true, attributeFilter: ATTRS});
    ['alert', 'confirm', 'prompt'].forEach(function (f) {
      var orig = window[f];
      window[f] = function (msg) { var a = [].slice.call(arguments); a[0] = tr(String(msg == null ? '' : msg)); return orig.apply(window, a); };
    });
  }
  function set(l) {
    l = l || 'de';
    var old = lang;
    lang = l;
    try { localStorage.setItem('vnLang', l); } catch (e) {}
    if (l !== old) location.reload();   // back to German or another language: start clean
  }
  // Language from the device (cached, so the next load translates right away)
  function init() {
    if (lang !== 'de') start();
    fetch('/api/system', {cache: 'no-store'}).then(function (r) { return r.ok ? r.json() : null; })
      .then(function (d) { if (d && d.fw) set(d.fw.lang || 'de'); }).catch(function () {});
  }
  // after all scripts of the page (their words are added further down)
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else setTimeout(init, 0);
  return {t: tr, lang: function () { return lang; }};
})();
// Shared cards: update, backup, events, device, clock, about (vannode-system.js, -backup.js, -about.js).
// The license and liability texts of the about card stay German on purpose (original wording).
vnI18nAdd('en', {
  'Firmware- & Web-Update': 'Firmware & web update', 'Firmware': 'Firmware', 'Hochladen': 'Upload', 'Durchsuchen': 'Browse',
  'Datei wählen …': 'Choose file …', '.bin wählen …': 'Choose .bin …', '.bin oder .html.gz wählen …': 'Choose .bin or .html.gz …',
  'Die Datei wird vor dem Hochladen geprüft.': 'The file is checked before uploading.',
  'Nur .bin (Firmware)': 'Only .bin (firmware)', 'Nur .bin (Firmware) oder .gz (Web-Oberfläche)': 'Only .bin (firmware) or .gz (web interface)',
  'Installiert': 'Installed', 'Installiert: …': 'Installed: …', 'Prüfe …': 'Checking …', 'Übertrage …': 'Transferring …', 'Wird geschrieben …': 'Writing …',
  'Keine ESP-Firmware': 'Not an ESP firmware', 'Keine gültige Datei': 'Not a valid file', 'Erst eine Datei wählen': 'Choose a file first',
  'Ohne Kennung hochladen': 'Upload without ID', 'Gerät beschäftigt': 'Device busy', 'Keine Verbindung zum Gerät': 'No connection to the device',
  'Keine Gerätekennung in der Datei: fremde Firmware oder eine Version vor der Kennung. Nur hochladen, wenn sicher ist, dass sie für dieses Gerät ist.':
    'No device ID in the file: foreign firmware or a version from before the ID. Only upload it if you are sure it is for this device.',
  'Das ist die .merged.bin (komplettes Flash-Abbild für USB). Bitte die .bin ohne „merged“ nehmen.': 'This is the .merged.bin (complete flash image for USB). Please take the .bin without "merged".',
  'Die Web-Oberfläche steckt bei diesem Gerät in der Firmware – nur die .bin hochladen': 'On this device the web interface is part of the firmware – only upload the .bin',
  'Ein LittleFS-Abbild (Dateiname mit „littlefs“) wird ebenfalls erkannt.': 'A LittleFS image (file name with "littlefs") is recognized as well.',
  'gleiche Version wie installiert': 'same version as installed', 'ÄLTER als die installierte (Downgrade)': 'OLDER than the installed one (downgrade)',
  'Trotzdem installieren?': 'Install anyway?', 'Das Gerät startet danach eventuell neu.': 'The device may restart afterwards.',
  '✓ Update ok – das Gerät startet neu, die Seite lädt gleich neu …': '✓ Update ok – the device restarts, the page reloads in a moment …',
  '✓ Web-Oberfläche ersetzt – Seite lädt neu …': '✓ Web interface replaced – page reloads …', '✕ Verbindung abgebrochen': '✕ Connection lost',
  'Fehler HTTP': 'HTTP error', 'Fehler bei': 'Error at', 'Datei zu groß': 'File too big', 'Firmware für': 'Firmware for',
  'aus dem Release oder aus der Arduino IDE': 'from the release or from the Arduino IDE', 'aus dem Release oder': 'from the release or',
  'Sketch → Kompilierte Binärdatei exportieren →': 'Sketch → Export compiled binary →', 'ersetzt nur die Seite, ohne Neustart': 'replaces only the page, no restart',
  'ersetzt die Web-Oberfläche, danach Neustart': 'replaces the web interface, then restart', 'danach Neustart.': 'then restart.',
  'Web-Oberfläche': 'Web interface', 'Firmware: die': 'Firmware: the', 'Web-Oberfläche: die': 'Web interface: the', 'Dateisystem-Abbild': 'File system image',
  'passende Datei': 'matching file', 'dieses Gerät ist ein': 'this device is a', 'Quelle unbekannt': 'Source unknown',
  'Einstellungen sichern': 'Back up settings', 'Sichern': 'Back up', 'Einspielen': 'Restore', 'Sicherung zum Einspielen wählen …': 'Choose a backup to restore …',
  'Erst eine Sicherungsdatei wählen': 'Choose a backup file first', 'Keine VanNode-Sicherung': 'Not a VanNode backup',
  'auch Netzwerk übernehmen': 'also restore network', 'Auch WLAN, IP und Gerätename': 'Also WiFi, IP and device name', 'sowie MQTT-Server': 'and MQTT server',
  'Alle Einstellungen und Geräte': 'All settings and devices', 'Alle Einstellungen': 'All settings',
  'Alle Einstellungen, Seiten, Kacheln, Automationen und Sensoren': 'All settings, pages, tiles, automations and sensors',
  'als Datei. Passwörter und die Anmeldung sind nicht enthalten, beim Einspielen bleiben die vorhandenen erhalten.': 'as a file. Passwords and the login are not included; the existing ones are kept when restoring.',
  'Einstellungen aus der Sicherung vom': 'Settings from the backup of', 'übernehmen?': 'restore?', 'übernehmen': 'restore',
  'WLAN, IP und Gerätename werden mit übernommen.': 'WiFi, IP and device name are restored too.', 'Die Sicherung gehört zu „': 'The backup belongs to "',
  'Einstellungen übernommen': 'Settings restored', 'Seiten und Kacheln übernommen': 'Pages and tiles restored', 'Automationen übernommen': 'Automations restored',
  'Sensoren übernommen': 'Sensors restored', 'Übernommen:': 'Restored:', 'Nicht angelegt:': 'Not created:', 'Nicht gesetzt:': 'Not set:',
  'Übersprungen (Typ fehlt in dieser Firmware):': 'Skipped (type missing in this firmware):',
  'Fertig. Geänderte Geräte gelten nach einem Neustart (Karte Gerät → Neustart).': 'Done. Changed devices apply after a restart (card Device → Restart).',
  'Fertig – Seite lädt gleich neu.': 'Done – page reloads in a moment.', 'Sichern fehlgeschlagen:': 'Backup failed:', 'Einspielen fehlgeschlagen:': 'Restore failed:',
  'Ereignisse': 'Events', 'Noch keine Einträge.': 'No entries yet.', 'Bleibt bei Neustart, Absturz und Update erhalten, nicht bei Stromausfall.': 'Kept across restarts, crashes and updates, not across power loss.',
  'vorheriger Start': 'previous start', 'Starts zuvor': 'starts before',
  'Gerät': 'Device', 'Neustart': 'Restart', 'Werkseinstellungen': 'Factory reset', 'Gerät neu starten?': 'Restart the device?',
  'Alle Einstellungen löschen?\nDas Gerät startet danach mit den Werkseinstellungen und öffnet sein Setup-WLAN.': 'Delete all settings?\nThe device then starts with factory settings and opens its setup WiFi.',
  'Startet neu – die Seite lädt gleich neu …': 'Restarting – the page reloads in a moment …', 'Zurückgesetzt – das Gerät startet neu und öffnet sein Setup-WLAN.': 'Reset – the device restarts and opens its setup WiFi.',
  'Uhrzeit': 'Clock', 'Keine Uhrzeit gesetzt': 'No time set', 'Uhrzeit vom Handy übernehmen': 'Take time from the phone', 'Von Hand setzen': 'Set manually',
  'Erst Datum und Uhrzeit eintragen': 'Enter date and time first', 'Uhrzeit gesetzt.': 'Time set.', 'Uhrzeit vom Browser übernommen.': 'Time taken from the browser.',
  'Uhrzeit automatisch vom Browser übernommen (das Gerät hatte keine).': 'Time taken from the browser automatically (the device had none).',
  'Ohne Internet übernimmt diese Seite die Uhrzeit automatisch vom Handy bzw. Rechner, sobald das Gerät keine hat.': 'Without internet this page takes the time from the phone or computer as soon as the device has none.',
  'über das Internet (NTP)': 'from the internet (NTP)', 'vom Browser übernommen': 'taken from the browser', 'von Hand gesetzt': 'set manually', 'min vom Browser ab': 'min off the browser',
  'Über VanNode': 'About VanNode', 'Über': 'About', 'Teil von VanNode – Camper-Elektronik über MQTT': 'Part of VanNode – camper electronics over MQTT',
  'Lizenz:': 'License:', 'Lizenztext (GPL-3.0)': 'License text (GPL-3.0)', 'Hinweise und fremde Teile': 'Notices and third-party parts', 'Ohne Gewähr.': 'No warranty.',
  'Platz': 'Space', 'Vorlage': 'Template', 'Lade …': 'Loading …', 'Gespeichert:': 'Saved:',
  // node page basics shared by all nodes
  'Speichern': 'Save', 'Gespeichert': 'Saved', 'Fehler': 'Error', 'Fehler:': 'Error:', 'Einstellungen': 'Settings', 'System': 'System', 'Live': 'Live',
  'Benutzer': 'User', 'Passwort': 'Password', 'Web-Login': 'Web login', 'Gerätename': 'Device name', 'Laufzeit': 'Uptime', 'Freier Speicher': 'Free memory',
  'WLAN-Name (SSID)': 'WiFi name (SSID)', 'WLAN-Passwort': 'WiFi password', 'WLAN-Signal': 'WiFi signal', 'WLAN-Abbrüche': 'WiFi drops',
  'Eigener Access Point': 'Own access point', 'Access Point immer an': 'Access point always on', 'AP-Passwort': 'AP password', 'Sendeleistung': 'Transmit power',
  'MQTT verbunden': 'MQTT connected', 'MQTT getrennt': 'MQTT disconnected', 'Topic-Präfix': 'Topic prefix', 'Letzter Neustart': 'Last restart',
  'Gesetzt – leer lassen zum Beibehalten': 'Set – leave empty to keep', 'Jetzt neu starten': 'Restart now', 'Neustart…': 'Restarting…', 'Seite neu laden': 'Reload page',
  'Start v*, Grund: *': 'Start v*, reason: *', 'WLAN verbunden (*)': 'WiFi connected (*)', 'WLAN getrennt': 'WiFi disconnected',
  'Neustart über die Web-Oberfläche': 'Restart from the web interface', 'Werkseinstellungen über die Web-Oberfläche': 'Factory reset from the web interface',
  'Neustart (Software)': 'Restart (software)', 'Absturz': 'Crash', 'Brownout (Spannung zu niedrig)': 'Brownout (voltage too low)', 'Deep Sleep': 'Deep sleep',
  'Watchdog (Hardware)': 'Watchdog (hardware)', 'Watchdog (Interrupt)': 'Watchdog (interrupt)', 'Watchdog (Software)': 'Watchdog (software)', 'Watchdog (Task)': 'Watchdog (task)',
  'Einstellungen gespeichert': 'Settings saved', 'Web-Login über USB auf Werks-Login zurückgesetzt': 'Web login reset to the factory login over USB',
  'Update über den IDE-Netzwerkport ok, Neustart': 'Update over the IDE network port ok, restart', 'Update abgelehnt:': 'Update refused:',
  'Wenig Speicher: frei *, größter Block *, Minimum * Bytes': 'Low memory: free *, largest block *, minimum * bytes',
  "Die Zeit bleibt bei einem Neustart erhalten, nicht bei Stromausfall; ein späterer Abgleich über das Internet ersetzt sie.": "The time is kept across a restart, not across power loss; a later sync over the internet replaces it.",
  "Auch WLAN, IP und Gerätename sowie MQTT-Server übernehmen": "Also restore WiFi, IP, device name and MQTT server",
  "ohne Neustart. Die Datei wird vor dem Hochladen geprüft; die Einstellungen bleiben erhalten.": "no restart. The file is checked before uploading; the settings are kept.",
  "das ist die .merged.bin (komplettes Flash-Abbild für USB) – bitte die .bin ohne „merged“ nehmen": "this is the .merged.bin (complete flash image for USB) – please take the .bin without \"merged\"",
  "Update auf v* ok (* kB), Neustart": "Update to v* ok (* kB), restart",
  "Firmware-Upload gestartet (* kB)": "Firmware upload started (* kB)",
  "Einschalten": "Power on",
  "Mindestens 8 Zeichen, leer = unverändert": "At least 8 characters, empty = unchanged",
  "Die Anmeldung ist immer an (ab Werk admin / admin). Leer lassen = bleibt. Login vergessen: über USB den Befehl noauth senden, dann gilt wieder der Werks-Login.": "The login is always on (factory admin / admin). Leave empty = kept. Forgot the login: send the command noauth over USB, then the factory login applies again.",
  "Aus = AP nur als Rückfall, wenn das WLAN fehlt": "Off = AP only as a fallback when WiFi is missing",
  "aus": "off",
  "an": "on",
  "zu": "closed",
  "offen": "open",
  "ja": "yes",
  "nein": "no",
  "keine": "none",
  "nichts": "nothing",
  "nicht": "not",
  "läuft": "running"
});
