#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>

// Standalone Cyberpunk Web Dashboard (Embedded HTML5 + CSS3 + Vanilla JS)
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Krypton Bionic Arm Control Hub</title>
  <style>
    :root {
      --bg: #090c10;
      --card: #121820;
      --border: #1f2937;
      --accent: #00f0ff;
      --accent-glow: rgba(0, 240, 255, 0.25);
      --yellow: #ffb700;
      --text: #e6edf3;
      --text-dim: #8b949e;
      --danger: #ff4757;
      --success: #2ed573;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; }
    body { background-color: var(--bg); color: var(--text); padding: 16px; min-height: 100vh; }
    .container { max-width: 600px; margin: 0 auto; display: flex; flex-direction: column; gap: 16px; }
    
    header { display: flex; justify-content: space-between; align-items: center; padding: 12px 16px; background: var(--card); border: 1px solid var(--border); border-radius: 8px; }
    .brand h1 { font-size: 1.25rem; font-weight: 700; letter-spacing: 1.5px; color: var(--accent); }
    .brand p { font-size: 0.75rem; color: var(--text-dim); }
    .status-badge { display: flex; align-items: center; gap: 6px; font-size: 0.75rem; font-weight: 600; padding: 4px 10px; border-radius: 12px; background: rgba(46, 213, 115, 0.1); color: var(--success); border: 1px solid rgba(46, 213, 115, 0.3); }
    .status-badge.offline { background: rgba(255, 71, 87, 0.1); color: var(--danger); border-color: rgba(255, 71, 87, 0.3); }
    .status-dot { width: 8px; height: 8px; border-radius: 50%; background: currentColor; }

    .card { background: var(--card); border: 1px solid var(--border); border-radius: 8px; padding: 16px; display: flex; flex-direction: column; gap: 14px; }
    .card-title { font-size: 0.85rem; font-weight: 700; text-transform: uppercase; letter-spacing: 1px; color: var(--text-dim); display: flex; justify-content: space-between; }
    
    .slider-row { display: flex; flex-direction: column; gap: 6px; }
    .slider-header { display: flex; justify-content: space-between; font-size: 0.85rem; font-weight: 500; }
    .slider-val { font-family: monospace; color: var(--accent); font-weight: 700; }
    input[type=range] { -webkit-appearance: none; width: 100%; height: 8px; border-radius: 4px; background: #1c2430; outline: none; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%; background: var(--accent); cursor: pointer; box-shadow: 0 0 10px var(--accent-glow); }
    
    .btn-grid { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
    button { background: #182230; border: 1px solid var(--border); color: var(--text); padding: 10px 12px; border-radius: 6px; font-size: 0.8rem; font-weight: 600; cursor: pointer; transition: all 0.15s ease; }
    button:hover { background: #223044; border-color: var(--accent); color: var(--accent); }
    button:active { transform: scale(0.97); }
    button.btn-accent { background: rgba(0, 240, 255, 0.1); border-color: var(--accent); color: var(--accent); }
    button.btn-yellow { background: rgba(255, 183, 0, 0.1); border-color: var(--yellow); color: var(--yellow); }

    .telemetry-row { display: flex; justify-content: space-between; font-size: 0.75rem; color: var(--text-dim); border-top: 1px solid var(--border); padding-top: 8px; }
    .telemetry-val { font-family: monospace; color: var(--text); }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="brand">
        <h1>KRYPTON</h1>
        <p>Bionic Arm Telemetry Hub</p>
      </div>
      <div id="statusBadge" class="status-badge offline">
        <div class="status-dot"></div>
        <span id="statusText">CONNECTING</span>
      </div>
    </header>

    <div class="card">
      <div class="card-title">
        <span>Articulated Tendon Servos</span>
        <button onclick="resetAll()" style="padding: 2px 8px; font-size: 0.7rem;">RESET (0&deg;)</button>
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Thumb (CH 0)</span><span class="slider-val" id="val0">0&deg;</span></div>
        <input type="range" id="ch0" min="0" max="180" value="0" oninput="sendServo(0, this.value)">
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Index (CH 1)</span><span class="slider-val" id="val1">0&deg;</span></div>
        <input type="range" id="ch1" min="0" max="180" value="0" oninput="sendServo(1, this.value)">
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Middle (CH 2)</span><span class="slider-val" id="val2">0&deg;</span></div>
        <input type="range" id="ch2" min="0" max="180" value="0" oninput="sendServo(2, this.value)">
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Ring (CH 3)</span><span class="slider-val" id="val3">0&deg;</span></div>
        <input type="range" id="ch3" min="0" max="180" value="0" oninput="sendServo(3, this.value)">
      </div>

      <div class="slider-row">
        <div class="slider-header"><span>Pinky (CH 4)</span><span class="slider-val" id="val4">0&deg;</span></div>
        <input type="range" id="ch4" min="0" max="180" value="0" oninput="sendServo(4, this.value)">
      </div>
    </div>

    <div class="card">
      <div class="card-title">Gesture Presets</div>
      <div class="btn-grid">
        <button class="btn-accent" onclick="sendPose('OPEN')">OPEN HAND</button>
        <button class="btn-accent" onclick="sendPose('FIST')">FIST</button>
        <button class="btn-accent" onclick="sendPose('POINT')">POINT</button>
        <button class="btn-yellow" onclick="sendPose('PEACE')">PEACE (V)</button>
        <button class="btn-yellow" onclick="sendPose('THUMBS_UP')">THUMBS UP</button>
        <button class="btn-yellow" onclick="sendPose('HORNS')">HORNS</button>
        <button class="btn-accent" onclick="sendPose('ROCK_ON')">ROCK ON</button>
        <button class="btn-yellow" onclick="sendPose('THREE_FINGER')">THREE</button>
        <button class="btn-accent" onclick="sendPose('PINCH')">PINCH</button>
      </div>
    </div>

    <div class="card">
      <div class="card-title">Diagnostics</div>
      <div class="telemetry-row">
        <span>Bus / Driver</span>
        <span class="telemetry-val">PCA9685 @ 0x40 (50Hz)</span>
      </div>
      <div class="telemetry-row">
        <span>Trajectory Mode</span>
        <span class="telemetry-val">Dynamic Easing (~66Hz)</span>
      </div>
      <div class="telemetry-row">
        <span>Vision / UDP Telemetry</span>
        <span class="telemetry-val">Port 8888 Active</span>
      </div>
      <div class="telemetry-row">
        <span>Link Latency</span>
        <span class="telemetry-val" id="linkLatency">&lt; 15 ms</span>
      </div>
    </div>
  </div>

  <script>
    let ws;
    let wsConnected = false;

    function initWS() {
      const loc = window.location;
      const wsUri = (loc.protocol === "https:" ? "wss://" : "ws://") + loc.hostname + ":81/";
      ws = new WebSocket(wsUri);

      ws.onopen = () => {
        wsConnected = true;
        updateBadge(true);
      };

      ws.onclose = () => {
        wsConnected = false;
        updateBadge(false);
        setTimeout(initWS, 2000);
      };

      ws.onmessage = (evt) => {
        try {
          const data = JSON.parse(evt.data);
          if (data.angles) {
            for (let i = 0; i < 5; i++) {
              document.getElementById('ch' + i).value = data.angles[i];
              document.getElementById('val' + i).innerText = Math.round(data.angles[i]) + '°';
            }
          }
        } catch(e) {}
      };
    }

    function updateBadge(online) {
      const b = document.getElementById('statusBadge');
      const t = document.getElementById('statusText');
      if (online) {
        b.className = 'status-badge';
        t.innerText = 'ONLINE (WS)';
      } else {
        b.className = 'status-badge offline';
        t.innerText = 'DISCONNECTED';
      }
    }

    function sendServo(ch, val) {
      document.getElementById('val' + ch).innerText = val + '°';
      if (wsConnected) {
        ws.send(JSON.stringify({ type: 'servo', channel: ch, angle: parseFloat(val) }));
      } else {
        fetch('/api/servos', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ channel: ch, angle: parseFloat(val) })
        }).catch(()=>{});
      }
    }

    function sendPose(poseName) {
      if (wsConnected) {
        ws.send(JSON.stringify({ type: 'pose', name: poseName }));
      } else {
        fetch('/api/pose', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name: poseName })
        }).catch(()=>{});
      }
    }

    function resetAll() {
      sendPose('OPEN');
    }

    window.onload = initWS;
  </script>
</body>
</html>
)rawliteral";

#endif // WEB_DASHBOARD_H
