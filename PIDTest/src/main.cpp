
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
int currentState = RUNNING;

/* Simulated motors */
// MotorSim leftMotor = MotorSim(120.0f, 220.0f, 300.0f, 5); 
// MotorSim rightMotor = MotorSim(100.0f, 180.0f, 240.0f, 50);

/* Real motors*/
Motor leftMotor = Motor(12, 14, 13, 18, 34, 146, false, 20);
Motor rightMotor = Motor(27, 26, 25, 19, 35, 146, true, 20);

IMotor* pCurrentMotor = &leftMotor;

MotorWebUI webUI(nullptr, &currentState);

// 參數: Kp=2.0, Ki=5.0, Kd=0.1, 積分限幅=50, 頻率=100Hz
PIDController leftPID(&leftMotor, 2.0, 3.0, 0.005, 50.0, 100); 
PIDController rightPID(&rightMotor, 2.0, 3.0, 0.005, 50.0, 100); 

// 測試參數: 起始 20 RPM, 終點 100 RPM, 每次增加 20 RPM, 區間 2000ms, 總時長 10000ms
PIDTest pidTest(&leftPID, 40.0, 100.0, 20.0, 0, 5000);
//PIDTest pidTest(&rightPID, 40.0, 100.0, 20.0, 0, 5000);


unsigned long lastWebUpdate = 0;
unsigned long lastSerialPrint = 0;

const char* sStateStr[] = {"IDLE", "RUNNING", "DONE"};



void setup() {
    Serial.begin(115200);
    leftMotor.init();
    rightMotor.init();
    leftPID.setOffset(120);
    rightPID.setOffset(120);

    leftPID.setTarget(0);
    rightPID.setTarget(0);

//    webUI.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
//    webUI.cleanup();
    // if (webUI.hasMotorChanged()) {
    //     int idx = webUI.getSelectedMotorIndex();

    //     currentMotor = (idx == 0) ? leftMotor : rightMotor;
    //     // 同步更新所有相關物件的指針
    //     webUI.setCurrentMotor(currentMotor);
    // }

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
        pidTest.run(&leftMotor);
        //pidTest.run(&rightMotor);
        
        // 如果測試完畢，自動切回 IDLE
        if (pidTest.state == TEST_DONE) {
            currentState = IDLE;
        }

        // 每 100ms 更新一次網頁數據，避免傳輸過於頻繁
        // if (now - lastWebUpdate >= 100) {
        //     float currentPWM = scanner.getCurrentPWM();
        //     webUI.broadcastData(now, current, currentPWM, 
        //         sStateStr[scanner.getScannerState()]);
        //     lastWebUpdate = now;
        // }
    }

    // 3. 固定更新馬達與 PID
    leftMotor.update();
    leftPID.update();
    rightMotor.update();
    rightPID.update();
}

// void loop() {
//     webUI.cleanup();

//     // 只有在 UI 真的有切換時才執行內部的指針重設邏輯
//     if (webUI.hasMotorChanged()) {
//         int idx = webUI.getSelectedMotorIndex();

//         currentMotor = (idx == 0) ? leftMotor : rightMotor;
//         // 同步更新所有相關物件的指針
//         webUI.setCurrentMotor(currentMotor);
//     }
//     if (currentState == RUNNING) {
//         if (scanner.getScannerState() == MotorScanner::ScannerState::IDLE)
//             scanner.begin(currentMotor);

//         if (scanner.getScannerState() == MotorScanner::ScannerState::SCAN)
//             scanner.run();

//         if (currentMotor->update()) {
//             unsigned long now = millis();
//             float current = currentMotor->getCurrRPM();
//             //  Serial.printf("PWM: %d, RPM: %.2f\n", scanner.getCurrentPWM(), current);

//             // 每 100ms 更新一次網頁數據，避免傳輸過於頻繁
//             if (now - lastWebUpdate >= 100) {
//                 float currentPWM = scanner.getCurrentPWM();
//                 webUI.broadcastData(now, current, currentPWM, 
//                     sStateStr[scanner.getScannerState()]);
//                 lastWebUpdate = now;
//             }
//          }
//     } else if (currentState == IDLE) {
//         if (scanner.getScannerState() != MotorScanner::ScannerState::IDLE) {
//             scanner.reset();
//         }
//     }
// }