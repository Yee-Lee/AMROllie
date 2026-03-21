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
    
    unsigned long intervalUs; // 計算週期 (微秒)
    unsigned long lastTime;
    
    float integralLimit; // 積分限幅 (防止飽和)
    bool firstRun;       // 旗標：用於初始化 lastError，避免啟動瞬間的 D 項衝擊

public:
    // 預設計算頻率為 100Hz (10ms 週期)
    PIDController(IMotor* m, float p, float i, float d, float iLimit = 50.0f, int hz = 100) 
        : motor(m), kp(p), ki(i), kd(d), targetRPM(0), offsetPWM(0), 
          integral(0), lastError(0), lastTargetRPM(0), integralLimit(iLimit), firstRun(true) {
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

    // 完全重置 PID 內部狀態
    void reset() {
        integral = 0;
        lastError = 0;
        lastTargetRPM = 0;
        firstRun = true;
        if (motor) motor->drive(0);
    }

    // 核心更新邏輯：必須在 loop() 中不斷呼叫
    void update() {
        unsigned long now = micros();
        if (now - lastTime < intervalUs) return;
        
        float dt = (now - lastTime) / 1000000.0f;
        lastTime = now;

        // 確保馬達硬體狀態更新（例如讀取編碼器）
        motor->update();

        float currentRPM = motor->getCurrRPM();
        float error = targetRPM - currentRPM;

        // --- 1. 積分項 (I) 與方向管理 ---
        
        // 清除時機 A：目標為 0 (停止)
        if (abs(targetRPM) < 0.1f) {
            integral = 0.0f;
        }
        // 清除時機 B：方向發生翻轉 (正負號變更)
        else if (targetRPM * lastTargetRPM < 0) {
            integral = 0.0f;
            // 換向時同時重置 lastError，防止 D 項產生巨大的「換向衝擊」
            lastError = error; 
        }
        
        integral += error * dt;
        
        // 積分限幅 (Clamping) - 防止 Anti-windup
        if (integral > integralLimit) integral = integralLimit;
        else if (integral < -integralLimit) integral = -integralLimit;

        // --- 2. 微分項 (D) 防護 ---
        // 處理首次執行或目標劇烈變動（> 10 RPM）時的誤差跳變
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

        // --- 4. 結合前饋控制 (Feed-forward / OffsetPWM) ---
        if (targetRPM > 0.1f) {
            final_output = pid_output + offsetPWM;
        } 
        else if (targetRPM < -0.1f) {
            final_output = pid_output - offsetPWM;
        } 
        else {
            final_output = 0; // 目標為 0，強制出力為 0
        }

        // 限制輸出範圍至硬體極限 (-255 到 255)
        int pwmOut = constrain((int)final_output, -255, 255);

        // 執行馬達驅動 (IMotor 應根據正負值判斷方向)
        motor->drive(pwmOut);

        // 調試用資訊 (可視需求取消註解)
        /*
        Serial.printf("T:%.1f C:%.1f E:%.1f | P:%.1f I:%.1f D:%.1f | PWM:%d\n", 
                      targetRPM, currentRPM, error, p_term, i_term, d_term, pwmOut);
        */
    }
};

#endif