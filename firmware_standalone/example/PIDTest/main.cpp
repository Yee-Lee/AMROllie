#include <Arduino.h>
#include <esp_wifi.h>

// 引入核心模組 (從主專案 src 目錄取得)
#include "../../src/config.h"
#include "../../src/Motor.h"
#include "../../src/PIDController.h"
#include "../../src/WebServerManager.h"

// 引入分離後的本地 WebUI 模組
#include "web/indexPage.h"
#include "web/PIDTestPage.h"

WebServerManager webServer;
IndexPage indexPage;
PIDTestPage pidPage;

// 實例化真實馬達與 PID 控制器 (參數與主系統保持一致)
Motor leftMotor(MOTOR_L_IN1, MOTOR_L_IN2, MOTOR_L_PWM, MOTOR_L_ENC_A, MOTOR_L_ENC_B, MOTOR_CPR, false, 5);
Motor rightMotor(MOTOR_R_IN1, MOTOR_R_IN2, MOTOR_R_PWM, MOTOR_R_ENC_A, MOTOR_R_ENC_B, MOTOR_CPR, true, 5);

PIDController leftPID(&leftMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ);
PIDController rightPID(&rightMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ);

// 測試狀態與時序控制變數
bool isTesting = false;
unsigned long testStartTime = 0;
unsigned long lastStepTime = 0;
unsigned long testDurationMs = 0;
unsigned long stepIntervalMs = 0;
float currentTargetRPM = 0;
float startRPM = 0;
float endRPM = 0;
float stepRPM = 0;

void startTest(const char* cmd) {
    // 解析網頁傳來的測試參數，格式: START,lkp,lki,lkd,rkp,rki,rkd,start,end,step,int,dur
    char* p = (char*)(cmd + 6);
    float lkp = atof(p); p = strchr(p, ','); if(p) p++;
    float lki = atof(p); p = strchr(p, ','); if(p) p++;
    float lkd = atof(p); p = strchr(p, ','); if(p) p++;
    float rkp = atof(p); p = strchr(p, ','); if(p) p++;
    float rki = atof(p); p = strchr(p, ','); if(p) p++;
    float rkd = atof(p); p = strchr(p, ','); if(p) p++;
    
    startRPM = atof(p); p = strchr(p, ','); if(p) p++;
    endRPM = atof(p); p = strchr(p, ','); if(p) p++;
    stepRPM = atof(p); p = strchr(p, ','); if(p) p++;
    stepIntervalMs = atoi(p); p = strchr(p, ','); if(p) p++;
    testDurationMs = atoi(p) * 1000;

    // 套用 PID 參數
    leftPID.setTunings(lkp, lki, lkd);
    rightPID.setTunings(rkp, rki, rkd);
    
    // 初始重置與設定
    currentTargetRPM = startRPM;
    leftPID.setTarget(currentTargetRPM);
    rightPID.setTarget(currentTargetRPM);
    leftPID.reset();
    rightPID.reset();

    isTesting = true;
    testStartTime = millis();
    lastStepTime = testStartTime;
    Serial.printf("Test Started. Target: %.1f -> %.1f, Duration: %lu ms\n", startRPM, endRPM, testDurationMs);
}

void stopTest() {
    isTesting = false;
    leftPID.setTarget(0);
    rightPID.setTarget(0);
    leftMotor.stop();
    rightMotor.stop();
    pidPage.notifyDone();
    Serial.println("Test Stopped.");
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- Starting PIDTest Environment ---");

    leftMotor.init();
    rightMotor.init();

    // 初始化並啟動主專案 WebServerManager
    webServer.begin(WIFI_SSID, WIFI_PASSWORD);
    
    // 確保 WiFi 處於高效能模式以減少 WebSocket 推播延遲
    esp_wifi_set_ps(WIFI_PS_NONE);

    indexPage.attachToServer(webServer.getServer());
    pidPage.attachToServer(webServer.getServer());
    webServer.start();

    Serial.println("Initialization complete. Waiting for WebUI commands...");
}

void loop() {
    pidPage.cleanup();
    leftMotor.update();
    rightMotor.update();
    
    // 1. 處理 WebSocket 傳來的控制指令 (從 Page 取得暫存)
    if (pidPage.has_new_command) {
        pidPage.has_new_command = false;
        if (strncmp(pidPage.last_command, "START", 5) == 0) startTest(pidPage.last_command);
        else if (strncmp(pidPage.last_command, "STOP", 4) == 0) stopTest();
    }

    // 2. 執行 PID 測試狀態機與高速圖表推送
    if (isTesting) {
        unsigned long now = millis();
        
        // 階梯狀目標 RPM 更新
        if (stepRPM > 0 && stepIntervalMs > 0 && (now - lastStepTime >= stepIntervalMs)) {
            currentTargetRPM += stepRPM;
            if (currentTargetRPM > endRPM) currentTargetRPM = endRPM;
            leftPID.setTarget(currentTargetRPM);
            rightPID.setTarget(currentTargetRPM);
            lastStepTime = now;
        }

        // 檢查是否超時結束，未結束則進行頻繁推播資料 (約 50ms 一次)
        if (now - testStartTime >= testDurationMs) stopTest();
        else {
            leftPID.update();
            rightPID.update();

            static unsigned long lastPush = 0;
            if (now - lastPush >= 50) {
                lastPush = now;
                pidPage.broadcastData(now - testStartTime, currentTargetRPM, leftMotor.getCurrRPM(), rightMotor.getCurrRPM());
            }
        }
    }
}