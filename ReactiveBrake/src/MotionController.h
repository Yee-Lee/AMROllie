#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include <Arduino.h>
#include "PIDController.h"

/**
 * @struct MotionTelemetry
 * @brief 用於監控與除錯的遙測數據包
 */
struct MotionTelemetry {
    float raw_v;      // 搖桿原始線速度
    float raw_w;      // 搖桿原始角速度
    float current_v;  // 緩坡處理後的當前線速度
    float current_w;  // 緩坡處理後的當前角速度
    float target_rpm_l; // 換算後的左輪目標 RPM
    float target_rpm_r; // 換算後的右輪目標 RPM
};

/**
 * @class MotionController
 * @brief 負責處理 AMR 的運動學逆解、加速度緩坡與最高速度限制
 */
class MotionController {
private:
    PIDController* leftPID;
    PIDController* rightPID;

    // 機構參數
    float wheelBase;     // 左右輪距 (m)
    float wheelDiameter; // 輪子直徑 (m)

    // 速度與加速度限制
    float maxV;       // 最高線速度 (m/s)
    float maxW;       // 最高角速度 (rad/s)
    float maxAccelV;  // 最大線加速度 (m/s^2)
    float maxAccelW;  // 最大角加速度 (rad/s^2)

    // 當前平滑狀態
    float currentV = 0.0f;
    float currentW = 0.0f;
    unsigned long lastUpdateTime = 0;

    // 遙測數據
    MotionTelemetry telemetry;

    /**
     * @brief 線性插值/限制函數 (Slew Rate Limit)
     */
    float applyRamp(float target, float current, float maxStep) {
        if (target > current + maxStep) return current + maxStep;
        if (target < current - maxStep) return current - maxStep;
        return target;
    }

public:
    /**
     * @brief 建構子
     * @param lPID 左輪 PID 控制器指標
     * @param rPID 右輪 PID 控制器指標
     * @param wBase 輪距 (預設 0.15m)
     * @param wDiam 輪徑 (預設 0.065m)
     */
    MotionController(PIDController* lPID, PIDController* rPID, float wBase = 0.15f, float wDiam = 0.065f) 
        : leftPID(lPID), rightPID(rPID), wheelBase(wBase), wheelDiameter(wDiam) {
        
        // 預設安全限制
        maxV = 1.0f;
        maxW = 2.0f;
        maxAccelV = 1.0f;
        maxAccelW = 2.0f;
    }

    /**
     * @brief 設定速度與加速度上限
     */
    void setLimits(float max_v, float max_w, float max_accel_v, float max_accel_w) {
        maxV = max_v;
        maxW = max_w;
        maxAccelV = max_accel_v;
        maxAccelW = max_accel_w;
    }

    /**
     * @brief 初始化時間戳記 (在 setup 的最後呼叫)
     */
    void begin() {
        lastUpdateTime = millis();
    }

    /**
     * @brief 緊急停止 (將目標速度歸零)
     */
    void stop() {
        currentV = 0.0f;
        currentW = 0.0f;
        leftPID->setTarget(0.0f);
        rightPID->setTarget(0.0f);
    }

    /**
     * @brief 更新運動狀態 (在 loop 中呼叫)
     * @param target_v 目標線速度 (m/s)
     * @param target_w 目標角速度 (rad/s)
     * @param w_correction 角速度 PID 的修正輸出
     * @return MotionTelemetry 當前的狀態數據，便於監控
     */
    MotionTelemetry update(float target_v, float target_w, float w_correction = 0.0f) {
        // 1. 計算時間差 dt
        unsigned long now = millis();
        float dt = (now - lastUpdateTime) / 1000.0f;
        if (dt <= 0.001f) dt = 0.001f; // 防呆
        lastUpdateTime = now;

        // 2. 限制最高速度 (Hard Limits)
        target_v = constrain(target_v, -maxV, maxV);
        target_w = constrain(target_w, -maxW, maxW);
        // 3. 處理加速度緩坡 (Slew Rate Limiter)
        float vStep = maxAccelV * dt;
        float wStep = maxAccelW * dt;

        currentV = applyRamp(target_v, currentV, vStep);
        currentW = applyRamp(target_w, currentW, wStep);
        
        // 4. 運動學逆解 (Inverse Kinematics)
        float rpm_left = 0.0f;
        float rpm_right = 0.0f;

        if (abs(currentV) < 0.001f && abs(currentW) < 0.001f) {
            // 當搖桿與緩坡皆歸零時，強制無視 w_correction 並將馬達目標歸零
            currentV = 0; currentW = 0;
            leftPID->setTarget(0);
            rightPID->setTarget(0);
        } else {
            // 將 PID 修正量當作「角速度補償」(rad/s) 直接加到目標角速度上
            float corrected_w = currentW + w_correction;
            float v_left = currentV - (corrected_w * wheelBase / 2.0f);
            float v_right = currentV + (corrected_w * wheelBase / 2.0f);

            rpm_left = (v_left * 60.0f) / (PI * wheelDiameter);
            rpm_right = (v_right * 60.0f) / (PI * wheelDiameter);

            // 強制最小有效轉速 (例如 25.0 RPM)
            // 如果目標轉速大於 0.1 但低於 25，則自動提升至 25 RPM 以確保馬達平順運轉
            float min_rpm = 25.0f;
            if (abs(rpm_left) > 0.1f && abs(rpm_left) < min_rpm) {
                rpm_left = (rpm_left > 0) ? min_rpm : -min_rpm;
            }
            if (abs(rpm_right) > 0.1f && abs(rpm_right) < min_rpm) {
                rpm_right = (rpm_right > 0) ? min_rpm : -min_rpm;
            }

            leftPID->setTarget(rpm_left);
            rightPID->setTarget(rpm_right);
        }

        // 5. 更新遙測數據
        telemetry.raw_v = target_v;
        telemetry.raw_w = target_w;
        telemetry.current_v = currentV;
        telemetry.current_w = currentW;
        telemetry.target_rpm_l = rpm_left;
        telemetry.target_rpm_r = rpm_right;

        return telemetry;
    }
};

#endif // MOTION_CONTROLLER_H