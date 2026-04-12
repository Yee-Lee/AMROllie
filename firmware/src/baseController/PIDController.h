/**
 * ============================================================================
 * 檔案: PIDController.h
 * 描述: 具備方向翻轉重置與前饋控制的 PID 控制器
 * ============================================================================
 */
#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

#include <Arduino.h>
#include "Motor.h"

class PIDController {
private:
    IMotor* motor;
    float kp, ki, kd;
    float targetRPM;
    float offsetPWM;
    
    float integral;
    float lastError;
    float lastTargetRPM;
    int lastPWM;
    
    unsigned long intervalUs; // 計算週期 (微秒)
    unsigned long lastTime;
    
    float integralLimit; // 積分限幅 (防止飽和)
    bool firstRun;

public:
    // 預設計算頻率為 100Hz (10ms 週期)
    PIDController(IMotor* m, float p, float i, float d, float iLimit = 50.0f, int hz = 100) 
        : motor(m), kp(p), ki(i), kd(d), targetRPM(0), offsetPWM(0), 
          integral(0), lastError(0), lastTargetRPM(0), lastPWM(0), integralLimit(iLimit), firstRun(true) {
        intervalUs = 1000000 / hz;
        lastTime = micros();
    }

    void setTarget(float target) {
        targetRPM = target;
    }

    void setTunings(float p, float i, float d) {
        kp = p; ki = i; kd = d;
    }

    void setOffset(float offset) {
        offsetPWM = offset;
    }

    int getLastPWM() const { return lastPWM; }

    void reset() {
        integral = 0;
        lastError = 0;
        lastTargetRPM = 0;
        firstRun = true;
        if (motor) motor->drive(0);
    }

    void update() {
        unsigned long now = micros();
        // 扣除 1000us 的寬容值，避免 FreeRTOS vTaskDelayUntil 喚醒時差 (Jitter) 
        // 導致不小心跳過整個運算週期，造成 PID 頻率減半或不穩
        if (now - lastTime < intervalUs - 1000) return;
        
        float dt = (now - lastTime) / 1000000.0f;
        lastTime = now;

        motor->update();

        float currentRPM = motor->getCurrRPM();
        float error = targetRPM - currentRPM;

        // --- 1. 積分項 (I) ---
        
        if (abs(targetRPM) < 0.1f) {
            integral = 0.0f;
        }
        else if (targetRPM * lastTargetRPM < 0) {
            integral = 0.0f;
            lastError = error; 
        }
        
        integral += error * dt;
        
        if (integral > integralLimit) integral = integralLimit;
        else if (integral < -integralLimit) integral = -integralLimit;

        // --- 2. 微分項 (D) ---
        if (firstRun || abs(targetRPM - lastTargetRPM) > 10.0f) {
            lastError = error;
            firstRun = false;
        }

        float derivative = (error - lastError) / dt;
        lastError = error;
        lastTargetRPM = targetRPM;

        // --- 3. 計算 PID 輸出 ---
        float p_term = kp * error;
        float i_term = ki * integral;
        float d_term = kd * derivative;
        
        float pid_output = p_term + i_term + d_term;
        float final_output = 0;

        // --- 4. 前饋控制 ---
        // 動態前饋 (Velocity Feed-Forward)
        // 根據先前數據，大約每 1 RPM 需要額外 0.9 的 PWM 動力
        float feed_forward = targetRPM * 0.9f;

        if (targetRPM > 0.1f) {
            final_output = pid_output + offsetPWM + feed_forward;
        } 
        else if (targetRPM < -0.1f) {
            final_output = pid_output - offsetPWM + feed_forward;
        } 
        else {
            final_output = 0; // 目標為 0，強制出力為 0
        }

        // 限制輸出範圍至硬體極限 (-255 到 255)
        int pwmOut = constrain((int)final_output, -255, 255);

        lastPWM = pwmOut;

        // 執行馬達驅動 (IMotor 應根據正負值判斷方向)
        motor->drive(pwmOut);
    }
};

#endif