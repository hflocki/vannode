// Single page web UI (served from flash, no filesystem upload needed).
#pragma once

static const char INDEX_HTML[] = R"rawliteral(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Truma Heizung</title>
<style>
:root{--bg:#f3f1ec;--card:#fff;--ink:#1d1f21;--mute:#6b6f74;--line:#e2ded6;--accent:#d2561e;--accent2:#1f6f8b;--ok:#2e7d4f;--bad:#b3261e}
@media (prefers-color-scheme:dark){:root{--bg:#141516;--card:#1e2022;--ink:#ecebe8;--mute:#9a9ea3;--line:#2e3134;--accent:#ff7a3d;--accent2:#5fb3d0;--ok:#5cc08a;--bad:#ff6b5e}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{display:flex;align-items:center;justify-content:space-between;padding:14px 16px;max-width:760px;margin:0 auto}
header h1{font-size:18px;margin:0;letter-spacing:.2px}
.pills{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.pill{font-size:12px;padding:3px 9px;border-radius:99px;border:1px solid var(--line);color:var(--mute)}
.pill.ok{color:var(--ok);border-color:var(--ok)}.pill.bad{color:var(--bad);border-color:var(--bad)}
nav{display:flex;gap:4px;max-width:760px;margin:0 auto;padding:0 16px}
nav button{flex:1;border:0;background:transparent;color:var(--mute);padding:10px;border-bottom:2px solid var(--line);font:inherit;cursor:pointer}
nav button.on{color:var(--ink);border-color:var(--accent)}
main{max-width:760px;margin:0 auto;padding:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:14px}
.card h2{font-size:13px;text-transform:uppercase;letter-spacing:.8px;color:var(--mute);margin:0 0 12px}
.big{display:flex;align-items:flex-end;gap:18px;flex-wrap:wrap}
.temp{font-size:52px;font-weight:600;line-height:1}
.temp small{font-size:20px;color:var(--mute);font-weight:400}
.sub{color:var(--mute)}
.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin-top:12px}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:10px;overflow:hidden}
.seg button{border:0;background:transparent;color:var(--ink);padding:9px 14px;font:inherit;cursor:pointer;border-right:1px solid var(--line)}
.seg button:last-child{border-right:0}
.seg button.on{background:var(--accent);color:#fff}
.btn{border:1px solid var(--line);background:var(--card);color:var(--ink);padding:9px 14px;border-radius:10px;font:inherit;cursor:pointer}
.btn.primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn:disabled{opacity:.45;cursor:not-allowed}
.stepper{display:inline-flex;align-items:center;gap:10px}
.stepper .btn{width:42px;padding:9px 0;font-size:18px}
.stepper b{font-size:22px;min-width:64px;text-align:center}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}
.kv{border:1px solid var(--line);border-radius:10px;padding:10px}
.kv span{display:block;font-size:12px;color:var(--mute)}
.kv b{font-size:16px;font-weight:600}
label{display:block;font-size:13px;color:var(--mute);margin:10px 0 4px}
input,select{width:100%;padding:9px 10px;border-radius:9px;border:1px solid var(--line);background:var(--bg);color:var(--ink);font:inherit}
.cols{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
@media (max-width:520px){.cols{grid-template-columns:1fr}.temp{font-size:44px}}
.check{display:flex;gap:8px;align-items:center;margin-top:12px;color:var(--ink)}
.check input{width:auto}
pre{white-space:pre-wrap;word-break:break-word;font-size:12px;max-height:340px;overflow:auto;background:var(--bg);padding:10px;border-radius:9px;margin:0}
.hint{font-size:13px;color:var(--mute);margin-top:10px}
.warn{border-color:var(--bad)}
.hide{display:none}
#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--ink);color:var(--bg);padding:8px 14px;border-radius:9px;font-size:14px;opacity:0;transition:opacity .2s;pointer-events:none}
#toast.show{opacity:1}
</style>
</head>
<body>
<header><h1>Truma Heizung</h1><div class="pills"><span id="p-cp" class="pill">CP Plus</span><span id="p-mqtt" class="pill">MQTT</span></div></header>
<nav><button class="on" data-tab="heat">Heizung</button><button data-tab="sys">System</button><button data-tab="cfg">Einstellungen</button></nav>
<main>
<section id="tab-heat">
  <div id="nolink" class="card warn hide">
    <h2>Keine Verbindung zum CP Plus</h2>
    <div id="nolink-text" class="sub"></div>
  </div>
  <div class="card">
    <h2>Raum</h2>
    <div class="big">
      <div class="temp"><span id="room">--</span><small> °C</small></div>
      <div class="sub">Ziel <b id="target">--</b> · <span id="status">--</span></div>
    </div>
    <div class="row">
      <div class="seg" id="seg-room"><button data-v="0">Aus</button><button data-v="1">Heizen</button></div>
      <div class="stepper"><button class="btn" id="minus">−</button><b id="setp">--</b><button class="btn" id="plus">+</button></div>
    </div>
    <div class="row"><span class="sub">Lüfter</span><div class="seg" id="seg-fan"><button data-v="eco">Eco</button><button data-v="high">High</button><button data-v="boost" class="hide" id="fan-boost">Boost</button></div></div>
  </div>
  <div class="card">
    <h2>Warmwasser</h2>
    <div class="big"><div class="temp"><span id="water">--</span><small> °C</small></div></div>
    <div class="row"><div class="seg" id="seg-water"><button data-v="off">Aus</button><button data-v="eco">Eco 40°</button><button data-v="high">Hot 60°</button><button data-v="boost" class="hide" id="water-boost">Boost</button></div></div>
  </div>
  <div class="card">
    <h2>Timer</h2>
    <div class="sub" id="timer-text">--</div>
    <div class="grid">
      <label>Start<input type="time" id="t-start"></label>
      <label>Ende<input type="time" id="t-stop"></label>
      <label>Temperatur °C<input type="number" id="t-temp" min="5" max="30" step="1"></label>
      <label>Lüfter<select id="t-fan"><option value="eco">Eco</option><option value="high">High</option><option value="boost" id="t-boost">Boost</option></select></label>
      <label>Warmwasser<select id="t-water"><option value="">unverändert</option><option value="off">Aus</option><option value="eco">Eco 40°</option><option value="high">Hot 60°</option></select></label>
    </div>
    <div class="row"><button class="btn primary" id="t-set">Timer setzen</button><button class="btn" id="t-off">Timer aus</button></div>
  </div>
  <div class="card">
    <h2>Freibrennen</h2>
    <div class="sub" id="decoke-text">Maximale Temperatur mit Lüfter High, danach werden die vorherigen Einstellungen wiederhergestellt.</div>
    <div class="row">
      <button class="btn primary" id="decoke">Starten</button>
      <span class="sub">Dauer</span>
      <select id="decoke-min" style="width:auto"></select>
    </div>
  </div>
  <div class="card">
    <h2>Details</h2>
    <div class="grid">
      <div class="kv"><span>Betriebsstatus</span><b id="opstat">--</b></div>
      <div class="kv"><span>Fehlercode</span><b id="err">--</b></div>
      <div class="kv"><span>Timer</span><b id="timer">--</b></div>
      <div class="kv"><span>Uhr CP Plus</span><b id="clock">--</b></div>
    </div>
    <div class="row"><button class="btn" id="sync">Uhrzeit an CP Plus senden</button></div>
  </div>
</section>

<section id="tab-sys" class="hide">
  <div class="card">
    <h2>LIN-Bus</h2>
    <div class="grid">
      <div class="kv"><span>LIN-Verkehr</span><b id="s-traffic">--</b></div>
      <div class="kv"><span>CP Plus angemeldet</span><b id="s-cp">--</b></div>
      <div class="kv"><span>CP Plus</span><b id="s-cpv">--</b></div>
      <div class="kv"><span>Heizung</span><b id="s-hv">--</b></div>
      <div class="kv"><span>Frames ok / CRC-Fehler</span><b id="s-frames">--</b></div>
      <div class="kv"><span>Abgelehnte Befehle</span><b id="s-ack">--</b></div>
    </div>
    <div class="hint">Anmeldung am CP Plus: Einstellungen → RESET → PR SET. Danach erscheint im INDEX-Menü ein dritter Eintrag T23.70.0. Eine echte iNet Box darf nicht gleichzeitig am Bus hängen.</div>
  </div>
  <div class="card">
    <h2>Gerät</h2>
    <div class="grid">
      <div class="kv"><span>Firmware</span><b id="s-fw">--</b></div>
      <div class="kv"><span>IP</span><b id="s-ip">--</b></div>
      <div class="kv"><span>WLAN</span><b id="s-rssi">--</b></div>
      <div class="kv"><span>Laufzeit</span><b id="s-up">--</b></div>
      <div class="kv"><span>Freier Speicher</span><b id="s-heap">--</b></div>
    </div>
    <div class="row"><button class="btn" id="reboot">Neustart</button></div>
  </div>
  <div class="card">
    <h2>Firmware-Update</h2>
    <input type="file" id="fw" accept=".bin">
    <div class="row"><button class="btn primary" id="upload">Hochladen</button><span class="sub" id="up-status"></span></div>
  </div>
  <div class="card">
    <h2>Log</h2>
    <div class="row" style="margin-top:0">
      <select id="loglvl" style="width:auto"><option value="0">Fehler</option><option value="1">Warnung</option><option value="2">Info</option><option value="3">Debug</option></select>
      <label class="check" style="margin:0"><input type="checkbox" id="loglive" checked> Live</label>
      <button class="btn" id="logclear">Leeren</button>
      <button class="btn" id="logsave">Speichern</button>
    </div>
    <pre id="log" style="margin-top:10px"></pre>
    <div class="hint">Läuft mit, solange dieser Reiter offen ist. Ohne USB nutzbar – auch per MQTT unter <code id="logtopic">service/truma/log</code>.</div>
  </div>
</section>

<section id="tab-cfg" class="hide">
  <form id="cfg">
  <div class="card">
    <h2>WLAN</h2>
    <div class="cols">
      <div><label>SSID</label><input name="ssid"></div>
      <div><label>Passwort (leer = unverändert)</label><input name="wifi_pass" type="password" autocomplete="new-password"></div>
      <div><label>Hostname</label><input name="hostname"></div>
      <div><label>Setup-AP Passwort (min. 8 Zeichen)</label><input name="ap_pass" type="password" autocomplete="new-password"></div>
    </div>
    <div class="hint">Feste IP optional, alle Felder leer = DHCP.</div>
    <div class="cols">
      <div><label>Statische IP</label><input name="static_ip" placeholder="192.168.x.y"></div>
      <div><label>Gateway</label><input name="gateway"></div>
      <div><label>Subnetz</label><input name="subnet" placeholder="255.255.255.0"></div>
      <div><label>DNS</label><input name="dns"></div>
    </div>
  </div>
  <div class="card">
    <h2>MQTT</h2>
    <div class="cols">
      <div><label>Broker</label><input name="mqtt_host"></div>
      <div><label>Port</label><input name="mqtt_port" type="number"></div>
      <div><label>Benutzer</label><input name="mqtt_user"></div>
      <div><label>Passwort (leer = unverändert)</label><input name="mqtt_pass" type="password" autocomplete="new-password"></div>
      <div><label>Basis-Topic</label><input name="mqtt_base"></div>
      <div><label>HA Discovery Prefix</label><input name="ha_prefix"></div>
    </div>
    <label class="check"><input type="checkbox" name="ha_discovery"> Home Assistant Auto-Discovery</label>
    <label class="check"><input type="checkbox" name="clear_mqtt_pass"> MQTT-Passwort löschen</label>
    <label>Log per MQTT (Topic &lt;Basis-Topic&gt;/log)</label>
    <select name="mqtt_log_level"><option value="4">Aus</option><option value="0">Nur Fehler</option><option value="1">Warnungen + Fehler</option><option value="2">Info</option><option value="3">Debug (viel Verkehr)</option></select>
  </div>
  <div class="card">
    <h2>Zeit & Zugriff</h2>
    <div class="cols">
      <div><label>NTP-Server</label><input name="ntp_server"></div>
      <div><label>Zeitzone (POSIX)</label><input name="tz"></div>
      <div><label>Web-Benutzer (leer = offen)</label><input name="web_user"></div>
      <div><label>Web-Passwort</label><input name="web_pass" type="password" autocomplete="new-password"></div>
    </div>
    <label class="check"><input type="checkbox" name="clear_web_pass"> Web-Login entfernen</label>
  </div>
  <button class="btn primary" type="submit">Speichern & Neustart</button>
  </form>
</section>
</main>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);
let st=null,localSetp=null,setpTimer=null,timerEdit=false;
function toast(t){const e=$('toast');e.textContent=t;e.classList.add('show');setTimeout(()=>e.classList.remove('show'),1800)}
async function cmd(a,v){const b=new URLSearchParams({a,v:v??''});try{const r=await fetch('/api/cmd',{method:'POST',body:b});if(!r.ok)throw 0;setTimeout(load,400)}catch(e){toast('Befehl fehlgeschlagen')}}
function seg(id,val){document.querySelectorAll('#'+id+' button').forEach(b=>b.classList.toggle('on',b.dataset.v===val))}
function setTxt(id,t){$(id).textContent=t}
function fmtUp(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);return(d?d+'d ':'')+h+'h '+m+'m'}
function render(){
  const s=st,h=s.heater,l=s.lin;
  $('p-cp').className='pill '+(l.cpplus?'ok':'bad');$('p-cp').textContent=l.cpplus?'CP Plus verbunden':'CP Plus getrennt';
  $('p-mqtt').className='pill '+(s.system.mqtt?'ok':'bad');$('p-mqtt').textContent=s.system.mqtt?'MQTT':'MQTT getrennt';
  const nl=$('nolink');
  if(!l.cpplus||!h.valid){nl.classList.remove('hide');
    setTxt('nolink-text',!l.traffic?'Kein LIN-Verkehr erkannt. Verkabelung, 12 V Versorgung und Masse prüfen.':(!l.cpplus?'LIN-Verkehr ist da, aber das CP Plus fragt dieses Gerät nicht ab. Einmal RESET → PR SET am CP Plus ausführen.':'Warte auf Status der Heizung …'));
  }else nl.classList.add('hide');
  setTxt('room',h.room!==undefined?h.room.toFixed(1):'--');
  setTxt('water',h.water!==undefined?h.water.toFixed(0):'--');
  setTxt('target',h.on?h.target_room+' °C':'aus');
  setTxt('status',h.valid?(h.action==='heating'?'heizt':(h.on?'Temperatur erreicht':'aus')):'--');
  seg('seg-room',h.on?'1':'0');
  if(localSetp===null)setTxt('setp',(+h.setpoint).toFixed(0)+' °C');
  seg('seg-fan',h.fan);
  seg('seg-water',h.water_mode||'');
  $('fan-boost').classList.toggle('hide',!h.boost);$('water-boost').classList.toggle('hide',!h.water_boost);
  const d=s.decoke;$('decoke').textContent=d.active?'Beenden':'Starten';
  setTxt('decoke-text',d.active?('Läuft noch '+Math.ceil(d.remaining/60)+' min.'):'Maximale Temperatur mit Lüfter High, danach werden die vorherigen Einstellungen wiederhergestellt.');
  if(document.activeElement!==$('decoke-min'))$('decoke-min').value=d.minutes;
  setTxt('opstat',h.valid?h.status+' ('+h.status_text+')':'--');
  setTxt('err',h.valid?(h.error?h.error:'keiner'):'--');
  setTxt('timer',s.timer??'--');
  const tc=s.timer_cfg;$('t-boost').classList.toggle('hide',!h.boost);
  setTxt('timer-text',tc?(tc.active?'Aktiv '+tc.start+'–'+tc.stop+', '+tc.temp+' °C, '+tc.fan:'Aus (gespeichert '+tc.start+'–'+tc.stop+')'):'Noch kein Timer-Status vom CP Plus.');
  if(tc&&!timerEdit){$('t-start').value=tc.start;$('t-stop').value=tc.stop;$('t-temp').value=tc.temp>=5?tc.temp:Math.round(+h.setpoint);
    $('t-fan').value=(tc.fan==='off'?h.fan:tc.fan)||'eco';}setTxt('clock',s.cpplus_clock??'--');
  setTxt('s-traffic',l.traffic?'ja':'nein');setTxt('s-cp',l.cpplus?'ja':'nein');
  setTxt('s-cpv',l.cpplus_version||'--');setTxt('s-hv',l.heater_version||'--');
  setTxt('s-frames',l.frames+' / '+l.crc_errors);setTxt('s-ack',l.ack_errors);
  setTxt('s-fw',s.system.fw);setTxt('s-ip',s.system.ip+(s.system.ap?' (Setup-AP)':''));
  setTxt('s-rssi',s.system.wifi?s.system.rssi+' dBm':'getrennt');setTxt('s-up',fmtUp(s.system.uptime));
  setTxt('s-heap',Math.round(s.system.heap/1024)+' kB');
  const ctl=h.valid&&l.cpplus;document.querySelectorAll('#tab-heat .btn,#tab-heat .seg button').forEach(b=>b.disabled=!ctl);
}
async function load(){try{const r=await fetch('/api/state');st=await r.json();render()}catch(e){}}
let logNext=0,logLines=[],logTimer=null;
async function loadLog(){try{const r=await fetch('/api/log?since='+logNext);const j=await r.json();logNext=j.next;
  if(j.lines){const el=$('log'),atEnd=el.scrollTop+el.clientHeight>=el.scrollHeight-20;
    logLines=logLines.concat(j.lines.replace(/\n$/,'').split('\n')).slice(-400);el.textContent=logLines.join('\n');if(atEnd)el.scrollTop=1e9}}catch(e){}}
function logLive(){clearInterval(logTimer);logTimer=null;if($('loglive').checked&&!$('tab-sys').classList.contains('hide'))logTimer=setInterval(loadLog,2000)}
async function loadCfg(){const r=await fetch('/api/config');const c=await r.json();const f=$('cfg');
  for(const[k,v]of Object.entries(c)){const e=f.elements[k];if(!e)continue;if(e.type==='checkbox')e.checked=!!v;else e.value=v}
  $('loglvl').value=c.log_level;setTxt('logtopic',c.mqtt_base+'/log')}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x===b));
  ['heat','sys','cfg'].forEach(t=>$('tab-'+t).classList.toggle('hide',t!==b.dataset.tab));if(b.dataset.tab==='sys')loadLog();if(b.dataset.tab==='cfg')loadCfg();logLive()});
