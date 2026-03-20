#ifndef JOYSTICK_WEB_UI_H
#define JOYSTICK_WEB_UI_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

/**
 * @brief 將 HTML UI 嵌入為常數標記字串
 * 包含：搖桿、連線狀態、速度數值顯示與動態能量條
 */
const char JOYSICK_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-Hant">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>Ollie Remote</title>
    <style>
        body { background: #011111; color: #0cccccc; font-family: 'Courier New', monospace; height: 100vh; margin: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 20px; overflow: hidden; touch-action: none; }
        .controls { display: flex; flex-direction: column; align-items: center; gap: 10px; }
        #connectBtn { background: #222; color: #6dbb8a; border: 1px solid #6dbb8a; padding: 10px 24px; font-weight: bold; cursor: pointer; border-radius: 4px; transition: all 0.2s; }
        #connectBtn.active { background: #6dbb8a; color: #011111; box-shadow: 0 0 15px #6dbb8a; }
        #statusText { font-size: 10px; color: #444; text-transform: uppercase; letter-spacing: 1px; }
        #JoystickCanvas { width: 240px; height: 240px; cursor: not-allowed; display: block; touch-action: none; opacity: 0.3; transition: opacity 0.3s; border: 1px solid #1a1a1a; border-radius: 50%; }
        #JoystickCanvas.enabled { cursor: grab; opacity: 1; border-color: #333; }
        .output { display: flex; gap: 40px; }
        .out-item { display: flex; flex-direction: column; align-items: center; min-width: 100px; }
        .out-value { font-size: 26px; font-weight: bold; font-variant-numeric: tabular-nums; }
        #valv { color: #6dbb8a; } #valw { color: #c8a84a; }
        .bar-track { width: 100px; height: 4px; background: #222; position: relative; margin-top: 8px; border-radius: 2px; overflow: hidden; }
        .bar-fill { height: 100%; position: absolute; width: 0%; left: 50%; transition: width 0.1s, left 0.1s; }
        #barv { background: #6dbb8a; } #barw { background: #c8a84a; }
    </style>
</head>
<body>
    <div class="controls">
        <button id="connectBtn">CONNECT OLLIE</button>
        <div id="statusText">System Offline</div>
    </div>
    <canvas id="JoystickCanvas" width="240" height="240"></canvas>
    <div class="output">
        <div class="out-item">
            <span class="out-value" id="valv">+0.000</span>
            <span style="font-size:10px; color:#666">LINEAR (m/s)</span>
            <div class="bar-track"><div class="bar-fill" id="barv"></div></div>
        </div>
        <div class="out-item">
            <span class="out-value" id="valw">+0.000</span>
            <span style="font-size:10px; color:#666">ANGULAR (rad/s)</span>
            <div class="bar-track"><div class="bar-fill" id="barw"></div></div>
        </div>
    </div>
    <script>
        class JoystickWebUI {
            constructor() {
                this.MAX_V = 2.0; this.MAX_W = 3.14; 
                this.JS_R = 100; this.KNOB_R = 18; this.CX = 120; this.CY = 120;
                this.knob = { x: 0, y: 0 }; this.isDragging = false; this.isEnabled = false;
                this.canvas = document.getElementById('JoystickCanvas');
                this.ctx = this.canvas.getContext('2d');
                this.connectBtn = document.getElementById('connectBtn');
                this.statusText = document.getElementById('statusText');
                this.ws = null; this.lastSent = 0;
                this.initEvents(); this.draw();
            }
            initEvents() {
                this.connectBtn.onclick = () => this.toggle();
                this.canvas.onmousedown = (e) => this.start(e);
                window.onmousemove = (e) => this.move(e);
                window.onmouseup = () => this.end();
                this.canvas.ontouchstart = (e) => { e.preventDefault(); this.start(e); };
                window.ontouchmove = (e) => { e.preventDefault(); this.move(e); };
                window.ontouchend = () => this.end();
            }
            toggle() {
                this.isEnabled = !this.isEnabled;
                if (this.isEnabled) {
                    this.connectBtn.classList.add('active'); this.connectBtn.textContent = 'DISCONNECT';
                    this.canvas.classList.add('enabled'); this.statusText.textContent = 'Socket Connected';
                    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
                    this.ws = new WebSocket(`${protocol}//${window.location.host}/ws`);
                    this.ws.onclose = () => { if(this.isEnabled) this.toggle(); };
                } else {
                    this.connectBtn.classList.remove('active'); this.connectBtn.textContent = 'CONNECT OLLIE';
                    this.canvas.classList.remove('enabled'); this.statusText.textContent = 'Offline';
                    if(this.ws) this.ws.close(); this.end();
                }
            }
            start() { if(this.isEnabled) this.isDragging = true; }
            move(e) {
                if(!this.isDragging) return;
                const rect = this.canvas.getBoundingClientRect();
                const clientX = e.touches ? e.touches[0].clientX : e.clientX;
                const clientY = e.touches ? e.touches[0].clientY : e.clientY;
                const x = clientX - rect.left - this.CX;
                const y = clientY - rect.top - this.CY;
                const dist = Math.hypot(x, y);
                const limit = this.JS_R - this.KNOB_R;
                this.knob.x = x * (dist > limit ? limit/dist : 1);
                this.knob.y = y * (dist > limit ? limit/dist : 1);
                this.draw(); this.update();
            }
            end() { this.isDragging = false; this.knob.x = 0; this.knob.y = 0; this.draw(); this.update(); }
            update() {
                const v = (this.knob.y / -this.JS_R) * this.MAX_V;
                const w = (this.knob.x / -this.JS_R) * this.MAX_W;
                
                document.getElementById('valv').textContent = (v>=0?'+':'')+v.toFixed(3);
                document.getElementById('valw').textContent = (w>=0?'+':'')+w.toFixed(3);
                
                const barv = document.getElementById('barv');
                barv.style.width = Math.abs(v/this.MAX_V * 50) + '%';
                barv.style.left = (v >= 0) ? '50%' : (50 - Math.abs(v/this.MAX_V * 50)) + '%';
                
                const barw = document.getElementById('barw');
                barw.style.width = Math.abs(w/this.MAX_W * 50) + '%';
                barw.style.left = (w >= 0) ? '50%' : (50 - Math.abs(w/this.MAX_W * 50)) + '%';

                if(this.ws && this.ws.readyState === 1) {
                    const now = Date.now();
                    if (now - this.lastSent > 50 || (v === 0 && w === 0)) {
                        this.ws.send(JSON.stringify({v, w}));
                        this.lastSent = now;
                    }
                }
            }
            draw() {
                const ctx = this.ctx; ctx.clearRect(0,0,240,240);
                ctx.beginPath(); ctx.arc(120,120,100,0,Math.PI*2);
                ctx.fillStyle='#0a1a1a'; ctx.fill();
                ctx.strokeStyle='#1a2a2a'; ctx.lineWidth=2; ctx.stroke();
                ctx.beginPath(); 
                ctx.arc(120+this.knob.x, 120+this.knob.y, this.KNOB_R, 0, Math.PI*2);
                ctx.fillStyle='#222'; ctx.fill();
                ctx.strokeStyle=this.isEnabled ? '#6dbb8a' : '#444'; 
                ctx.lineWidth=3; ctx.stroke();
            }
        }
        new JoystickWebUI();
    </script>
</body>
</html>
)rawliteral";

class JoystickWebUI {
public:
    volatile float target_v = 0.0f;
    volatile float target_w = 0.0f;
    volatile uint32_t last_update_time = 0;

    JoystickWebUI() : server(80), ws("/ws") {}

    void begin() {
        ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            this->onWebSocketEvent(server, client, type, arg, data, len);
        });
        server.addHandler(&ws);

        // 修正 send_P 為建議的 send 方式
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/html", JOYSICK_HTML);
        });

        server.begin();
        Serial.println("[WebUI] Ollie Server Started on port 80");
    }

    void update() {
        ws.cleanupClients();
        if (millis() - last_update_time > 500) {
            if (target_v != 0.0f || target_w != 0.0f) {
                stopMotors();
            }
        }
    }

    void stopMotors() {
        target_v = 0.0f;
        target_w = 0.0f;
    }

private:
    AsyncWebServer server;
    AsyncWebSocket ws;

    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_DATA: {
                AwsFrameInfo *info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    // 修正：在 ArduinoJson 6.x 中使用 StaticJsonDocument 代替 JsonDocument 基類
                    StaticJsonDocument<128> doc;
                    DeserializationError error = deserializeJson(doc, (char*)data, len);
                    if (!error) {
                        target_v = doc["v"] | 0.0f;
                        target_w = doc["w"] | 0.0f;
                        last_update_time = millis();
                    }
                }
                break;
            }
            case WS_EVT_DISCONNECT:
                stopMotors();
                break;
            default:
                break;
        }
    }
};

#endif