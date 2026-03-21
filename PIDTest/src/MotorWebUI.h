#ifndef MOTOR_WEB_UI_H
#define MOTOR_WEB_UI_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h> // 加入此標頭檔以控制 WiFi 省電模式

// ============================================================================
// HTML / CSS / JS 原始碼 (儲存於 Flash)
// ============================================================================
static const char index_html[] PROGMEM = R"raw(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <title>AMR Ollie PID Tuner</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        /* Dark Mode & Simple Style */
        body { 
            background-color: #121212; 
            color: #e0e0e0; 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            margin: 0; 
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
        }
        
        /* UI Container 1600x800 */
        #ui-container {
            width: 1600px;
            height: 800px;
            background: #1e1e1e;
            border-radius: 12px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            display: flex;
            flex-direction: row; 
            overflow: hidden;
            box-sizing: border-box;
            padding: 20px;
            gap: 20px;
        }

        /* Left Side: Chart + Controls */
        #main-panel {
            flex: 3;
            display: flex;
            flex-direction: column;
            gap: 20px;
            min-width: 0; 
        }
        
        #chart-wrapper {
            flex: 1;
            background: #252525;
            border-radius: 8px;
            padding: 10px;
            position: relative;
            overflow-x: auto;
        }

        #chart-box {
            width: 100%;
            height: 100%;
            position: relative;
        }

        /* Parameter Zones */
        #controls {
            height: 160px;
            display: flex;
            gap: 15px;
            background: #252525;
            border-radius: 8px;
            padding: 15px;
            box-sizing: border-box;
        }

        .param-section {
            flex: 1;
            display: flex;
            flex-wrap: wrap;
            align-content: flex-start;
            gap: 10px;
            background: #1a1a1a;
            padding: 10px;
            border-radius: 6px;
            border-top: 3px solid #555;
        }
        .param-section.left-pid { border-color: #e74c3c; flex: 1; } 
        .param-section.right-pid { border-color: #2ecc71; flex: 1; } 
        /* 增加測試參數區塊的權重，讓 5 個欄位有更多空間 */
        .param-section.test-params { border-color: #3498db; flex: 2; } 

        .section-title { width: 100%; font-size: 13px; font-weight: bold; color: #aaa; margin-bottom: 5px; text-transform: uppercase; }
        
        .input-group { display: flex; flex-direction: column; width: calc(33.3% - 7px); }
        /* 針對 5 個欄位的特殊寬度計算 (約 20%) */
        .input-group.five-cols { width: calc(20% - 8px); }
        
        label { font-size: 11px; color: #aaa; margin-bottom: 3px; white-space: nowrap; overflow: hidden; }
        input { 
            background: #333; 
            color: #fff; 
            border: 1px solid #444; 
            border-radius: 4px; 
            padding: 6px; 
            font-size: 13px; 
            width: 100%;
            box-sizing: border-box;
        }
        input:focus { outline: none; border-color: #3498db; }
        input:disabled { opacity: 0.4; cursor: not-allowed; } 

        .btn-container {
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 0 10px;
        }

        /* Single Toggle Button */
        button { 
            padding: 15px 40px; 
            font-size: 20px; 
            font-weight: bold; 
            border: none; 
            border-radius: 8px; 
            cursor: pointer; 
            transition: all 0.2s; 
            color: white;
            background: #2ecc71; 
            box-shadow: 0 4px 15px rgba(46, 204, 113, 0.3);
            white-space: nowrap;
        }
        button:hover { filter: brightness(1.1); }
        button:active { transform: scale(0.95); }
        button.running { 
            background: #e74c3c; 
            box-shadow: 0 4px 15px rgba(231, 76, 60, 0.3);
        }

        /* Right Side: Log Panel */
        #log-panel {
            flex: 1;
            background: #252525;
            border-radius: 8px;
            display: flex;
            flex-direction: column;
            padding: 15px;
            min-width: 250px;
        }

        #log-panel h3 {
            margin: 0 0 10px 0;
            font-size: 14px;
            color: #aaa;
            text-transform: uppercase;
            border-bottom: 1px solid #444;
            padding-bottom: 8px;
        }

        #log-content {
            flex: 1;
            overflow-y: auto;
            font-family: 'Courier New', Courier, monospace;
            font-size: 17px; 
            line-height: 1.5;
            padding-right: 5px;
        }

        /* Scrollbar styling for log panel */
        #log-content::-webkit-scrollbar { width: 8px; }
        #log-content::-webkit-scrollbar-track { background: #1a1a1a; border-radius: 4px; }
        #log-content::-webkit-scrollbar-thumb { background: #555; border-radius: 4px; }
        
        .log-row { display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding: 4px 0; }
        .log-time { color: #888; width: 75px;} 
        .log-val { text-align: right; width: 60px;}
        .val-tgt { color: #3498db; }
        .val-l { color: #e74c3c; }
        .val-r { color: #2ecc71; }
    </style>
</head>
<body>
    <div id="ui-container">
        <div id="main-panel">
            <div id="chart-wrapper">
                <div id="chart-box">
                    <canvas id="pidChart"></canvas>
                </div>
            </div>
            <div id="controls">
                <!-- Left PID -->
                <div class="param-section left-pid">
                    <div class="section-title">Left Motor PID</div>
                    <div class="input-group"><label>Kp</label><input type="number" step="0.1" id="lkp" value="2.0"></div>
                    <div class="input-group"><label>Ki</label><input type="number" step="0.1" id="lki" value="3.5"></div>
                    <div class="input-group"><label>Kd</label><input type="number" step="0.001" id="lkd" value="0.005"></div>
                </div>
                
                <!-- Right PID -->
                <div class="param-section right-pid">
                    <div class="section-title">Right Motor PID</div>
                    <div class="input-group"><label>Kp</label><input type="number" step="0.1" id="rkp" value="2.0"></div>
                    <div class="input-group"><label>Ki</label><input type="number" step="0.1" id="rki" value="3.5"></div>
                    <div class="input-group"><label>Kd</label><input type="number" step="0.001" id="rkd" value="0.005"></div>
                </div>

                <!-- Test Params - 調整為 5 個欄位併排 -->
                <div class="param-section test-params">
                    <div class="section-title">Test Profile</div>
                    <div class="input-group five-cols"><label>Start RPM</label><input type="number" id="start" value="60"></div>
                    <div class="input-group five-cols"><label>End RPM</label><input type="number" id="end" value="100"></div>
                    <div class="input-group five-cols"><label>Step RPM</label><input type="number" id="step" value="00"></div>
                    <div class="input-group five-cols"><label>Int(ms)</label><input type="number" id="int" value="1000"></div>
                    <div class="input-group five-cols"><label>Dur(s)</label><input type="number" id="dur" value="10"></div>
                </div>

                <div class="btn-container">
                    <button id="btn-toggle" onclick="toggleTest()">START</button>
                </div>
            </div>
        </div>

        <div id="log-panel">
            <h3>Data Log <span style="font-size:10px; color:#666; float:right;">(Tgt / L / R)</span></h3>
            <div id="log-content"></div>
        </div>
    </div>

    <script>
        // (1) 防止進入省電模式 (Wake Lock)
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

        // Chart.js 配置
        Chart.defaults.color = '#888';
        var ctx = document.getElementById('pidChart').getContext('2d');
        var pidChart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [
                    { label: 'Target RPM', borderColor: '#3498db', borderDash: [5, 5], data: [], pointRadius: 0, borderWidth: 2, tension: 0.1 },
                    { label: 'Left RPM', borderColor: '#e74c3c', data: [], pointRadius: 0, borderWidth: 2, tension: 0.1 },
                    { label: 'Right RPM', borderColor: '#2ecc71', data: [], pointRadius: 0, borderWidth: 2, tension: 0.1 }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false, 
                animation: false,
                plugins: {
                    legend: { labels: { color: '#e0e0e0' } }
                },
                scales: {
                    x: {
                        type: 'linear',
                        min: 0,
                        max: 10, 
                        title: { display: true, text: 'Time (s)', color: '#aaa' },
                        grid: { color: '#333' }
                    },
                    y: { 
                        title: { display: true, text: 'RPM', color: '#aaa' },
                        grid: { color: '#333' }
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
            
            // 處理測試完成的通知
            if (d.type === "DONE") {
                stopUIState();
                return;
            }

            var t_sec = d.time / 1000.0; 
            var t_ms = d.time; 
            
            // 更新圖表
            pidChart.data.datasets[0].data.push({x: t_sec, y: d.target});
            pidChart.data.datasets[1].data.push({x: t_sec, y: d.left});
            pidChart.data.datasets[2].data.push({x: t_sec, y: d.right});

            var wrapper = document.getElementById('chart-wrapper');
            var box = document.getElementById('chart-box');
            var isAtEnd = (wrapper.scrollLeft + wrapper.clientWidth >= wrapper.scrollWidth - 50);

            if (t_sec > 10) {
                // 超過 10 秒後，動態增加寬度以維持解析度 (每 10 秒 100% 寬度)
                box.style.width = (t_sec * 10.0) + "%";
                pidChart.options.scales.x.max = t_sec;
            }

            pidChart.update('none');

            if (isAtEnd) {
                wrapper.scrollLeft = wrapper.scrollWidth;
            }

            // 更新 Log Panel
            var logRow = document.createElement('div');
            logRow.className = 'log-row';
            logRow.innerHTML = `
                <span class="log-time">${t_ms}ms</span>
                <span class="log-val val-tgt">${d.target.toFixed(1)}</span>
                <span class="log-val val-l">${d.left.toFixed(1)}</span>
                <span class="log-val val-r">${d.right.toFixed(1)}</span>
            `;
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

class MotorWebUI {
public:
    inline MotorWebUI() : _server(80), _ws("/ws") {}
    
    inline void begin(const char* ssid, const char* password) {
        WiFi.begin(ssid, password);
        Serial.print("Connecting to WiFi");
        while (WiFi.status() != WL_CONNECTED) { 
            delay(500); 
            Serial.print("."); 
        }
        
        // 核心設定：禁用 WiFi 省電模式以確保 Full Power / 低延遲
        esp_wifi_set_ps(WIFI_PS_NONE); 
        WiFi.setSleep(false); 
        
        Serial.println("\nConnected!");
        Serial.print("WebUI IP Address: ");
        Serial.println(WiFi.localIP());

        _server.addHandler(&_ws);
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
            request->send(200, "text/html", index_html); 
        });
        _server.begin();
    }
    
    inline void cleanup() { _ws.cleanupClients(); }
    inline AsyncWebSocket* getWS() { return &_ws; }
    
    // 廣播感測器數據到網頁
    inline void broadcastData(int time_ms, float target, float left, float right) {
        char msg[128];
        snprintf(msg, sizeof(msg), "{\"time\":%d,\"target\":%.2f,\"left\":%.2f,\"right\":%.2f}", 
                 time_ms, target, left, right);
        _ws.textAll(msg);
    }

    // 通知網頁測試已完成
    inline void notifyDone() { 
        _ws.textAll("{\"type\":\"DONE\"}");
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
};

#endif // MOTOR_WEB_UI_H