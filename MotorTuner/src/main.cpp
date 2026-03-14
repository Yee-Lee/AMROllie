
/**
 * ============================================================================
 * 檔案: main.cpp
 * 描述: 主程式整合
 * ============================================================================
 */
#include <Arduino.h>
#include "Motor.h"
#include "MotorSim.h"
#include "PIDController.h"
#include "MotorWebUI.h"

enum SystemState { IDLE = 0, RUNNING = 1 };
int currentState = IDLE;
int controlMode = 0;   // 0: PID, 1: PWM Direct
int manualPWM = 0;     // PWM 模式下的手動輸出值

IMotor* motor = new MotorSim(300.0f, 50); 
PIDController pid(0.8, 0.1, 0.01);
MotorWebUI webUI(&pid, &motor, &currentState, &controlMode, &manualPWM);

unsigned long lastWebUpdate = 0;

void setup() {
    Serial.begin(115200);
    motor->init();
    webUI.begin("Yee13m", "12345678"); // 修改為你的 WiFi
}

void loop() {
    webUI.cleanup();

    if (currentState == RUNNING) {
        if (motor->update()) {
            float current = motor->getCurrRPM();
            float target = pid.getTarget();
            float finalPWM = 0;

            if (controlMode == 0) {
                // PID 模式
                finalPWM = pid.compute(current);
            } else {
                // PWM 監控模式 (直接驅動)
                finalPWM = (float)manualPWM;
            }

            motor->drive((int)finalPWM);

            unsigned long now = millis();
            if (now - lastWebUpdate >= 50) {
                webUI.broadcastData(now, current, target, finalPWM);
                lastWebUpdate = now;
            }
        }
    }
}