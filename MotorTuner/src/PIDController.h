
/**
 * ============================================================================
 * 檔案: PIDController.h
 * 描述: PID 演算法控制器
 * ============================================================================
 */
#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>

class PIDController {
private:
    float kp, ki, kd;
    float target;
    float integral;
    float prevError;
    unsigned long lastTime;
    float outMin, outMax;

public:
    PIDController(float p, float i, float d, float minVal = -255, float maxVal = 255);
    void setTunings(float p, float i, float d);
    void setTarget(float t);
    float getTarget();
    void reset();
    float compute(float currentValue);
};

#endif
