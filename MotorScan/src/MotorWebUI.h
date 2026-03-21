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
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
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
        body { font-family: 'Segoe UI', 'Consolas', monospace; background: var(--bg-dark); padding: 20px; color: var(--text-main); margin: 0; overflow: hidden; }
        .container { max-width: 1100px; margin: 0 auto; background: var(--panel-bg); padding: 20px; border-radius: 12px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
        
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 10px; border-bottom: 1px solid #334155; padding-bottom: 10px; }
        .status-box { display: flex; gap: 15px; font-size: 0.8rem; font-weight: bold; }
        .status-item { padding: 2px 8px; border-radius: 4px; background: #0f172a; border: 1px solid #334155; }
        
        .motor-selector { display: flex; align-items: center; gap: 10px; margin-bottom: 15px; background: #0f172a; padding: 10px; border-radius: 8px; }
        select { background: #1e293b; color: #fff; border: 1px solid var(--accent); padding: 5px 15px; border-radius: 4px; cursor: pointer; font-family: inherit; }
        select:disabled { opacity: 0.5; cursor: not-allowed; border-color: #334155; }

        .main-layout { display: grid; grid-template-columns: 1.5fr 1fr; gap: 15px; height: 440px; margin-bottom: 15px; }
        .chart-section { background: #000; padding: 10px; border-radius: 8px; border: 1px solid #334155; position: relative; overflow: hidden; }
        
        .terminal-section { 
            background: #000; 
            padding: 15px; 
            border-radius: 8px; 
            border: 1px solid #334155; 
            display: flex; 
            flex-direction: column; 
            overflow: hidden; 
            height: 100%;
            box-sizing: border-box;
        }

        #terminal { 
            flex: 1; 
            overflow-y: auto; 
            font-size: 0.82rem; 
            color: var(--success); 
            white-space: pre-wrap; 
            line-height: 1.3;
            scrollbar-width: thin;
            scrollbar-color: var(--accent) #000;
        }

        #terminal::-webkit-scrollbar { width: 6px; }
        #terminal::-webkit-scrollbar-track { background: #000; }
        #terminal::-webkit-scrollbar-thumb { background: var(--accent); border-radius: 10px; }

        .term-row { display: flex; border-bottom: 1px solid #111; padding: 1px 0; align-items: center; }
        .term-ts { color: var(--text-dim); width: 60px; flex-shrink: 0; font-size: 0.75rem; }
        .term-val-rpm { color: #fff; width: 90px; text-align: right; font-weight: bold; flex-shrink: 0; }
        .term-val-pwm { color: var(--accent); width: 80px; text-align: right; flex-shrink: 0; }

        .btn-container { text-align: center; }
        button { 
            padding: 12px 60px; font-size: 1.1rem; cursor: pointer; border: none; border-radius: 6px; 
            color: white; font-weight: 700; transition: all 0.2s; text-transform: uppercase; letter-spacing: 1px;
        }
        .btn-idle { background: var(--accent); border-bottom: 4px solid #4338ca; }
        .btn-running { background: var(--danger); border-bottom: 4px solid #991b1b; }
        button:active { transform: translateY(2px); border-bottom: none; margin-bottom: 4px; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h3 style="margin:0; color:var(--accent);">OLLIE_SYSTEM_v3.3</h3>
            <div class="status-box">
                <div class="status-item">SCAN: <span id="scannerState" style="color:var(--text-dim)">IDLE</span></div>
                <div class="status-item">SYS: <span id="systemState" style="color:var(--text-dim)">IDLE</span></div>
                <div id="statusTag" style="color: var(--text-dim);">● DISCONNECTED</div>
            </div>
        </div>

        <div class="motor-selector">
            <span style="font-size: 0.85rem; color: var(--text-dim);">SELECT MOTOR:</span>
            <select id="motorSelect" onchange="changeMotor()">
                <option value="0">LEFT MOTOR</option>
                <option value="1">RIGHT MOTOR</option>
            </select>
            <span id="motorLockMsg" style="font-size: 0.75rem; color: var(--danger); display: none;"> [LOCKED]</span>
        </div>

        <div class="main-layout">
            <div class="chart-section">
                <canvas id="rpmChart"></canvas>
            </div>
            <div class="terminal-section">
                <div style="color:var(--text-dim); font-size:0.7rem; margin-bottom:10px; border-bottom:1px solid #333; padding-bottom:5px; display:flex; justify-content:space-between;">
                    <span>TIMESTAMP</span>
                    <span>RPM / PWM (BUFF: 500)</span>
                </div>
                <div id="terminal"></div>
            </div>
        </div>

        <div class="btn-container" style="display:flex; align-items:center; justify-content:center; gap:20px;">
            <div style="display:flex; align-items:center;">
                <input type="checkbox" id="manualCheck" style="width:18px; height:18px; cursor:pointer;">
                <label for="manualCheck" style="margin-left:8px; color:var(--text-dim); cursor:pointer;">manual test</label>
            </div>
            <button id="stateBtn" class="btn-idle" onclick="toggleState()">Start System</button>
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