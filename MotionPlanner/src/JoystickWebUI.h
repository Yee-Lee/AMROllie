#ifndef JOYSTICK_WEB_UI_H
#define JOYSTICK_WEB_UI_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>

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
            --accent: #00ff88;
            --line: #333;
            --text: #eee;
            --panel-bg: rgba(20, 20, 20, 0.8);
        }

        body { 
            background-color: var(--bg); 
            color: var(--text); 
            font-family: 'Segoe UI', system-ui, sans-serif; 
            margin: 0; 
            height: 100vh;
            overflow: hidden;
            user-select: none;
            -webkit-user-select: none;
            display: flex;
            flex-direction: column;
        }

        #header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid var(--line);
            padding: 8px 20px;
            background: rgba(0,0,0,0.5);
            height: 45px;
            box-sizing: border-box;
        }

        #conn-btn {
            padding: 5px 20px;
            font-size: 12px;
            font-weight: bold;
            border: 1px solid var(--line);
            border-radius: 4px;
            background: transparent;
            color: var(--text);
            cursor: pointer;
        }

        #conn-btn.active {
            color: var(--accent);
            border-color: var(--accent);
            box-shadow: 0 0 10px rgba(0, 255, 136, 0.2);
        }

        #main-area {
            flex: 1;
            display: flex;
            flex-direction: row;
            justify-content: space-around;
            align-items: center;
            padding: 10px;
            box-sizing: border-box;
        }

        .joy-container {
            width: 180px;
            height: 180px;
            border-radius: 50%;
            border: 2px solid var(--line);
            position: relative;
            touch-action: none;
            background: rgba(255, 255, 255, 0.02);
            transition: border-color 0.2s ease, box-shadow 0.2s ease;
        }

        /* 只有操作中的圈圈會 Highlight */
        .joy-container.active {
            border-color: var(--accent);
            box-shadow: 0 0 30px rgba(0, 255, 136, 0.3);
        }

        .joy-knob {
            width: 70px;
            height: 70px;
            background: var(--accent);
            border-radius: 50%;
            position: absolute;
            /* 居中計算: (180-70)/2 = 55 */
            top: 55px;
            left: 55px;
            box-shadow: 0 0 15px rgba(0, 255, 136, 0.3);
            transition: transform 0.05s linear;
        }

        #center-dashboard {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 12px;
            background: var(--panel-bg);
            padding: 15px;
            border-radius: 8px;
            border: 1px solid var(--line);
            min-width: 220px;
        }

        .data-card {
            display: flex;
            flex-direction: column;
            align-items: center;
            background: rgba(0,0,0,0.4);
            padding: 8px;
            border-radius: 4px;
        }

        .label { font-size: 9px; color: var(--accent); margin-bottom: 4px; text-transform: uppercase;}
        .value { font-size: 24px; font-family: monospace; color: var(--text); }

        #portrait-warning {
            display: none;
            position: absolute;
            top: 0; left: 0; width: 100%; height: 100%;
            background: var(--bg);
            z-index: 9999;
            color: var(--accent);
            text-align: center;
            padding-top: 20%;
        }

        @media screen and (orientation: portrait) {
            #main-area, #header { display: none; }
            #portrait-warning { display: block; }
        }
    </style>
</head>
<body>
    <div id="portrait-warning">請將手機橫放操控 Ollie</div>

    <div id="header">
        <div style="font-weight:bold; letter-spacing:1px;">OLLIE DUAL-STICK</div>
        <button id="conn-btn" onclick="toggleConnection()">OFF</button>
    </div>

    <div id="main-area">
        <div id="joy-left" class="joy-container">
            <div id="knob-left" class="joy-knob"></div>
        </div>
        
        <div id="center-dashboard">
            <div class="data-card"><div class="label">Linear V</div><div id="val-v" class="value">0.00</div></div>
            <div class="data-card"><div class="label">Angular W</div><div id="val-w" class="value">0.00</div></div>
            <div class="data-card"><div class="label">L-Pwr</div><div id="pwr-l" class="value">0%</div></div>
            <div class="data-card"><div class="label">R-Pwr</div><div id="pwr-r" class="value">0%</div></div>
        </div>

        <div id="joy-right" class="joy-container">
            <div id="knob-right" class="joy-knob"></div>
        </div>
    </div>

    <script>
        var ws = new WebSocket('ws://' + window.location.hostname + '/ws');
        let isConnected = false;
        let currentV = 0, currentW = 0;
        const maxDist = 55; 

        function toggleConnection() {
            isConnected = !isConnected;
            const btn = document.getElementById('conn-btn');
            btn.innerText = isConnected ? "ON" : "OFF";
            btn.classList.toggle('active', isConnected);
            if (!isConnected) { currentV = 0; currentW = 0; updateUI(); }
        }

        function updateUI() {
            if (ws.readyState === WebSocket.OPEN && isConnected) {
                ws.send(`JOY,${currentV.toFixed(2)},${currentW.toFixed(2)}`);
            }
            document.getElementById('val-v').innerText = currentV.toFixed(2);
            document.getElementById('val-w').innerText = currentW.toFixed(2);
            let l = currentV - currentW, r = currentV + currentW;
            let m = Math.max(1, Math.abs(l), Math.abs(r));
            document.getElementById('pwr-l').innerText = Math.round((l/m)*100) + "%";
            document.getElementById('pwr-r').innerText = Math.round((r/m)*100) + "%";
        }

        function initJoystick(zoneId, knobId, isLeftStick) {
            const zone = document.getElementById(zoneId);
            const knob = document.getElementById(knobId);
            let pointerId = null;

            zone.addEventListener('pointerdown', (e) => {
                if (!isConnected) return;
                pointerId = e.pointerId;
                zone.setPointerCapture(pointerId);
                zone.classList.add('active'); // 按下時 Highlight 大圈
                handleMove(e);
            });

            const handleMove = (e) => {
                if (pointerId === null || e.pointerId !== pointerId) return;
                const rect = zone.getBoundingClientRect();
                const cx = rect.left + rect.width / 2;
                const cy = rect.top + rect.height / 2;
                
                let dx = e.clientX - cx;
                let dy = e.clientY - cy;

                // 360度自由移動限制
                const dist = Math.sqrt(dx*dx + dy*dy);
                if (dist > maxDist) {
                    dx *= maxDist / dist;
                    dy *= maxDist / dist;
                }

                knob.style.transform = `translate(${dx}px, ${dy}px)`;

                // 數值提取邏輯
                if (isLeftStick) {
                    currentV = -(dy / maxDist); // 左手只取 Y 軸
                } else {
                    currentW = -(dx / maxDist); // 右手只取 X 軸
                }
                updateUI();
            };

            zone.addEventListener('pointermove', handleMove);

            const reset = (e) => {
                if (e.pointerId !== pointerId) return;
                pointerId = null;
                zone.classList.remove('active'); // 放開時取消 Highlight
                knob.style.transform = `translate(0px, 0px)`;
                if (isLeftStick) currentV = 0; else currentW = 0;
                updateUI();
            };

            zone.addEventListener('pointerup', reset);
            zone.addEventListener('pointercancel', reset);
        }

        initJoystick('joy-left', 'knob-left', true);
        initJoystick('joy-right', 'knob-right', false);
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
        Serial.println(WiFi.localIP());

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