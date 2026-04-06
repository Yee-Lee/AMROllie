#ifndef PID_TEST_PAGE_H
#define PID_TEST_PAGE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

static const char index_html[] PROGMEM = R"raw(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Ollie PID Tuner</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root { --bg: #121212; --panel: #1e1e1e; --text: #e0e0e0; --accent: #4caf50; --border: #333; }
        body { background: var(--bg); color: var(--text); font-family: system-ui, -apple-system, sans-serif; margin: 0; padding: 10px; height: 100vh; width: 100vw; box-sizing: border-box; display: flex; gap: 10px; overflow: hidden; }
        * { box-sizing: border-box; }
        ::-webkit-scrollbar { width: 6px; height: 6px; }
        ::-webkit-scrollbar-track { background: var(--bg); }
        ::-webkit-scrollbar-thumb { background: var(--border); border-radius: 3px; }
        
        #chart-area { flex: 1; display: flex; flex-direction: column; gap: 10px; min-width: 0; }
        #chart-wrapper { flex: 1.2; border: 1px solid var(--border); background: var(--panel); border-radius: 6px; overflow-x: auto; position: relative; min-height: 0; }
        #chart-box { height: 100%; width: 100%; position: relative; }
        #log-content { flex: 1; border: 1px solid var(--border); background: var(--panel); border-radius: 6px; font-family: 'Courier New', monospace; font-size: 13px; overflow-y: auto; padding: 8px; display: flex; flex-direction: column; }
        .log-row { display: flex; justify-content: space-between; border-bottom: 1px solid #2a2a2a; padding: 4px 0; color: #bbb; }
        
        #ctrl-area { width: 240px; display: flex; flex-direction: column; gap: 10px; overflow-y: auto; padding-right: 2px; }
        button { background: var(--accent); color: #fff; font-size: 16px; font-weight: bold; border: none; border-radius: 6px; padding: 12px; cursor: pointer; transition: 0.2s; flex-shrink: 0; letter-spacing: 1px; }
        button:active { filter: brightness(0.9); }
        button.running { background: #555; color: #fff; }
        
        .panel { border: 1px solid var(--border); border-radius: 6px; padding: 10px; background: var(--panel); flex-shrink: 0; }
        .panel-title { font-size: 12px; font-weight: bold; color: #888; border-bottom: 1px solid var(--border); margin-bottom: 8px; padding-bottom: 4px; text-transform: uppercase; }
        .grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px; }
        .grid-2 { display: grid; grid-template-columns: repeat(2, 1fr); gap: 6px; }
        .input-group { display: flex; flex-direction: column; }
        label { font-size: 10px; color: #888; margin-bottom: 3px; }
        input { background: var(--bg); color: var(--text); border: 1px solid var(--border); border-radius: 4px; padding: 6px; font-family: inherit; font-size: 13px; width: 100%; text-align: center; }
        input:focus { outline: none; border-color: var(--accent); }
    </style>
</head>
<body>
    <div id="chart-area">
        <div id="chart-wrapper">
            <div id="chart-box">
                <canvas id="pidChart"></canvas>
            </div>
        </div>
        <div id="log-content"></div>
    </div>
    
    <div id="ctrl-area">
        <button id="btn-toggle" onclick="toggleTest()">START</button>
        
        <div class="panel">
            <div class="panel-title">Left PID</div>
            <div class="grid-3">
                <div class="input-group"><label>Kp</label><input type="number" step="0.1" id="lkp" value="2.0"></div>
                <div class="input-group"><label>Ki</label><input type="number" step="0.1" id="lki" value="3.5"></div>
                <div class="input-group"><label>Kd</label><input type="number" step="0.001" id="lkd" value="0.005"></div>
            </div>
        </div>
        
        <div class="panel">
            <div class="panel-title">Right PID</div>
            <div class="grid-3">
                <div class="input-group"><label>Kp</label><input type="number" step="0.1" id="rkp" value="2.0"></div>
                <div class="input-group"><label>Ki</label><input type="number" step="0.1" id="rki" value="3.5"></div>
                <div class="input-group"><label>Kd</label><input type="number" step="0.001" id="rkd" value="0.005"></div>
            </div>
        </div>
        
        <div class="panel">
            <div class="panel-title">Test Profile</div>
            <div class="grid-2">
                <div class="input-group"><label>Start</label><input type="number" id="start" value="60"></div>
                <div class="input-group"><label>End</label><input type="number" id="end" value="100"></div>
                <div class="input-group"><label>Step</label><input type="number" id="step" value="0"></div>
                <div class="input-group"><label>Int(ms)</label><input type="number" id="int" value="1000"></div>
                <div class="input-group" style="grid-column: span 2;"><label>Duration(s)</label><input type="number" id="dur" value="10"></div>
            </div>
        </div>
    </div>

    <script>
        let wakeLock = null;
        async function requestWakeLock() {
            try {
                if ('wakeLock' in navigator) {
                    wakeLock = await navigator.wakeLock.request('screen');
                }
            } catch (err) {
                console.error(`${err.name}, ${err.message}`);
            }
        }
        requestWakeLock();
        document.addEventListener('visibilitychange', () => {
            if (wakeLock !== null && document.visibilityState === 'visible') requestWakeLock();
        });

        Chart.defaults.color = '#888';
        Chart.defaults.font.family = "system-ui, -apple-system, sans-serif";
        var ctx = document.getElementById('pidChart').getContext('2d');
        var pidChart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [
                    { label: 'TGT', borderColor: '#888', borderDash: [4, 4], data: [], pointRadius: 0, borderWidth: 1.5, tension: 0.1 },
                    { label: 'L', borderColor: '#4caf50', data: [], pointRadius: 0, borderWidth: 2, tension: 0.1 },
                    { label: 'R', borderColor: '#ccc', data: [], pointRadius: 0, borderWidth: 2, tension: 0.1 }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false, 
                animation: false,
                plugins: {
                    legend: { labels: { color: '#ccc', font: { size: 11 } }, position: 'top', align: 'end' }
                },
                scales: {
                    x: {
                        type: 'linear', min: 0, max: 10, 
                        title: { display: false },
                        grid: { color: '#222' },
                        ticks: { font: { size: 10 } }
                    },
                    y: { 
                        title: { display: false },
                        grid: { color: '#222' },
                        ticks: { font: { size: 10 } }
                    }
                }
            }
        });

        // WebSocket 設置
        var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
        var isRunning = false;
        var logContent = document.getElementById('log-content');
        
        ws.onmessage = function(event) {
            var d = JSON.parse(event.data);
            
            if (d.type === "DONE") {
                stopUIState();
                return;
            }

            var t_sec = d.time / 1000.0; 
            var t_ms = d.time; 
            
            pidChart.data.datasets[0].data.push({x: t_sec, y: d.target});
            pidChart.data.datasets[1].data.push({x: t_sec, y: d.left});
            pidChart.data.datasets[2].data.push({x: t_sec, y: d.right});

            var wrapper = document.getElementById('chart-wrapper');
            var box = document.getElementById('chart-box');
            var isAtEnd = (wrapper.scrollLeft + wrapper.clientWidth >= wrapper.scrollWidth - 50);

            if (t_sec > 10) {
                box.style.width = (t_sec * 10.0) + "%";
                pidChart.options.scales.x.max = t_sec;
            }

            pidChart.update('none');

            if (isAtEnd) {
                wrapper.scrollLeft = wrapper.scrollWidth;
            }

            var logRow = document.createElement('div');
            logRow.className = 'log-row';
            logRow.innerHTML = `<span style="color:#888; width:65px;">[${t_ms}ms]</span><span style="width:60px;">T:${d.target.toFixed(1)}</span><span style="color:#4caf50; width:60px;">L:${d.left.toFixed(1)}</span><span style="color:#ccc; width:60px;">R:${d.right.toFixed(1)}</span>`;
            logContent.appendChild(logRow);
            logContent.scrollTop = logContent.scrollHeight;
        };

        function stopUIState() {
            var btn = document.getElementById('btn-toggle');
            var inputs = document.querySelectorAll('input');
            isRunning = false;
            btn.innerText = "START";
            btn.classList.remove('running');
            inputs.forEach(i => i.disabled = false);
        }

        function toggleTest() {
            var btn = document.getElementById('btn-toggle');
            var inputs = document.querySelectorAll('input');

            if (isRunning) {
                ws.send("STOP");
                stopUIState();
            } else {
                pidChart.data.datasets.forEach(ds => ds.data = []);
                pidChart.options.scales.x.min = 0;
                pidChart.options.scales.x.max = 10;
                pidChart.update();
                document.getElementById('chart-box').style.width = '100%';
                logContent.innerHTML = '';

                var ids = ['lkp','lki','lkd','rkp','rki','rkd','start','end','step','int','dur'];
                var vals = ids.map(id => document.getElementById(id).value);
                ws.send("START," + vals.join(","));
                
                isRunning = true;
                btn.innerText = "STOP";
                btn.classList.add('running');
                inputs.forEach(i => i.disabled = true);
            }
        }
    </script>
</body>
</html>
)raw";

class PIDTestPage {
public:
    volatile bool has_new_command = false;
    char last_command[128] = {0};

    inline PIDTestPage() : _ws("/ws") {}
    
    inline void attachToServer(AsyncWebServer* server) {
        _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            if (type == WS_EVT_DATA) {
                AwsFrameInfo *info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    char msg[128];
                    size_t copy_len = (len < sizeof(msg) - 1) ? len : sizeof(msg) - 1;
                    memcpy(msg, data, copy_len);
                    msg[copy_len] = '\0';
                    
                    // 將網頁傳來的指令暫存下來交給 main.cpp 處理
                    if (strncmp(msg, "START,", 6) == 0 || strncmp(msg, "STOP", 4) == 0) {
                        strncpy(this->last_command, msg, sizeof(this->last_command));
                        this->has_new_command = true;
                    }
                }
            }
        });

        server->addHandler(&_ws);
        server->on("/pid", HTTP_GET, [](AsyncWebServerRequest *request){ 
            request->send(200, "text/html", index_html); 
        });
    }
    
    inline void cleanup() {
        _ws.cleanupClients();
    }
    
    inline void broadcastData(int time_ms, float target, float left, float right) {
        char msg[128];
        snprintf(msg, sizeof(msg), "{\"time\":%d,\"target\":%.2f,\"left\":%.2f,\"right\":%.2f}", 
                 time_ms, target, left, right);
        _ws.textAll(msg);
    }

    inline void notifyDone() { 
        _ws.textAll("{\"type\":\"DONE\"}");
    }

private:
    AsyncWebSocket _ws;
};

#endif // PID_TEST_PAGE_H