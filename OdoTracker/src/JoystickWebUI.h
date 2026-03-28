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
            gap: 6px;
            background: var(--panel-bg);
            padding: 10px;
            border-radius: 8px;
            border: 1px solid var(--line);
            min-width: 220px;
        }

        .data-card {
            display: flex;
            flex-direction: column;
            align-items: center;
            background: rgba(0,0,0,0.4);
            padding: 4px;
            border-radius: 4px;
        }

        .label { font-size: 9px; color: var(--accent); margin-bottom: 2px; text-transform: uppercase;}
        .value { font-size: 16px; font-family: monospace; color: var(--text); }

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

        .btn-group {
            grid-column: 1 / -1; /* 跨越兩欄 */
            display: flex;
            gap: 6px;
            margin-top: 2px;
        }
        .btn-group button {
            flex: 1;
            padding: 8px;
            font-size: 11px;
            font-weight: bold;
            border: 1px solid var(--line);
            border-radius: 4px;
            background: transparent;
            color: var(--text);
            cursor: pointer;
        }
        .btn-active { background: #ff8800 !important; color: var(--bg) !important; border-color: #ff8800 !important; }

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
            <div class="data-card"><div class="label">X (m)</div><div id="val-x" class="value">0.00</div></div>
            <div class="data-card"><div class="label">Y (m)</div><div id="val-y" class="value">0.00</div></div>
            <div class="data-card" style="grid-column: 1 / -1; flex-direction: row; justify-content: space-around;"><div class="label">Theta(deg)</div><div id="val-th" class="value" style="font-size:18px;">0.0</div></div>
            <div class="btn-group">
                <button id="test-btn" onclick="toggleMotionTest()">Test: OFF</button>
                <button id="rst-btn" onclick="resetOdom()">Reset Pose</button>
            </div>
        </div>

        <div id="joy-right" class="joy-container">
            <div id="knob-right" class="joy-knob"></div>
        </div>
    </div>

    <script>
        var ws = null;
        let isConnected = false;
        let isMotionTest = false;
        let currentV = 0, currentW = 0;
        const maxDist = 55; 

        // 建立具備自動重連機制的 WebSocket
        function connectWS() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            ws.onopen = () => {
                console.log("WebSocket connected.");
                ws.send(`TEST,${isMotionTest ? 1 : 0}`); // 連線時，主動同步一次測試模式狀態
            };
            ws.onmessage = (event) => {
                if (event.data.startsWith("ODOM,")) {
                    let parts = event.data.split(",");
                    if (parts.length === 4) {
                        document.getElementById('val-x').innerText = parseFloat(parts[1]).toFixed(2);
                        document.getElementById('val-y').innerText = parseFloat(parts[2]).toFixed(2);
                        document.getElementById('val-th').innerText = parseFloat(parts[3]).toFixed(1);
                    }
                }
            };
            ws.onclose = () => {
                console.log("WebSocket 斷線，2秒後嘗試重連...");
                setTimeout(connectWS, 2000);
            };
        }
        connectWS();

        // 定頻發送機制：每 50 毫秒 (20Hz) 固定發送狀態給 ESP32。
        // (將發送邏輯從觸控事件中抽離，避免 120Hz 的手機螢幕狂發封包塞爆 ESP32 緩衝區)
        setInterval(() => {
            if (ws && ws.readyState === WebSocket.OPEN && isConnected) {
                ws.send(`JOY,${currentV.toFixed(2)},${currentW.toFixed(2)}`);
            }
        }, 50);

        function toggleConnection() {
            isConnected = !isConnected;
            const btn = document.getElementById('conn-btn');
            btn.innerText = isConnected ? "ON" : "OFF";
            btn.classList.toggle('active', isConnected);
            if (!isConnected) { 
                currentV = 0; currentW = 0; updateUI(); 
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(`JOY,0.00,0.00`); // 斷線時立即強制煞停
                }
            }
        }

        function toggleMotionTest() {
            if (isConnected) {
                alert("Please turn OFF connection to change test mode.");
                return;
            }
            isMotionTest = !isMotionTest;
            const btn = document.getElementById('test-btn');
            btn.innerText = isMotionTest ? "Test: ON (40)" : "Test: OFF (80)";
            btn.classList.toggle('btn-active', isMotionTest);
            
            if (ws && ws.readyState === WebSocket.OPEN) ws.send(`TEST,${isMotionTest ? 1 : 0}`);
        }

        function resetOdom() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send("RST_ODOM");
            }
        }

        function updateUI() {
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
    // 加入 volatile，防止多任務架構下的變數快取問題 (看門狗誤判)
    volatile float target_v = 0.0f;
    volatile float target_w = 0.0f;
    volatile bool motion_test_active = false;
    volatile bool request_odom_reset = false;
    volatile unsigned long last_msg_time = 0; 

    inline JoystickWebUI() : _server(80), _ws("/ws") {}
    
    inline void begin(const char* ssid, const char* password) {
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) { delay(500); }
        Serial.println(WiFi.localIP());

        esp_wifi_set_ps(WIFI_PS_NONE); 
        WiFi.setSleep(false); 
        
        _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
            if (type == WS_EVT_DATA) {
                // 放寬封包檢查：直接將收到的二進位資料安全轉為字串
                char msg[64];
                size_t copy_len = (len < 63) ? len : 63;
                memcpy(msg, data, copy_len);
                msg[copy_len] = '\0';

                // 指令解析路由
                if (strncmp(msg, "JOY,", 4) == 0) {
                    // 改用 strchr 與 atof 取代 sscanf，速度更快且能避免格式化解析失敗
                    char* comma = strchr(msg + 4, ',');
                    if (comma != nullptr) {
                        *comma = '\0'; // 切割字串
                        this->target_v = atof(msg + 4);
                        this->target_w = atof(comma + 1);
                        this->last_msg_time = millis(); // 更新心跳時間戳記，重置看門狗
                    }
                } else if (strncmp(msg, "TEST,", 5) == 0) {
                    this->motion_test_active = (atoi(msg + 5) == 1);
                } else if (strncmp(msg, "RST_ODOM", 8) == 0) {
                    this->request_odom_reset = true;
                }
            }
        });

        _server.addHandler(&_ws);
        _server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){ 
            request->send(200, "text/html", index_html); 
        });
        _server.begin();
    }
    
    inline void cleanup() { 
        _ws.cleanupClients(); 
        
        // 軟體看門狗 (Watchdog)：若超過 1 秒沒收到前端的訊號，判定為失聯並自動煞停
        if (millis() - last_msg_time > 1000) {
            target_v = 0.0f;
            target_w = 0.0f;
        }
    }

    // 將里程計推播給前端網頁
    inline void broadcastOdom(float x, float y, float theta) {
        char msg[64];
        // 內部 theta 是弧度，推播給前端轉為更容易閱讀的「度數 (Degrees)」
        snprintf(msg, sizeof(msg), "ODOM,%.2f,%.2f,%.1f", x, y, theta * 180.0f / PI);
        _ws.textAll(msg);
    }
    
private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
};

#endif