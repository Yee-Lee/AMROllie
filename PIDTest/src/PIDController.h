/**
 * ============================================================================
 * 檔案: PIDController.h
 * 描述: PID 控制器
 * ============================================================================
 */
#ifndef PIDCONTROLLER_H
#define PIDCONTROLLER_H

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
    
    float integralLimit; // 積分限幅
    bool firstRun;       // 防微分衝擊旗標

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

    // 將 PID 內部狀態重置
    void reset() {
        integral = 0;
        lastError = 0;
        firstRun = true;
        motor->drive(0);
    }

    // 必須在 loop() 中不斷呼叫，它會根據 hz 自動控制計算頻率
    void update() {
        unsigned long now = micros();
        if (now - lastTime < intervalUs) return;
        
        float dt = (now - lastTime) / 1000000.0f;
        lastTime = now;

        float currentRPM = motor->getCurrRPM();
        float error = targetRPM - currentRPM;

        // --- 1. 積分項 (I) 管理 ---
        // 清除時機：Target 為 0 或 發生方向切換
        if (targetRPM == 0.0f || (targetRPM * lastTargetRPM < 0)) {
            integral = 0.0f;
        }
        
        integral += error * dt;
        
        // 積分限幅 (Clamping)
        if (integral > integralLimit) integral = integralLimit;
        else if (integral < -integralLimit) integral = -integralLimit;

        // --- 2. 微分項 (D) 防護 ---
        // 避免啟動瞬間或目標劇烈改變時的「微分衝擊」
        if (firstRun || abs(targetRPM - lastTargetRPM) > 10.0f) {
            lastError = error;
            firstRun = false;
        }

        float derivative = (error - lastError) / dt;
        lastError = error;
        lastTargetRPM = targetRPM;

        // --- 計算輸出 ---
        float p_term = kp * error;
        float i_term = ki * integral;
        float d_term = kd * derivative;
        float output = p_term + i_term + d_term + offsetPWM;

        // 限制 PWM 範圍 (-255 到 255)
        int pwmOut = constrain((int)output, 0, 255);
        
        if (targetRPM == 0.0f) {
            pwmOut = 0; // 若目標為0，確保沒有微小輸出
        }

        // Serial.printf("[PID] RPM: %6.2f, Err: %6.2f, P: %6.2f, I: %6.2f, D: %6.2f, PWM: %4d\n",
        //                 currentRPM, error, p_term, i_term, d_term, pwmOut);

        motor->drive(pwmOut);
    }
};

#endif