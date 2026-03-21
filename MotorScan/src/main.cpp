
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

enum SystemState { IDLE = 0, RUNNING = 1};
int currentState = IDLE;
bool isManual = false;

/* Simulated motors */
//IMotor* leftMotor = new MotorSim(120.0f, 220.0f, 300.0f, 50); 
//IMotor* rightMotor = new MotorSim(100.0f, 180.0f, 240.0f, 50);

/* Real motors*/
IMotor *leftMotor = new Motor(12, 14, 13, 18, 34, 292, false, 10);
IMotor *rightMotor = new Motor(27, 26, 25, 19, 35, 292, true, 10);

IMotor* currentMotor = leftMotor;

MotorScanner scanner(nullptr);
MotorWebUI webUI(nullptr, &currentState, &isManual);

unsigned long lastWebUpdate = 0;
unsigned long lastSerialPrint = 0;

const char* sStateStr[] = {"IDLE", "RUNNING", "DONE"};

void setup() {
    Serial.begin(115200);
    leftMotor->init();
    rightMotor->init();
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
    }
    if (currentState == RUNNING) {
        unsigned long now = millis();
         
        if (!isManual) {
            if (scanner.getScannerState() == MotorScanner::ScannerState::IDLE)
                scanner.begin(currentMotor);

            if (scanner.getScannerState() == MotorScanner::ScannerState::SCAN)
                scanner.run();

            // 更新馬達
            if (currentMotor->update()) {
                float current = currentMotor->getCurrRPM();
                Serial.printf(">PWM:%d|g\n", scanner.getCurrentPWM());
                Serial.printf(">RPM:%.2f|g\n",current);
            }
        }

        // 每 100ms 更新一次網頁數據，避免傳輸過於頻繁
        if (now - lastWebUpdate >= 100) {
            float currentPWM = scanner.getCurrentPWM();
            float current = currentMotor->getCurrRPM();
            webUI.broadcastData(now, current, currentPWM, 
                sStateStr[scanner.getScannerState()]);
            lastWebUpdate = now;

            // 更新打印手動測試
            if (isManual)
                currentMotor->print_details();
        }
    } else if (currentState == IDLE) {
        if (scanner.getScannerState() != MotorScanner::ScannerState::IDLE) {
            scanner.reset();
        }
    }
}