document.querySelectorAll('#seg-room button').forEach(b=>b.onclick=()=>cmd('room_on',b.dataset.v));
document.querySelectorAll('#seg-fan button').forEach(b=>b.onclick=()=>cmd('fan',b.dataset.v));
document.querySelectorAll('#seg-water button').forEach(b=>b.onclick=()=>cmd('water',b.dataset.v));
function step(d){const base=localSetp??Math.round(+st.heater.setpoint);localSetp=Math.min(30,Math.max(5,base+d));setTxt('setp',localSetp+' °C');
  clearTimeout(setpTimer);setpTimer=setTimeout(()=>{cmd('setpoint',localSetp);setTimeout(()=>localSetp=null,1500)},900)}
$('minus').onclick=()=>step(-1);$('plus').onclick=()=>step(1);
$('decoke').onclick=()=>{const on=!st.decoke.active;if(on&&!confirm('Freibrennen jetzt starten?'))return;cmd('decoke',on?'1':'0')};
for(let m=5;m<=120;m+=5){const o=document.createElement('option');o.value=m;o.textContent=m+' min';$('decoke-min').appendChild(o)}
$('decoke-min').onchange=e=>cmd('decoke_minutes',e.target.value);
['t-start','t-stop','t-temp','t-fan','t-water'].forEach(id=>$(id).oninput=()=>timerEdit=true);
$('t-set').onclick=()=>{const a=$('t-start').value,b=$('t-stop').value;if(!a||!b||a===b){toast('Start und Ende prüfen');return}
  cmd('timer',a+'-'+b+','+$('t-temp').value+','+$('t-fan').value+($('t-water').value?','+$('t-water').value:''));timerEdit=false;toast('Timer wird gesendet')};
