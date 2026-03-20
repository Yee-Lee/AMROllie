
/**
 * ============================================================================
 * 檔案: main.cpp
 * 描述: 主程式整合
 * ============================================================================
 */
#include <Arduino.h>
#include "Motor.h"
#include "MotorSim.h"
#include "MotorWebUI.h"
#include "PIDController.h"
#include "PIDTest.h"

#include "config.h"

enum SystemState { IDLE = 0, RUNNING = 1 };
int currentState = IDLE;

/* Simulated motors */
MotorSim leftMotor = MotorSim(120.0f, 220.0f, 300.0f, 10); // update interval same as PID freq 
MotorSim rightMotor = MotorSim(100.0f, 180.0f, 240.0f, 10);
PIDController leftPID(&leftMotor, 2.0, 3.5, 0.001, 50.0, 100); 
PIDController rightPID(&rightMotor, 2.0, 3.5, 0.001, 50.0, 100); 

/* Real motors*/
// Motor leftMotor = Motor(12, 14, 13, 18, 34, 146, false, 10);
// Motor rightMotor = Motor(27, 26, 25, 19, 35, 146, true, 10);
// // 參數: Kp=2.0, Ki=5.0, Kd=0.1, 積分限幅=50, 頻率=100Hz
// PIDController leftPID(&leftMotor, 2.0, 3.0, 0.005, 50.0, 100); 
// PIDController rightPID(&rightMotor, 2.0, 3.0, 0.005, 50.0, 100); 

MotorWebUI webUI;


// 測試參數: 起始 20 RPM, 終點 100 RPM, 每次增加 20 RPM, 區間 2000ms, 總時長 10000ms
PIDTest pidTest(&leftPID, &rightPID,
    40.0, 100.0, 20.0, 0, 5000);

unsigned long lastWebUpdate = 0;
unsigned long lastSerialPrint = 0;

const char* sStateStr[] = {"IDLE", "RUNNING", "DONE"};

// 用於存放從 Web 接收到的原始字串，在 loop 中處理
String lastWebMsg = "";
bool hasNewCmd = false;

/**
 * 解析從網頁傳來的指令
 * 格式: START,lkp,lki,lkd,rkp,rki,rkd,start,end,step,int,dur
 */
void handleWebCommands(String msg) {
    if (msg.startsWith("START,")) {
        // 跳過 "START," 這 6 個字元
        String data = msg.substring(6);
        
        // 依序拆解參數 (這是一種簡單的解析方式)
        float params[11];
        int lastIdx = 0;
        for (int i = 0; i < 11; i++) {
            int commaIdx = data.indexOf(',', lastIdx);
            if (commaIdx != -1) {
                params[i] = data.substring(lastIdx, commaIdx).toFloat();
                lastIdx = commaIdx + 1;
            } else {
                params[i] = data.substring(lastIdx).toFloat();
            }
        }

        // 1. 更新 PID 增益
        leftPID.setTunings(params[0], params[1], params[2]);
        rightPID.setTunings(params[3], params[4], params[5]);

        // 2. 更新測試排程參數
        // 參數順序: start, end, step, interval(ms), duration(s -> ms)
        pidTest.setParams(params[6], params[7], params[8], (unsigned long)params[9], (unsigned long)params[10] * 1000);

        // 3. 觸發測試開始
        pidTest.begin();
        currentState = RUNNING;
        Serial.println("WebUI: Test Started with new PID gains");
    } 
    else if (msg == "STOP") {
        // 僅重置測試狀態，不主動停止馬達
        pidTest.reset();
        currentState = IDLE;
        Serial.println("WebUI: Test Stopped");
    }
}

void setup() {
    Serial.begin(115200);
    
    // 初始化硬體
    leftMotor.init();
    rightMotor.init();

    leftPID.setOffset(110);
    rightPID.setOffset(90);

    leftPID.setTarget(0);
    rightPID.setTarget(0);

    // 設定 WebUI 的 WebSocket 事件回呼
    webUI.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // 透過一個 Lambda 或是儲存字串的方式讓 loop 處理 Web 指令
    webUI.getWS()->onEvent([](AsyncWebSocket *s, AsyncWebSocketClient *c, AwsEventType t, void *a, uint8_t *d, size_t l) {
        if (t == WS_EVT_DATA) {
            d[l] = 0;
            lastWebMsg = (char*)d;
            hasNewCmd = true;
        }
    });
}

void loop() {

    webUI.cleanup();

    // 檢查是否有來自 Web 的新指令
    if (hasNewCmd) {
        handleWebCommands(lastWebMsg);
        hasNewCmd = false;
    }

    // 2. 系統狀態機邏輯
    if (currentState == IDLE) {
        if (pidTest.state != TEST_IDLE) {
            pidTest.reset();
        }
    } 
    else if (currentState == RUNNING) {
        if (pidTest.state == TEST_IDLE) {
            pidTest.begin();
        }

        // 2. 運行測試邏輯
        PIDTest::Telemetry data = pidTest.run(&leftMotor, &rightMotor);

        if (data.updated) {
            Serial.printf("%d, %f, %f, %f\n", data.time, data.target, data.left, data.right);
            webUI.broadcastData(data.time, data.target, data.left, data.right);
        }

        // 4. 結束判斷
        if (pidTest.state == TEST_DONE) {
            webUI.notifyDone();
            currentState = IDLE;
        }
    }

    leftPID.update();
    rightPID.update();
}