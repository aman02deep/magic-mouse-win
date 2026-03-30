package server

import (
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"time"

	"magicmouse-util/internal/bluetooth"
	"magicmouse-util/internal/device"
	"magicmouse-util/internal/driver"
)

// Server holds the HTTP server and monitor reference
type Server struct {
	monitor *bluetooth.Monitor
	mux     *http.ServeMux
}

// New creates a new web server
func New(monitor *bluetooth.Monitor) *Server {
	s := &Server{
		monitor: monitor,
		mux:     http.NewServeMux(),
	}
	s.registerRoutes()
	return s
}

// Start begins listening on the given address
func (s *Server) Start(addr string) error {
	log.Printf("Web UI available at http://localhost%s", addr)
	return http.ListenAndServe(addr, s.mux)
}

func (s *Server) registerRoutes() {
	s.mux.HandleFunc("/api/mouse", s.handleGetMouse)
	s.mux.HandleFunc("/api/status", s.handleStatus)
	s.mux.HandleFunc("/api/diagnostics", s.handleDiagnostics)
	s.mux.HandleFunc("/api/events", s.handleSSE)
	s.mux.HandleFunc("/api/install", s.handleInstallDriver)
	s.mux.HandleFunc("/", s.handleUI)
}

// handleInstallDriver invokes the underlying driver installation logic
func (s *Server) handleInstallDriver(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
		return
	}

	err := driver.InstallDriver()
	w.Header().Set("Content-Type", "application/json")
	if err != nil {
		w.WriteHeader(http.StatusInternalServerError)
		json.NewEncoder(w).Encode(map[string]string{"error": err.Error()})
		return
	}
	json.NewEncoder(w).Encode(map[string]bool{"success": true})
}

// handleGetMouse returns current mouse state as JSON
func (s *Server) handleGetMouse(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	mouse := s.monitor.GetMouse()
	if mouse == nil {
		mouse = &device.MagicMouse{
			Name:          "No Magic Mouse found",
			Connected:     false,
			BatteryLevel:  0,
			ChargingState: device.StateUnknown,
		}
	}
	mouse.DriverInstalled = driver.IsDriverInstalled()
	json.NewEncoder(w).Encode(mouse)
}

// handleStatus returns a simple health check
func (s *Server) handleStatus(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	json.NewEncoder(w).Encode(map[string]interface{}{
		"status": "ok",
		"time":   time.Now().Format(time.RFC3339),
	})
}

// handleDiagnostics returns all BT/HID devices for debugging
func (s *Server) handleDiagnostics(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	out, err := bluetooth.QueryAllBluetoothDevices()
	if err != nil {
		json.NewEncoder(w).Encode(map[string]string{"error": err.Error()})
		return
	}
	// Already JSON from PowerShell, write directly
	w.Write([]byte(out))
}

// handleSSE sends server-sent events with mouse state updates
func (s *Server) handleSSE(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/event-stream")
	w.Header().Set("Cache-Control", "no-cache")
	w.Header().Set("Connection", "keep-alive")
	w.Header().Set("Access-Control-Allow-Origin", "*")

	flusher, ok := w.(http.Flusher)
	if !ok {
		http.Error(w, "Streaming not supported", http.StatusInternalServerError)
		return
	}

	ticker := time.NewTicker(2 * time.Second)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			mouse := s.monitor.GetMouse()
			if mouse == nil {
				mouse = &device.MagicMouse{
					Name:      "No Magic Mouse found",
					Connected: false,
				}
			}
			mouse.DriverInstalled = driver.IsDriverInstalled()
			data, _ := json.Marshal(mouse)
			fmt.Fprintf(w, "data: %s\n\n", data)
			flusher.Flush()
		case <-r.Context().Done():
			return
		}
	}
}

// handleUI serves the embedded HTML dashboard
func (s *Server) handleUI(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "text/html")
	w.Write([]byte(dashboardHTML))
}

const dashboardHTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1.0"/>
  <title>Magic Mouse Utility</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0a0a1a;
      color: #e0e0f0;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      overflow: hidden;
    }

    /* Animated background gradient */
    body::before {
      content: '';
      position: fixed;
      top: -50%; left: -50%;
      width: 200%; height: 200%;
      background: radial-gradient(ellipse at 30% 20%, rgba(99, 102, 241, 0.08) 0%, transparent 50%),
                  radial-gradient(ellipse at 70% 80%, rgba(139, 92, 246, 0.06) 0%, transparent 50%);
      animation: bgShift 20s ease-in-out infinite alternate;
      z-index: 0;
    }
    @keyframes bgShift {
      0% { transform: translate(0, 0) rotate(0deg); }
      100% { transform: translate(-5%, 3%) rotate(3deg); }
    }

    .container {
      position: relative;
      z-index: 1;
      width: 100%;
      max-width: 420px;
      padding: 16px;
    }

    .card {
      background: rgba(18, 18, 40, 0.85);
      backdrop-filter: blur(24px);
      -webkit-backdrop-filter: blur(24px);
      border-radius: 24px;
      padding: 36px 32px;
      border: 1px solid rgba(99, 102, 241, 0.15);
      box-shadow: 0 24px 80px rgba(0,0,0,0.6), 0 0 0 1px rgba(255,255,255,0.03) inset;
    }

    .header {
      display: flex;
      align-items: center;
      gap: 14px;
      margin-bottom: 28px;
    }

    .dot {
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #ef4444;
      transition: all 0.5s ease;
      flex-shrink: 0;
    }
    .dot.connected {
      background: #22c55e;
      box-shadow: 0 0 12px rgba(34, 197, 94, 0.5), 0 0 4px rgba(34, 197, 94, 0.3);
      animation: pulse 2s ease-in-out infinite;
    }
    @keyframes pulse {
      0%, 100% { box-shadow: 0 0 12px rgba(34,197,94,0.5); }
      50% { box-shadow: 0 0 20px rgba(34,197,94,0.8); }
    }

    h1 {
      font-size: 18px;
      font-weight: 700;
      letter-spacing: -0.3px;
      background: linear-gradient(135deg, #e0e0f0, #a5b4fc);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }

    .mouse-visual {
      display: flex;
      justify-content: center;
      margin: 20px 0 28px;
    }
    .mouse-visual svg {
      width: 64px;
      opacity: 0.7;
      filter: drop-shadow(0 8px 24px rgba(99,102,241,0.2));
      transition: opacity 0.3s;
    }
    .mouse-visual.active svg { opacity: 1; }

    .info-grid {
      display: grid;
      gap: 8px;
      margin-bottom: 24px;
    }
    .info-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      background: rgba(255,255,255,0.03);
      padding: 12px 16px;
      border-radius: 12px;
      border: 1px solid rgba(255,255,255,0.04);
      transition: background 0.2s;
    }
    .info-row:hover {
      background: rgba(255,255,255,0.06);
    }
    .info-label {
      color: #6b7280;
      font-size: 13px;
      font-weight: 500;
    }
    .info-val {
      font-weight: 600;
      font-size: 13px;
      color: #c7d2fe;
      text-align: right;
      max-width: 200px;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    /* Battery */
    .battery-section { margin-bottom: 24px; }
    .battery-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 10px;
    }
    .battery-label {
      font-size: 13px;
      color: #6b7280;
      font-weight: 500;
    }
    .battery-pct {
      font-size: 24px;
      font-weight: 700;
      letter-spacing: -1px;
    }
    .battery-pct.high { color: #22c55e; }
    .battery-pct.medium { color: #eab308; }
    .battery-pct.low { color: #ef4444; }
    .battery-pct.unknown { color: #6b7280; font-size: 14px; }

    .battery-track {
      height: 8px;
      background: rgba(255,255,255,0.06);
      border-radius: 4px;
      overflow: hidden;
    }
    .battery-fill {
      height: 100%;
      border-radius: 4px;
      transition: width 0.8s cubic-bezier(0.4, 0, 0.2, 1);
    }
    .battery-fill.high { background: linear-gradient(90deg, #22c55e, #4ade80); }
    .battery-fill.medium { background: linear-gradient(90deg, #eab308, #facc15); }
    .battery-fill.low { background: linear-gradient(90deg, #ef4444, #f87171); }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 4px;
      padding: 3px 10px;
      border-radius: 100px;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.3px;
    }
    .badge-ok { background: rgba(34,197,94,0.12); color: #4ade80; border: 1px solid rgba(34,197,94,0.2); }
    .badge-warn { background: rgba(234,179,8,0.12); color: #facc15; border: 1px solid rgba(234,179,8,0.2); }
    .badge-err { background: rgba(239,68,68,0.12); color: #f87171; border: 1px solid rgba(239,68,68,0.2); }

    .no-device {
      text-align: center;
      color: #4b5563;
      padding: 32px 0;
      font-size: 14px;
      line-height: 1.6;
    }

    .footer {
      display: flex;
      gap: 8px;
      margin-top: 8px;
    }
    .btn {
      flex: 1;
      padding: 11px;
      border: 1px solid rgba(99,102,241,0.2);
      background: rgba(99,102,241,0.08);
      color: #a5b4fc;
      border-radius: 12px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.2s;
      font-family: inherit;
    }
    .btn:hover {
      background: rgba(99,102,241,0.18);
      border-color: rgba(99,102,241,0.4);
      transform: translateY(-1px);
    }

    .stream-dot {
      display: inline-block;
      width: 6px; height: 6px;
      background: #22c55e;
      border-radius: 50%;
      margin-right: 6px;
      animation: blink 1.5s ease-in-out infinite;
    }
    @keyframes blink {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.3; }
    }

    .last-update {
      text-align: center;
      font-size: 11px;
      color: #374151;
      margin-top: 14px;
    }
  </style>
</head>
<body>
<div class="container">
<div class="card">
  <div class="header">
    <div class="dot" id="dot"></div>
    <h1>Magic Mouse Utility</h1>
  </div>

  <div class="mouse-visual" id="mouse-visual">
    <svg viewBox="0 0 100 160" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="bodyGrad" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stop-color="#e5e5ea"/>
          <stop offset="100%" stop-color="#c7c7cc"/>
        </linearGradient>
        <linearGradient id="topGrad" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stop-color="#f5f5f7"/>
          <stop offset="100%" stop-color="#e5e5ea"/>
        </linearGradient>
      </defs>
      <rect x="15" y="40" width="70" height="110" rx="35" fill="url(#bodyGrad)"/>
      <rect x="15" y="40" width="70" height="55" rx="35" fill="url(#topGrad)"/>
      <line x1="50" y1="42" x2="50" y2="92" stroke="#d1d1d6" stroke-width="1.5" opacity="0.6"/>
    </svg>
  </div>

  <div id="content">
    <div class="no-device"><span class="stream-dot"></span>Connecting...</div>
  </div>

  <div class="footer">
    <button class="btn" onclick="fetchData()">↻ Refresh</button>
    <button class="btn" onclick="showDiagnostics()">🔍 Diagnostics</button>
  </div>
  <div class="last-update" id="last-update"></div>
</div>
</div>

<script>
let evtSource;

function startSSE() {
  if (evtSource) evtSource.close();
  evtSource = new EventSource('/api/events');
  evtSource.onmessage = (e) => {
    try { render(JSON.parse(e.data)); } catch(err) {}
  };
  evtSource.onerror = () => {
    document.getElementById('content').innerHTML = '<div class="no-device">Connection lost. Retrying...</div>';
  };
}

async function fetchData() {
  try {
    const res = await fetch('/api/mouse');
    const d = await res.json();
    render(d);
  } catch(e) {
    document.getElementById('content').innerHTML = '<div class="no-device">Could not connect to service.</div>';
  }
}

function render(d) {
  const dot = document.getElementById('dot');
  dot.className = 'dot' + (d.connected ? ' connected' : '');
  document.getElementById('mouse-visual').className = 'mouse-visual' + (d.connected ? ' active' : '');

  if (!d.connected) {
    document.getElementById('content').innerHTML =
      '<div class="no-device">⚠️ No Magic Mouse detected.<br/>Make sure it is connected via Bluetooth.</div>';
    document.getElementById('last-update').textContent = 'Updated: ' + new Date().toLocaleTimeString();
    return;
  }

  const pct = d.battery_level || 0;
  const battAvail = d.battery_available;
  const pctClass = !battAvail ? 'unknown' : pct <= 20 ? 'low' : pct <= 50 ? 'medium' : 'high';
  const fillClass = pct <= 20 ? 'low' : pct <= 50 ? 'medium' : 'high';

  let batteryHTML;
  if (battAvail) {
    batteryHTML = ` + "`" + `
      <div class="battery-section">
        <div class="battery-header">
          <span class="battery-label">Battery</span>
          <span class="battery-pct ${pctClass}">${pct}%</span>
        </div>
        <div class="battery-track">
          <div class="battery-fill ${fillClass}" style="width:${pct}%"></div>
        </div>
      </div>` + "`" + `;
  } else {
    batteryHTML = ` + "`" + `
      <div class="battery-section">
        <div class="battery-header">
          <span class="battery-label">Battery</span>
          <span class="battery-pct unknown">Unavailable</span>
        </div>
        <div class="battery-track">
          <div class="battery-fill" style="width:0%"></div>
        </div>
      </div>` + "`" + `;
  }

  const rssiText = d.rssi ? d.rssi + ' dBm' : 'N/A';

  const driverHTML = d.driver_installed ? '' : ` + "`" + `
    <div style="margin-top:24px;">
      <button id="install-btn" class="btn" onclick="installDriver()" style="background:rgba(234,179,8,0.15); border-color:rgba(234,179,8,0.4); color:#facc15; font-weight:700; width:100%; padding:14px;">
        ⚙️ Install Scrolling Driver (Admin Req)
      </button>
      <div style="font-size:11px; color:#9ca3af; text-align:center; margin-top:8px;">Required to enable native Windows scrolling.</div>
    </div>
  ` + "`" + `;

  document.getElementById('content').innerHTML = ` + "`" + `
    ${driverHTML}
    <div class="info-grid">
      <div class="info-row">
        <span class="info-label">Name</span>
        <span class="info-val">${d.name}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Model</span>
        <span class="info-val">${d.model}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Bluetooth</span>
        <span class="info-val">${d.bluetooth_address || 'N/A'}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Signal</span>
        <span class="info-val">${rssiText}</span>
      </div>
      <div class="info-row">
        <span class="info-label">Device ID</span>
        <span class="info-val" style="font-size:11px">${d.device_id}</span>
      </div>
    </div>
    ${batteryHTML}
  ` + "`" + `;

  document.getElementById('last-update').innerHTML = '<span class="stream-dot"></span>Live — ' + new Date().toLocaleTimeString();
}

async function installDriver() {
  const btn = document.getElementById('install-btn');
  btn.innerText = 'Installing...';
  btn.disabled = true;
  
  try {
    const res = await fetch('/api/install', { method: 'POST' });
    const data = await res.json();
    if (res.ok) {
      alert("Success! Magic Mouse scrolling driver has been installed. The cursor might freeze for a second while it reloads.");
      fetchData();
    } else {
      alert("Installation Failed: \n" + (data.error || "Unknown error") + "\n\nMake sure you right-clicked and selected 'Run as Administrator'!");
      btn.innerText = '⚙️ Try Again As Admin';
      btn.disabled = false;
    }
  } catch(e) {
    alert("Network error installing driver: " + e.message);
    btn.innerText = '⚙️ Install Scrolling Driver (Admin Req)';
    btn.disabled = false;
  }
}

async function showDiagnostics() {
  try {
    const res = await fetch('/api/diagnostics');
    const data = await res.json();
    const pre = JSON.stringify(data, null, 2);
    document.getElementById('content').innerHTML =
      '<div style="max-height:300px;overflow-y:auto;background:rgba(0,0,0,0.3);border-radius:12px;padding:16px;font-size:11px;font-family:monospace;white-space:pre-wrap;color:#94a3b8;border:1px solid rgba(255,255,255,0.05);">' + pre + '</div>';
  } catch(e) {
    document.getElementById('content').innerHTML = '<div class="no-device">Diagnostics failed.</div>';
  }
}

// Start with SSE for live updates, with fetch fallback
fetchData();
startSSE();
</script>
</body>
</html>`
