/**
 * ============================================================================
 * 檔案: PIDTest.h
 * 描述: PID 步階響應測試
 * ============================================================================
 */
#ifndef PIDTEST_H
#define PIDTEST_H

#include "PIDController.h"

enum TestState { TEST_IDLE = 0, TEST_TESTING, TEST_DONE };

class PIDTest {
private:
    PIDController* pid;
    float startRpm, endRpm, stepRpm;
    unsigned long intervalMs, durationMs;
    
    unsigned long startTime;
    unsigned long lastStepTime;
    unsigned long lastPrintTime;
    float currentTarget;
    
public:
    TestState state;

    PIDTest(PIDController* p, float start, float end, float step, unsigned long interval, unsigned long duration)
        : pid(p), startRpm(start), endRpm(end), stepRpm(step), 
          intervalMs(interval), durationMs(duration), state(TEST_IDLE) {}

    void reset() {
        state = TEST_IDLE;
        pid->setTarget(0);
        pid->reset();
    }

    void begin() {
        state = TEST_TESTING;
        startTime = millis();
        lastStepTime = startTime;
        lastPrintTime = startTime;
        currentTarget = startRpm;
        pid->setTarget(currentTarget);
        Serial.println("--- PID Test Started ---");
        Serial.println("Time(ms),TargetRPM,CurrentRPM");
    }

    void run(IMotor* motor) {
        if (state != TEST_TESTING) return;
        
        unsigned long now = millis();

        // 檢查是否達到總測試時間
        if (now - startTime >= durationMs) {
            state = TEST_DONE;
            pid->setTarget(0);
            Serial.println("--- PID Test Done ---");
            return;
        }

        // 階躍邏輯處理
        if (intervalMs > 0 && (now - lastStepTime >= intervalMs)) {
            lastStepTime = now;
            if (currentTarget < endRpm) {
                currentTarget += stepRpm;
                if (currentTarget > endRpm) currentTarget = endRpm;
                pid->setTarget(currentTarget);
            }
        }

        //  telemetry 輸出 (每 50ms 印一次，供 Serial Plotter 繪圖)
        if (now - lastPrintTime >= 50) {
            lastPrintTime = now;
            Serial.printf("[%5lu] Target: %6.2f, Current: %6.2f\n",
                          now - startTime, currentTarget, motor->getCurrRPM());
        }
    }
};

#endif