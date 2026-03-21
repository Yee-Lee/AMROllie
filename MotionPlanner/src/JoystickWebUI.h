#ifndef JOYSTICK_WEB_UI_H
#define JOYSTICK_WEB_UI_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>

// ============================================================================
// HTML / CSS / JS 原始碼 (儲存於 Flash)
// ============================================================================
static const char index_html[] PROGMEM = R"raw(
<!DOCTYPE HTML>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Ollie Remote</title>
    <style>
        :root {
            --bg: #0a0a0a;
            --bg-active: #121815;
            --accent: #00ff88;
            --line: #222;
            --text: #eee;
        }

        body { 
            background-color: var(--bg); 
            color: var(--text); 
            font-family: 'Segoe UI', system-ui, sans-serif; 
            margin: 0; 
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            overflow: hidden;
            user-select: none;
            transition: background-color 0.3s ease;
        }
        
        /* 當搖桿被按下時改變全域背景顏色 */
        body.joystick-active {
            background-color: var(--bg-active);
        }

        #ui-container {
            width: 100%;
            max-width: 600px;
            height: 95vh;
            display: flex;
            flex-direction: column;
            padding: 20px;
            gap: 15px;
            box-sizing: border-box;
        }

        /* 頂部控制欄 */
        #header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border: 1px solid var(--line);
            padding: 10px 15px;
            border-radius: 4px;
            background: rgba(0,0,0,0.5);
        }

        .controls-group {
            display: flex;
            align-items: center;
            gap: 15px;
        }

        #conn-btn {
            padding: 5px 12px;
            font-size: 11px;
            font-weight: bold;
            border: 1px solid var(--line);
            border-radius: 2px;
            background: transparent;
            color: var(--text);
            cursor: pointer;
        }

        #conn-btn.active {
            color: var(--accent);
            border-color: var(--accent);
        }

        .constrain-toggle {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 11px;
            color: var(--text);
            cursor: pointer;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        input[type="checkbox"] {
            accent-color: var(--accent);
        }

        /* 搖桿區域 */
        #joystick-zone {
            flex: 1;
            border: 1px solid var(--line);
            border-radius: 4px;
            display: flex;
            align-items: center;
            justify-content: center;
            position: relative;
            transition: border-color 0.3s ease;
        }

        /* 搖桿按下時強化邊框 */
        body.joystick-active #joystick-zone {
            border-color: var(--accent);
        }

        #joystick-container {
            width: 240px;
            height: 240px;
            border-radius: 50%;
            position: relative;
            touch-action: none;
            border: 1px solid var(--line);
        }

        #joystick-knob {
            width: 80px; /* 增大球體 */
            height: 80px;
            background: var(--accent);
            border-radius: 50%;
            position: absolute;
            top: 80px; /* 重新計算中心點：(240-80)/2 */
            left: 80px;
            box-shadow: 0 0 20px rgba(0, 255, 136, 0.2);
            transition: transform 0.05s linear, scale 0.2s ease;
        }

        /* 按下時的視覺縮放 */
        body.joystick-active #joystick-knob {
            scale: 1.1;
            box-shadow: 0 0 30px rgba(0, 255, 136, 0.4);
        }

        /* 數據面板 */
        .data-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }

        .data-card {
            border: 1px solid var(--line);
            padding: 15px;
            border-radius: 4px;
            text-align: left;
            background: rgba(0,0,0,0.3);
        }

        .label { font-size: 9px; color: var(--accent); margin-bottom: 8px; text-transform: uppercase; letter-spacing: 1px;}
        .value { font-size: 32px; font-weight: 200; font-family: 'Courier New', monospace; color: var(--text); }
        
    </style>
