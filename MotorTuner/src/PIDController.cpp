
/**
 * ============================================================================
 * 檔案: PIDController.cpp
 * ============================================================================
 */

#include "PIDController.h"

PIDController::PIDController(float p, float i, float d, float minVal, float maxVal) 
    : kp(p), ki(i), kd(d), target(0), integral(0), prevError(0), lastTime(0), outMin(minVal), outMax(maxVal) {}

void PIDController::setTunings(float p, float i, float d) {
    kp = p; ki = i; kd = d;
}

void PIDController::setTarget(float t) {
    target = t;
}

float PIDController::getTarget() {
    return target;
}

void PIDController::reset() {
    integral = 0;
    prevError = 0;
    lastTime = millis();
}

float PIDController::compute(float currentValue) {
    unsigned long now = millis();
    float dt = (float)(now - lastTime) / 1000.0f;
    if (dt <= 0) return 0; 

    float error = target - currentValue;
    integral += error * dt;

    ki = kd = 0;

    // 抗飽和 (Anti-windup)
    if (integral * ki > outMax) integral = outMax / ki;
    else if (integral * ki < outMin) integral = outMin / ki;

    float derivative = (error - prevError) / dt;
    float output = (kp * error) + (ki * integral) + (kd * derivative);

    if (output > outMax) output = outMax;
    if (output < outMin) output = outMin;

    prevError = error;
    lastTime = now;
    return output;
}
