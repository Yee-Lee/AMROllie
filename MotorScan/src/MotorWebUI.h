/**
 * ============================================================================
 * 檔案: MotorWebUI.h
 * 描述: Ollie 控制監控系統 - 終端機大容量數據支援
 * 更新: 提升終端機緩存至 500 筆資料，優化大量資料下的捲動效能與 CSS 布局。
 * ============================================================================
 */
#ifndef MOTOR_WEBUI_H
#define MOTOR_WEBUI_H

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Ollie 控制監控系統</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg-dark: #0f172a;
            --panel-bg: #1e293b;
            --accent: #6366f1;
            --success: #10b981;
            --danger: #ef4444;
            --text-main: #f8fafc;
            --text-dim: #94a3b8;
        }
        body { 
            font-family: 'Segoe UI', 'Consolas', monospace; background: var(--bg-dark); color: var(--text-main); 
            margin: 0; padding: 0; height: 100vh; width: 100vw; overflow: hidden; box-sizing: border-box; 
        }
        .container { width: 100%; height: 100%; padding: 10px 15px; display: flex; flex-direction: column; gap: 10px; box-sizing: border-box; }
        
        .header-row { display: flex; justify-content: space-between; align-items: center; flex-shrink: 0; padding-bottom: 8px; border-bottom: 1px solid #334155; }
        .title-group { display: flex; align-items: center; gap: 15px; }
        .title-group h3 { margin: 0; font-size: 1.1rem; color: var(--accent); letter-spacing: 0.5px; white-space: nowrap; }
        
        .motor-selector { display: flex; align-items: center; gap: 8px; background: var(--panel-bg); padding: 4px 10px; border-radius: 6px; }
        select { background: #0f172a; color: #fff; border: 1px solid var(--accent); padding: 3px 6px; border-radius: 4px; cursor: pointer; font-size: 0.85rem; outline: none; }
        select:disabled { opacity: 0.5; cursor: not-allowed; border-color: #334155; }

        .status-box { display: flex; gap: 10px; font-size: 0.75rem; font-weight: bold; align-items: center; white-space: nowrap; }
        .status-item { padding: 4px 8px; border-radius: 4px; background: var(--panel-bg); border: 1px solid #334155; }
        
        .main-layout { display: flex; gap: 15px; flex: 1; min-height: 0; }
        .chart-section { background: #000; padding: 5px; border-radius: 8px; border: 1px solid #334155; flex: 2; position: relative; min-height: 0; }
        
        .terminal-section { 
            background: #000; padding: 8px 12px; border-radius: 8px; border: 1px solid #334155; 
            flex: 1; display: flex; flex-direction: column; min-height: 0; min-width: 220px;
        }

        #terminal { flex: 1; overflow-y: auto; font-size: 0.75rem; color: var(--success); white-space: pre-wrap; line-height: 1.4; scrollbar-width: thin; scrollbar-color: var(--accent) #000; }
        #terminal::-webkit-scrollbar { width: 4px; }
        #terminal::-webkit-scrollbar-track { background: #000; }
        #terminal::-webkit-scrollbar-thumb { background: var(--accent); border-radius: 4px; }

        .term-row { display: flex; border-bottom: 1px solid #111; padding: 2px 0; align-items: center; }
        .term-ts { color: var(--text-dim); width: 45px; flex-shrink: 0; }
        .term-val-rpm { color: #fff; width: 75px; text-align: right; font-weight: bold; flex-shrink: 0; }
        .term-val-pwm { color: var(--accent); flex: 1; text-align: right; flex-shrink: 0; }

        .footer-row { display: flex; justify-content: space-between; align-items: center; flex-shrink: 0; padding-top: 5px; }
        .manual-test { display: flex; align-items: center; font-size: 0.85rem; width: 140px; }
        .btn-container { flex: 1; display: flex; justify-content: center; }
        .placeholder { width: 140px; }
        
        button { 
            padding: 8px 40px; font-size: 1rem; cursor: pointer; border: none; border-radius: 6px; 
            color: white; font-weight: 700; transition: all 0.2s; text-transform: uppercase; letter-spacing: 1px;
        }
        .btn-idle { background: var(--accent); border-bottom: 3px solid #4338ca; }
        .btn-running { background: var(--danger); border-bottom: 3px solid #991b1b; }
        button:active { transform: translateY(2px); border-bottom: none; margin-bottom: 3px; }

        @media (orientation: portrait) {
            .header-row { flex-direction: column; align-items: stretch; gap: 10px; }
            .title-group { justify-content: space-between; }
            .status-box { justify-content: space-between; }
            .main-layout { flex-direction: column; }
            .footer-row { flex-direction: column; gap: 15px; }
            .placeholder, .manual-test { width: auto; justify-content: center; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header-row">
            <div class="title-group">
                <h3>OLLIE_SYS_v3.3</h3>
                <div class="motor-selector">
                    <span style="font-size: 0.75rem; color: var(--text-dim);">MOTOR:</span>
                    <select id="motorSelect" onchange="changeMotor()">
                        <option value="0">LEFT</option>
                        <option value="1">RIGHT</option>
                    </select>
                    <span id="motorLockMsg" style="font-size: 0.7rem; color: var(--danger); display: none;"> [LOCKED]</span>
                </div>
            </div>
            <div class="status-box">
                <div class="status-item">SCAN: <span id="scannerState" style="color:var(--text-dim)">IDLE</span></div>
                <div class="status-item">SYS: <span id="systemState" style="color:var(--text-dim)">IDLE</span></div>
                <div id="statusTag" style="color: var(--text-dim); margin-left: 5px;">● DISCONNECTED</div>
            </div>
        </div>

        <div class="main-layout">
            <div class="chart-section">
                <canvas id="rpmChart"></canvas>
            </div>
            <div class="terminal-section">
                <div style="color:var(--text-dim); font-size:0.7rem; margin-bottom:6px; border-bottom:1px solid #334155; padding-bottom:4px; display:flex; justify-content:space-between;">
                    <span>TIME</span>
                    <span>RPM / PWM (500)</span>
                </div>
                <div id="terminal"></div>
            </div>
        </div>

        <div class="footer-row">
            <div class="manual-test">
                <input type="checkbox" id="manualCheck" style="width:16px; height:16px; cursor:pointer; margin:0;">
                <label for="manualCheck" style="margin-left:6px; color:var(--text-dim); cursor:pointer;">Manual Test</label>
            </div>
            <div class="btn-container">
                <button id="stateBtn" class="btn-idle" onclick="toggleState()">START SYSTEM</button>
            </div>
            <div class="placeholder"></div>
        </div>
    </div>

    <script>
        let isRunning = false;
        let ws;
        let startTime = 0;
        const terminal = document.getElementById('terminal');
        const ctx = document.getElementById('rpmChart').getContext('2d');
        const motorSelect = document.getElementById('motorSelect');
        const motorLockMsg = document.getElementById('motorLockMsg');

        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [{
                    label: 'Motor RPM',
                    borderColor: '#6366f1',
                    data: [], 
                    fill: false,
                    pointRadius: 1,
                    borderWidth: 2,
                    tension: 0.2
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                scales: {
                    x: { 
                        type: 'linear',
                        min: 0, max: 40, 
                        title: { display: true, text: 'Time (s)', color: '#94a3b8' },
                        grid: { color: '#1e293b' }, 
                        ticks: { color: '#94a3b8' }
                    },
                    y: { 
                        min: 0, max: 500,
                        title: { display: true, text: 'RPM', color: '#94a3b8' },
                        grid: { color: '#1e293b' },
                        ticks: { color: '#94a3b8' }
                    }
                },
                plugins: { legend: { display: false } }
            }
        });

        function initWebSocket() {
            ws = new WebSocket(`ws://${window.location.hostname}/ws`);
            const statusTag = document.getElementById('statusTag');
            
            ws.onopen = () => { statusTag.textContent = "● CONNECTED"; statusTag.style.color = "#10b981"; };
            ws.onclose = () => { statusTag.textContent = "● DISCONNECTED"; statusTag.style.color = "#ef4444"; setTimeout(initWebSocket, 2000); };
            
            ws.onmessage = (e) => {
                if(!isRunning) return;
                if(e.data.startsWith("D,")) {
                    const parts = e.data.split(",");
                    if(parts.length < 5) return;
                    const rpm = parseFloat(parts[2]);
                    const pwm = parts[3];
                    const sState = parts[4];
                    const timeS = ((Date.now() - startTime)/1000);
                    
                    document.getElementById('scannerState').textContent = sState;
                    
                    const row = document.createElement('div');
                    row.className = 'term-row';
                    row.innerHTML = `<span class="term-ts">${timeS.toFixed(1)}s</span><span class="term-val-rpm">R: ${rpm.toFixed(1)}</span><span class="term-val-pwm">P: ${pwm}</span>`;
                    
                    terminal.appendChild(row);

                    // 提升緩存上限至 500 筆資料
                    if(terminal.childNodes.length > 500) {
                        terminal.removeChild(terminal.firstChild);
                    }

                    // 強化的自動捲動機制
                    requestAnimationFrame(() => {
                        terminal.scrollTop = terminal.scrollHeight;
                    });

                    chart.data.datasets[0].data.push({x: timeS, y: rpm});
                    if(timeS > chart.options.scales.x.max) {
                        chart.options.scales.x.min = timeS - 35;
                        chart.options.scales.x.max = timeS + 5;
                    }
                    chart.update('none');
                }
            };
        }

        function changeMotor() {
            if(!isRunning) {
                ws.send("MOTOR," + motorSelect.value);
            }
        }

        function toggleState() {
            isRunning = !isRunning;
            const btn = document.getElementById('stateBtn');
            const stateText = document.getElementById('systemState');
            const scanText = document.getElementById('scannerState');
            const manualCheck = document.getElementById('manualCheck');
            
            if(isRunning) {
                motorSelect.disabled = true;
                manualCheck.disabled = true;
                motorLockMsg.style.display = "inline";
                startTime = Date.now();
                terminal.innerHTML = '';
                chart.data.datasets[0].data = [];
                chart.options.scales.x.min = 0;
                chart.options.scales.x.max = 40;
                chart.update();
                ws.send("STATE,1," + (manualCheck.checked ? "1" : "0"));
                stateText.textContent = "RUNNING";
                stateText.style.color = "#10b981";
                btn.textContent = "Stop System"; btn.className = "btn-running";
            } else {
                motorSelect.disabled = false;
                manualCheck.disabled = false;
                motorLockMsg.style.display = "none";
                ws.send("STATE,0");
                stateText.textContent = "IDLE";
                stateText.style.color = "#94a3b8";
                scanText.textContent = "IDLE";
                btn.textContent = "Start System"; btn.className = "btn-idle";
            }
        }
        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

class IMotor;

class MotorWebUI {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    int* _state;       
    bool* _manualMode;
    IMotor* _motor;    
    int selectedMotorIdx = 0; 
    bool motorChangedFlag = true; 
    unsigned long lastBroadcastTime = 0;

    void onEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_DATA) {
            data[len] = 0;
            String msg = (char*)data;
            if (msg.startsWith("STATE,")) {
                String payload = msg.substring(6);
                int commaIdx = payload.indexOf(',');
                if (commaIdx != -1) {
                    *_state = payload.substring(0, commaIdx).toInt();
                    if (_manualMode) *_manualMode = (payload.substring(commaIdx + 1).toInt() == 1);
                } else {
                    *_state = payload.toInt();
                }

                if (*_state == 0 && _motor != nullptr) {
                    _motor->stop(); 
                }
            } else if (msg.startsWith("MOTOR,")) {
                int newIdx = msg.substring(6).toInt();
                if (newIdx != selectedMotorIdx) {
                    selectedMotorIdx = newIdx;
                    motorChangedFlag = true; 
                }
            }
        }
    }

public:
    MotorWebUI(IMotor* m, int* s, bool* manual) : server(80), ws("/ws"), _motor(m), _state(s), _manualMode(manual) {}

    void setCurrentMotor(IMotor* m) { _motor = m; }

    bool hasMotorChanged() {
        if (motorChangedFlag) {
            motorChangedFlag = false;
            return true;
        }
        return false;
    }

    void begin(const char* ssid, const char* pass) {
        Serial.printf("Connecting to %s ", ssid);
        WiFi.begin(ssid, pass);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        
        WiFi.setSleep(false); 

        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        
        ws.onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
            this->onEvent(s, c, t, a, d, l);
        });
        server.addHandler(&ws);
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
            r->send(200, "text/html", index_html);
        });
        server.begin();
    }

    int getSelectedMotorIndex() const { return selectedMotorIdx; }

    void broadcastData(unsigned long t, float current, float pwm, const char* scannerState = "SCAN") {
        if (ws.count() > 0 && ws.availableForWriteAll()) {
            if (millis() - lastBroadcastTime < 50) return; 
            lastBroadcastTime = millis();
            char buf[128];
            snprintf(buf, sizeof(buf), "D,%lu,%.1f,%.0f,%s", t, current, pwm, scannerState);
            ws.textAll(buf);
        }
    }

    void cleanup() { ws.cleanupClients(); }
};

#endif