
/**
 * ============================================================================
 * 檔案: MotorWebUI.h
 * 描述: 改進 UI 為逐行打印終端面板，支援自動重置
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
    <meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ollie 控制監控系統</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { font-family: 'Segoe UI', system-ui, sans-serif; background: #0f172a; padding: 20px; color: #f8fafc; margin: 0; }
        .container { max-width: 1200px; margin: 0 auto; background: #1e293b; padding: 25px; border-radius: 16px; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; border-bottom: 1px solid #334155; padding-bottom: 15px; }
        
        .mode-selector { background: #334155; padding: 10px 20px; border-radius: 12px; display: flex; align-items: center; gap: 15px; }
        .switch { position: relative; display: inline-block; width: 50px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #1e293b; transition: .4s; border-radius: 24px; border: 1px solid #475569; }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 4px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
        input:checked + .slider { background-color: #6366f1; }
        input:checked + .slider:before { transform: translateX(26px); }
        input:disabled + .slider { opacity: 0.5; cursor: not-allowed; }

        .main-layout { display: flex; gap: 20px; height: 400px; margin-bottom: 20px; }
        .chart-section { flex: 2; background: #0f172a; padding: 15px; border-radius: 12px; border: 1px solid #334155; }
        .terminal-section { flex: 1; background: #000; padding: 10px; border-radius: 12px; border: 1px solid #334155; display: flex; flex-direction: column; }
        #terminal { flex: 1; overflow-y: auto; font-family: 'Consolas', monospace; font-size: 0.8rem; color: #10b981; scroll-behavior: smooth; }
        
        .controls-grid { display: flex; flex-direction: column; gap: 15px; }
        .param-row { display: grid; grid-template-columns: repeat(4, 1fr); gap: 15px; padding: 15px; border-radius: 12px; border: 1px solid #334155; transition: 0.3s; }
        .param-row.active { background: rgba(99, 102, 241, 0.05); border-color: #6366f1; }
        .param-row.disabled { opacity: 0.4; pointer-events: none; background: #1e293b; filter: grayscale(1); }
        
        .control-group label { font-size: 0.75rem; color: #94a3b8; display: block; margin-bottom: 5px; font-weight: 600; }
        input[type="number"] { width: 100%; padding: 8px; background: #0f172a; border: 1px solid #334155; border-radius: 6px; color: #f8fafc; box-sizing: border-box; }
        
        .btn-container { text-align: center; margin-top: 20px; }
        button { padding: 12px 60px; font-size: 1rem; cursor: pointer; border: none; border-radius: 50px; color: white; font-weight: 700; transition: 0.3s; }
        .btn-idle { background: #10b981; box-shadow: 0 4px 0 #065f46; }
        .btn-running { background: #ef4444; box-shadow: 0 4px 0 #991b1b; }
        button:active { transform: translateY(2px); box-shadow: none; }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h2 style="margin:0;">Ollie Console</h2>
            <div class="mode-selector">
                <span id="modeText" style="font-size: 0.9rem; font-weight: 600;">PID 控制模式</span>
                <label class="switch">
                    <input type="checkbox" id="modeSwitch" onchange="updateModeUI()">
                    <span class="slider"></span>
                </label>
            </div>
            <div id="statusTag" style="color: #94a3b8; font-size: 0.8rem;">DISCONNECTED</div>
        </div>
        
        <div class="main-layout">
            <div class="chart-section"><canvas id="rpmChart"></canvas></div>
            <div class="terminal-section"><div id="terminal"></div></div>
        </div>

        <div class="controls-grid">
            <!-- PID 參數列 -->
            <div id="pidRow" class="param-row active">
                <div class="control-group"><label>PID Target RPM</label><input type="number" id="target" value="150"></div>
                <div class="control-group"><label>Kp</label><input type="number" id="kp" value="0.8" step="0.1"></div>
                <div class="control-group"><label>Ki</label><input type="number" id="ki" value="0.1" step="0.01"></div>
                <div class="control-group"><label>Kd</label><input type="number" id="kd" value="0.01" step="0.01"></div>
            </div>
            
            <!-- PWM 參數列 -->
            <div id="pwmRow" class="param-row disabled">
                <div class="control-group"><label>PWM Target RPM (觀測)</label><input type="number" id="pwmTarget" value="150"></div>
                <div class="control-group"><label>Direct PWM Out</label><input type="number" id="pwmDirect" value="180" min="0" max="255"></div>
                <div class="control-group"></div><div class="control-group"></div>
            </div>
        </div>
        
        <div class="btn-container">
            <button id="stateBtn" class="btn-idle" onclick="toggleState()">START SYSTEM</button>
        </div>
    </div>

    <script>
        let isRunning = false;
        let ws;
        let startTime = 0;
        const ctx = document.getElementById('rpmChart').getContext('2d');
        const chart = new Chart(ctx, {
            type: 'line', data: { labels: [], datasets: [
                { label: 'Current', borderColor: '#3498db', data: [], fill: false, pointRadius: 0, tension: 0.3 },
                { label: 'Target', borderColor: '#ef4444', borderDash: [5,5], data: [], fill: false, pointRadius: 0 }
            ]},
            options: { responsive: true, maintainAspectRatio: false, animation: false, scales: { x: { display: false }, y: { grid: { color: '#334155' } } } }
        });

        function updateModeUI() {
            const isPWM = document.getElementById('modeSwitch').checked;
            document.getElementById('modeText').textContent = isPWM ? "PWM 監控模式" : "PID 控制模式";
            document.getElementById('pidRow').className = isPWM ? "param-row disabled" : "param-row active";
            document.getElementById('pwmRow').className = isPWM ? "param-row active" : "param-row disabled";
        }

        function initWebSocket() {
            ws = new WebSocket(`ws://${window.location.hostname}/ws`);
            ws.onopen = () => document.getElementById('statusTag').textContent = "CONNECTED";
            ws.onmessage = (e) => {
                if(!e.data.startsWith("D,")) return;
                const [_, ms, curr, target, pwm] = e.data.split(",");
                const c = parseFloat(curr), t = parseFloat(target);
                
                const line = document.createElement('div');
                line.innerHTML = `[${((Date.now()-startTime)/1000).toFixed(1)}s] RPM:${c.toFixed(1)} T:${t} PWM:${pwm}`;
                const term = document.getElementById('terminal');
                term.appendChild(line);
                term.scrollTop = term.scrollHeight;
                if(term.childNodes.length > 50) term.removeChild(term.firstChild);

                chart.data.labels.push("");
                chart.data.datasets[0].data.push(c);
                chart.data.datasets[1].data.push(t);
                if(chart.data.labels.length > 60) { chart.data.labels.shift(); chart.data.datasets[0].data.shift(); chart.data.datasets[1].data.shift(); }
                chart.update();
            };
        }

        function toggleState() {
            isRunning = !isRunning;
            const btn = document.getElementById('stateBtn');
            const modeSwitch = document.getElementById('modeSwitch');
            
            if(isRunning) {
                startTime = Date.now();
                document.getElementById('terminal').innerHTML = "";
                const isPWM = modeSwitch.checked;
                const modeVal = isPWM ? 1 : 0;
                
                let params = "";
                if(!isPWM) {
                    params = `PARAM,0,${document.getElementById('target').value},${document.getElementById('kp').value},${document.getElementById('ki').value},${document.getElementById('kd').value}`;
                } else {
                    params = `PARAM,1,${document.getElementById('pwmTarget').value},${document.getElementById('pwmDirect').value},0,0`;
                }
                
                ws.send(params);
                setTimeout(() => ws.send("STATE,1"), 50);
                btn.textContent = "STOP SYSTEM"; btn.className = "btn-running";
                modeSwitch.disabled = true;
            } else {
                ws.send("STATE,0");
                btn.textContent = "START SYSTEM"; btn.className = "btn-idle";
                modeSwitch.disabled = false;
            }
        }
        window.onload = initWebSocket;
    </script>
</body>
</html>
)rawliteral";

class MotorWebUI {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    PIDController* _pid;
    IMotor** _motor;
    int* _state;
    int* _mode; // 0: PID, 1: PWM
    int* _manualPWM;

    void onEvent(AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_DATA) {
            data[len] = 0; String msg = (char*)data;
            if (msg.startsWith("STATE,")) {
                *_state = msg.substring(6).toInt();
                if (*_state == 0) { (*_motor)->stop(); _pid->reset(); }
            } else if (msg.startsWith("PARAM,")) {
                // 格式: PARAM,mode,target,v1,v2,v3
                int c1 = msg.indexOf(',', 6);
                int c2 = msg.indexOf(',', c1+1);
                int c3 = msg.indexOf(',', c2+1);
                int c4 = msg.indexOf(',', c3+1);
                
                *_mode = msg.substring(6, c1).toInt();
                float target = msg.substring(c1+1, c2).toFloat();
                _pid->setTarget(target);

                if (*_mode == 0) { // PID Mode
                    _pid->setTunings(msg.substring(c2+1, c3).toFloat(), msg.substring(c3+1, c4).toFloat(), msg.substring(c4+1).toFloat());
                } else { // PWM Mode
                    *_manualPWM = msg.substring(c2+1, c3).toInt();
                }
            }
        }
    }

public:
    MotorWebUI(PIDController* p, IMotor** m, int* s, int* md, int* mp) 
        : server(80), ws("/ws"), _pid(p), _motor(m), _state(s), _mode(md), _manualPWM(mp) {}

    void begin(const char* ssid, const char* pass) {
        WiFi.begin(ssid, pass);
        while (WiFi.status() != WL_CONNECTED) delay(500);
        Serial.println(WiFi.localIP());
        ws.onEvent([this](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
            this->onEvent(s, c, t, a, d, l);
        });
        server.addHandler(&ws);
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send(200, "text/html", index_html); });
        server.begin();
    }

    void broadcastData(unsigned long t, float current, float target, float pwm) {
        if (ws.count() > 0) {
            char buf[128]; 
            snprintf(buf, sizeof(buf), "D,%lu,%.1f,%.1f,%.0f", t, current, target, pwm);
            ws.textAll(buf);
        }
    }
    void cleanup() { ws.cleanupClients(); }
};

#endif

