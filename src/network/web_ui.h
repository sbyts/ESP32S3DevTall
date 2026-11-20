#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Ridy-10-Lite Core OS</title>
<style>
  :root {
    --bg: #121210;
    --card: #1e1e1b;
    --graphite: #2d2d14;
    --orange: #fc5a2c;
    --cyan: #00beff;
    --text: #f0f0ec;
    --muted: #8a8a80;
  }
  * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
  body { background: var(--bg); color: var(--text); margin: 0; padding: 16px; display: flex; justify-content: center; }
  .wrap { width: 100%; max-width: 520px; display: flex; flex-direction: column; gap: 14px; }
  .header { display: flex; justify-content: space-between; align-items: center; padding: 8px 4px; }
  .header h1 { font-size: 1.2rem; margin: 0; letter-spacing: 0.5px; }
  .badge { background: var(--graphite); color: var(--cyan); padding: 4px 12px; border-radius: 99px; font-size: 0.8rem; font-weight: 600; }
  .card { background: var(--card); border-radius: 18px; padding: 18px; border: 1px solid #2c2c28; }
  .card-head { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
  .card h2 { margin: 0; font-size: 0.9rem; color: var(--muted); text-transform: uppercase; letter-spacing: 1px; }
  .row { display: flex; gap: 10px; align-items: center; margin-bottom: 12px; }
  .btn { flex: 1; padding: 12px; border: none; border-radius: 12px; font-weight: 600; cursor: pointer; background: #2a2a26; color: var(--text); transition: 0.2s; }
  .btn.active { background: var(--orange); color: #fff; }
  .btn.cyan.active { background: var(--cyan); color: #000; }
  .btn.power { background: #2a2a26; border: 1px solid var(--orange); color: var(--orange); font-size: 1rem; }
  .btn.power.on { background: var(--orange); color: #fff; }
  .btn-xs { background: #2a2a26; color: var(--muted); border: 1px solid #353530; border-radius: 8px; padding: 4px 10px; font-size: 0.75rem; cursor: pointer; }
  input[type=range] { width: 100%; accent-color: var(--orange); height: 6px; border-radius: 5px; }
  .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 0.85rem; }
  .stat { background: #161614; padding: 10px 12px; border-radius: 10px; }
  .stat span { display: block; color: var(--muted); font-size: 0.75rem; margin-bottom: 3px; }
  .log-box { background: #121210; border: 1px solid #2a2a25; border-radius: 10px; padding: 10px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; font-size: 0.76rem; max-height: 190px; overflow-y: auto; display: flex; flex-direction: column; gap: 4px; }
  .log-line { line-height: 1.35; word-break: break-word; }
  .l-0 { color: #d8d8d0; }
  .l-1 { color: #ffb347; }
  .l-2 { color: #ff5c5c; }
  .l-ts { color: #6e6e65; margin-right: 6px; }
  .wifi-slot { display: flex; gap: 6px; margin-bottom: 8px; }
  .wifi-slot input { width: 50%; background: #161614; border: 1px solid #333; color: var(--text); padding: 8px 10px; border-radius: 8px; font-size: 0.85rem; }
  .save-btn { width: 100%; padding: 12px; border: none; border-radius: 12px; background: var(--cyan); color: #000; font-weight: 700; cursor: pointer; margin-top: 6px; }
</style>
</head>
<body>
<div class="wrap">
  <div class="header">
    <h1>💡 RIDY-10-LITE CORE</h1>
    <div class="badge" id="netBadge">Connecting...</div>
  </div>

  <div class="card">
    <div class="card-head"><h2>Lighting Control</h2></div>
    <div class="row">
      <button id="pwrBtn" class="btn power" onclick="togglePower()">POWER OFF</button>
    </div>
    <div class="row">
      <button id="m1" class="btn" onclick="setMode(1)">Warm 3000K</button>
      <button id="m2" class="btn cyan" onclick="setMode(2)">Cool 6000K</button>
      <button id="m3" class="btn" onclick="setMode(3)">Mix 4500K</button>
    </div>
    <div style="margin-top:14px;">
      <div style="display:flex; justify-content:space-between; font-size:0.85rem; margin-bottom:6px;">
        <span>Brightness</span><span id="briVal">80%</span>
      </div>
      <input type="range" id="briSlider" min="10" max="100" value="80" oninput="onBriInput(this.value)" onchange="sendControl()">
    </div>
  </div>

  <div class="card">
    <div class="card-head"><h2>Hardware Telemetry</h2></div>
    <div class="grid">
      <div class="stat"><span>Wi-Fi / RSSI</span><b id="tWifi">—</b></div>
      <div class="stat"><span>IP Address</span><b id="tIp">—</b></div>
      <div class="stat"><span>Free / Min Heap</span><b id="tHeap">—</b></div>
      <div class="stat"><span>CPU Temp / Uptime</span><b id="tTemp">—</b></div>
      <div class="stat"><span>Touch Delta (Δ)</span><b id="tTouch">0</b></div>
    </div>
  </div>

  <div class="card">
    <div class="card-head">
      <h2>System Ring Log (12 Entries)</h2>
      <button class="btn-xs" onclick="clearLogs()">Clear</button>
    </div>
    <div class="log-box" id="logBox">Waiting for telemetry...</div>
  </div>

  <div class="card">
    <div class="card-head"><h2>Multi-AP Roaming & Security</h2></div>
    <div id="slots"></div>
    <div class="wifi-slot" style="margin-top:10px;">
      <input type="password" id="adminPass" placeholder="New Web/OTA Password (optional)">
      <input type="text" id="otaHost" placeholder="Hostname (ridy-lamp)">
    </div>
    <button class="save-btn" onclick="saveConfig()">Save & Reboot</button>
  </div>
</div>

<script>
let state = { on: false, mode: 1, bri: 80 };

function renderLightUI() {
  const pwr = document.getElementById('pwrBtn');
  pwr.textContent = state.on ? 'POWER ON' : 'POWER OFF';
  pwr.className = state.on ? 'btn power on' : 'btn power';
  [1, 2, 3].forEach(m => {
    document.getElementById('m'+m).classList.toggle('active', state.on && state.mode === m);
  });
  document.getElementById('briSlider').value = state.bri;
  document.getElementById('briVal').textContent = state.bri + '%';
}

function togglePower() { state.on = !state.on; renderLightUI(); sendControl(); }
function setMode(m) { state.on = true; state.mode = m; renderLightUI(); sendControl(); }
function onBriInput(v) { state.bri = parseInt(v); document.getElementById('briVal').textContent = v + '%'; }

async function sendControl() {
  await fetch('/api/control', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(state)
  });
}

async function clearLogs() {
  await fetch('/api/logs/clear', { method: 'POST' });
  fetchTelemetry();
}

async function fetchTelemetry() {
  try {
    const r = await fetch('/api/telemetry');
    const d = await r.json();
    state.on = d.on; state.mode = d.mode; state.bri = d.bri;
    renderLightUI();
    document.getElementById('netBadge').textContent = d.apMode ? 'AP FALLBACK' : (d.ssid + ' (' + d.rssi + ' dBm)');
    document.getElementById('tWifi').textContent = d.ssid + ' (' + d.rssi + ' dBm)';
    document.getElementById('tIp').textContent = d.ip;
    document.getElementById('tHeap').textContent = Math.round(d.freeHeap/1024) + ' / ' + Math.round(d.minHeap/1024) + ' KB';
    document.getElementById('tTemp').textContent = d.temp.toFixed(1) + '°C / ' + d.uptime + 's';
    document.getElementById('tTouch').textContent = (d.touch >= 0 ? '+' : '') + d.touch;

    if (d.logs) {
      const box = document.getElementById('logBox');
      box.innerHTML = d.logs.map(x =>
        `<div class="log-line l-${x.l}"><span class="l-ts">[${x.t}s]</span>${x.m}</div>`
      ).join('');
      box.scrollTop = box.scrollHeight;
    }
  } catch(e) {}
}

async function loadConfig() {
  const r = await fetch('/api/config');
  const d = await r.json();
  document.getElementById('otaHost').value = d.host || 'ridy-lamp';
  const container = document.getElementById('slots');
  container.innerHTML = '';
  for (let i = 0; i < 6; i++) {
    const s = d.nets[i] || {ssid:'', pass:''};
    container.innerHTML += `<div class="wifi-slot">
      <input type="text" id="ssid_${i}" value="${s.ssid}" placeholder="Slot ${i+1} SSID">
      <input type="password" id="pass_${i}" value="${s.pass}" placeholder="Password">
    </div>`;
  }
}

async function saveConfig() {
  let nets = [];
  for (let i = 0; i < 6; i++) {
    nets.push({
      ssid: document.getElementById('ssid_'+i).value.trim(),
      pass: document.getElementById('pass_'+i).value
    });
  }
  const payload = {
    nets: nets,
    pass: document.getElementById('adminPass').value,
    host: document.getElementById('otaHost').value.trim()
  };
  await fetch('/api/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(payload)
  });
  alert('Saved! Device is rebooting...');
}

loadConfig();
fetchTelemetry();
setInterval(fetchTelemetry, 2000);
</script>
</body>
</html>
)rawliteral";