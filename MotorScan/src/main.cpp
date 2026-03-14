
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
#include "MotorScanner.h"
#include "config.h"

enum SystemState { IDLE = 0, RUNNING = 1 };
int currentState = IDLE;

// IMotor* leftMotor = new MotorSim(120.0f, 220.0f, 300.0f, 50); 
// IMotor* rightMotor = new MotorSim(100.0f, 180.0f, 240.0f, 50);
IMotor *leftMotor = new Motor(12, 14, 13, 18, 34, 146, false, 20);
IMotor *rightMotor = new Motor(27, 26, 25, 19, 35, 146, true, 20);

IMotor* currentMotor = leftMotor;

MotorScanner scanner(nullptr);
MotorWebUI webUI(nullptr, &currentState);

unsigned long lastWebUpdate = 0;
unsigned long lastSerialPrint = 0;

const char* sStateStr[] = {"IDLE", "RUNNING", "DONE"};

void setup() {
    Serial.begin(115200);
    leftMotor->init();
    rightMotor->init();
    Serial.printf("leftMotor=0x%x, rightMotor=0x%x\n", (uint32_t)leftMotor, (uint32_t)rightMotor);
    webUI.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
    webUI.cleanup();

    // 只有在 UI 真的有切換時才執行內部的指針重設邏輯
    if (webUI.hasMotorChanged()) {
        int idx = webUI.getSelectedMotorIndex();
        currentMotor = (idx == 0) ? leftMotor : rightMotor;
        
        // 同步更新所有相關物件的指針
        webUI.setCurrentMotor(currentMotor);
        //scanner.begin(*currentMotor); // 重新初始化 scanner 指向新馬達
        Serial.printf("Motor switched to: %s\n", (idx == 0) ? "LEFT" : "RIGHT");
    }
    if (currentState == RUNNING) {
        if (scanner.getScannerState() == MotorScanner::ScannerState::IDLE) {
            // webUI.setCurrentMotor(currentMotor);
            scanner.begin(currentMotor);
        }
         
        if (scanner.getScannerState() == MotorScanner::ScannerState::SCAN) {
            scanner.run();
        }

        if (currentMotor->update()) {
            unsigned long now = millis();
            float current = currentMotor->getCurrRPM();
          //  Serial.printf("PWM: %d, RPM: %.2f\n", scanner.getCurrentPWM(), current);
          // 每 100ms 更新一次網頁數據，避免傳輸過於頻繁
            if (now - lastWebUpdate >= 100) {
                float currentPWM = scanner.getCurrentPWM();
                webUI.broadcastData(now, current, currentPWM, 
                    sStateStr[scanner.getScannerState()]);
                lastWebUpdate = now;
            }
         }
    } else if (currentState == IDLE) {
        if (scanner.getScannerState() != MotorScanner::ScannerState::IDLE) {
            scanner.reset();
        }
    }
}