$('t-off').onclick=()=>{cmd('timer','off');timerEdit=false};
$('sync').onclick=()=>{cmd('sync_clock');toast('Uhrzeit wird gesendet')};
$('reboot').onclick=async()=>{if(!confirm('Gerät neu starten?'))return;await fetch('/api/reboot',{method:'POST'});toast('Neustart …')};
$('loglive').onchange=logLive;
$('logclear').onclick=()=>{logLines=[];$('log').textContent=''};
$('logsave').onclick=()=>{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([logLines.join('\n')],{type:'text/plain'}));a.download='truma-log.txt';a.click()};
$('loglvl').onchange=e=>cmd('log_level',e.target.value);
$('upload').onclick=async()=>{const f=$('fw').files[0];if(!f)return;const fd=new FormData();fd.append('firmware',f,f.name);setTxt('up-status','Lade hoch …');
  try{const r=await fetch('/update',{method:'POST',body:fd});setTxt('up-status',r.ok?'Fertig, Neustart …':'Fehlgeschlagen')}catch(e){setTxt('up-status','Fehlgeschlagen')}};
$('cfg').onsubmit=async e=>{e.preventDefault();if(!confirm('Speichern und neu starten?'))return;
  const r=await fetch('/api/config',{method:'POST',body:new URLSearchParams(new FormData(e.target))});toast(r.ok?'Gespeichert, Neustart …':'Fehler beim Speichern')};
load();setInterval(load,2000);loadCfg().catch(()=>{});
</script>
</body>
</html>)rawliteral";