</head>
<body>
    <div id="ui-container">
        <div id="header">
            <div style="font-weight:bold; color:var(--text); font-size:12px; letter-spacing:2px;">OLLIE MOTION CONTROLLER</div>
            <div class="controls-group">
                <label class="constrain-toggle">
                    <input type="checkbox" id="w-constrain" onchange="forceSync()"> W-LOCK
                </label>
                <button id="conn-btn" onclick="toggleConnection()">OFF</button>
            </div>
        </div>

        <div id="joystick-zone">
            <div id="joystick-container">
                <div id="joystick-knob"></div>
            </div>
        </div>
        
        <div class="data-grid">
            <div class="data-card">
                <div class="label">Linear (V)</div>
                <div id="val-v" class="value">0.00</div>
            </div>
            <div class="data-card">
                <div class="label">Angular (W)</div>
                <div id="val-w" class="value">0.00</div>
            </div>
        </div>

        <div class="data-grid">
            <div class="data-card">
                <div class="label">Left Power</div>
                <div id="pwr-l" class="value">0%</div>
            </div>
            <div class="data-card">
                <div class="label">Right Power</div>
                <div id="pwr-r" class="value">0%</div>
            </div>
        </div>
    </div>

    <script>
        var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
        const knob = document.getElementById('joystick-knob');
        const container = document.getElementById('joystick-container');
        const connBtn = document.getElementById('conn-btn');
        const wLock = document.getElementById('w-constrain');
        const body = document.body;
        
        let isConnected = false;
        let isMoving = false;
        let centerX = 120, centerY = 120;
        let curX = 120, curY = 120;

        async function toggleConnection() {
            isConnected = !isConnected;
            if (isConnected) {
                connBtn.innerText = "ON";
                connBtn.classList.add('active');
            } else {
                connBtn.innerText = "OFF";
                connBtn.classList.remove('active');
                resetControls();
            }
        }

        function forceSync() {
            if (isConnected) updateJoystick(curX, curY);
        }

        function updateJoystick(x, y) {
            curX = x; curY = y;
            if (!isConnected) return;

            let dx = x - centerX;
            let dy = y - centerY;
            
            // W Lock 核心邏輯
            if (wLock.checked) dx = 0;

            const maxDist = 80;
            let dist = Math.sqrt(dx*dx + dy*dy);
            if (dist > maxDist) {
                dx *= maxDist / dist;
                dy *= maxDist / dist;
            }

            knob.style.transform = `translate(${dx}px, ${dy}px)`;

            let v = -(dy / maxDist);
            let w = wLock.checked ? 0 : -(dx / maxDist); 
            
            v = parseFloat(v.toFixed(2));
            w = parseFloat(w.toFixed(2));

            if (ws.readyState === WebSocket.OPEN) {
                ws.send(`JOY,${v.toFixed(2)},${w.toFixed(2)}`);
            }
            updateDisplay(v, w);
        }

        function updateDisplay(v, w) {
            document.getElementById('val-v').innerText = v.toFixed(2);
            document.getElementById('val-w').innerText = w.toFixed(2);

            let l = v - w;
            let r = v + w;
            let max = Math.max(1, Math.abs(l), Math.abs(r));
            document.getElementById('pwr-l').innerText = Math.round((l/max)*100) + "%";
            document.getElementById('pwr-r').innerText = Math.round((r/max)*100) + "%";
        }

        function resetControls() {
            knob.style.transform = `translate(0px, 0px)`;
            curX = centerX; curY = centerY;
            if (ws.readyState === WebSocket.OPEN) ws.send(`JOY,0.00,0.00`);
            updateDisplay(0, 0);
        }

        const handleStart = (e) => { 
            isMoving = true; 
            body.classList.add('joystick-active'); // 按下時加入類別
            const rect = container.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;
            updateJoystick(clientX - rect.left, clientY - rect.top);
        };
        
        const handleEnd = () => { 
            isMoving = false; 
            body.classList.remove('joystick-active'); // 放開時移除類別
            if(isConnected) resetControls(); 
        };

        const handleMove = (e) => {
            if (!isMoving || !isConnected) return;
            const rect = container.getBoundingClientRect();
            const clientX = e.touches ? e.touches[0].clientX : e.clientX;
            const clientY = e.touches ? e.touches[0].clientY : e.clientY;
            updateJoystick(clientX - rect.left, clientY - rect.top);
        };

        container.addEventListener('mousedown', handleStart);
        window.addEventListener('mouseup', handleEnd);
        window.addEventListener('mousemove', handleMove);
        container.addEventListener('touchstart', (e) => { handleStart(e); e.preventDefault(); }, {passive:false});
        window.addEventListener('touchend', handleEnd);
        window.addEventListener('touchmove', handleMove, {passive:false});
    </script>
</body>
</html>
)raw";

class JoystickWebUI {
public:
    float target_v = 0.0f;
    float target_w = 0.0f;

    inline JoystickWebUI() : _server(80), _ws("/ws") {}
    
    inline void begin(const char* ssid, const char* password) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) { delay(500); }
        
        esp_wifi_set_ps(WIFI_PS_NONE); 
        WiFi.setSleep(false); 
        
        _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            if (type == WS_EVT_DATA) {
                data[len] = 0;
                char* msg = (char*)data;
                if (strncmp(msg, "JOY,", 4) == 0) {
                    float v, w;
                    if (sscanf(msg + 4, "%f,%f", &v, &w) == 2) {
                        this->target_v = v;
                        this->target_w = w;
                    }
                }
            } else if (type == WS_EVT_DISCONNECT) {
                this->target_v = 0.0f;
                this->target_w = 0.0f;
            }
        });

        _server.addHandler(&_ws);
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
            request->send(200, "text/html", index_html); 
        });
        _server.begin();
    }
    
    inline void cleanup() { _ws.cleanupClients(); }
    
private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
};

#endif