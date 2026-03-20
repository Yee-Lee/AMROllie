/**
 * ============================================================================
 * 檔案: PIDTest.h
 * 描述: 支援單輪或雙輪同步 PID 步階響應測試。
 * ============================================================================
 */
#ifndef PIDTEST_H
#define PIDTEST_H

#include "PIDController.h"
#include <Arduino.h>

enum TestState { TEST_IDLE = 0, TEST_TESTING, TEST_DONE };

class PIDTest {
private:
    PIDController *pidL, *pidR;
    float startRpm, endRpm, stepRpm;
    unsigned long intervalMs, durationMs;
    
    unsigned long startTime;
    unsigned long lastStepTime;
    unsigned long lastPrintTime;
    float currentTarget;
    
public:
    TestState state;

    PIDTest(PIDController* pL, PIDController* pR, float start, float end, float step, unsigned long interval, unsigned long duration)
        : pidL(pL), pidR(pR), startRpm(start), endRpm(end), stepRpm(step), 
          intervalMs(interval), durationMs(duration), state(TEST_IDLE) {}

    // 允許動態修改測試參數
    void setParams(float start, float end, float step, unsigned long interval, unsigned long duration) {
        startRpm = start; endRpm = end; stepRpm = step;
        intervalMs = interval; durationMs = duration;
    }

    void reset() {
        state = TEST_IDLE;
        if (pidL) { pidL->setTarget(0); pidL->reset(); }
        if (pidR) { pidR->setTarget(0); pidR->reset(); }
    }

    void begin() {
        state = TEST_TESTING;
        startTime = millis();
        lastStepTime = startTime;
        lastPrintTime = startTime;
        currentTarget = startRpm;
        if (pidL) pidL->setTarget(currentTarget);
        if (pidR) pidR->setTarget(currentTarget);
    }

    // 讓 run 傳回當前數據，方便網頁廣播
    struct Telemetry {
        unsigned long time;
        float target;
        float left;
        float right;
        bool updated;
    };

    Telemetry run(IMotor* motorL, IMotor* motorR) {
        Telemetry data = {0, 0, 0, 0, false};
        if (state != TEST_TESTING) return data;
        
        unsigned long now = millis();

        if (now - startTime >= durationMs) {
            state = TEST_DONE;
            if (pidL) pidL->setTarget(0);
            if (pidR) pidR->setTarget(0);
            return data;
        }

        if (intervalMs > 0 && (now - lastStepTime >= intervalMs)) {
            lastStepTime = now;
            if (currentTarget < endRpm) {
                currentTarget += stepRpm;
                if (currentTarget > endRpm) currentTarget = endRpm;
                if (pidL) pidL->setTarget(currentTarget);
                if (pidR) pidR->setTarget(currentTarget);
            }
        }

        if (now - lastPrintTime >= 50) {
            lastPrintTime = now;
            data.time = now - startTime;
            data.target = currentTarget;
            data.left = (pidL && motorL) ? motorL->getCurrRPM() : 0;
            data.right = (pidR && motorR) ? motorR->getCurrRPM() : 0;
            data.updated = true;
        }
        return data;
    }
};

#endif