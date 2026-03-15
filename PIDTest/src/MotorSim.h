
/**
 * ============================================================================
 * 檔案: MotorSim.h
 * 描述: 模擬馬達實作
 * ============================================================================
 */
#ifndef MOTOR_SIM_H
#define MOTOR_SIM_H

#include <Arduino.h>
#include "IMotor.h"

class MotorSim : public IMotor {
private:
    unsigned long lastUpdate;
    float _maxRPM;
    int _updateInterval;
    float _currRPM;
    int _currentPWM;
    unsigned long _lastUpdate;
    int _minPWM;
    int _satPWM;

    // 物理參數：假設從 0 到 63% 目標轉速需要 0.1 秒 (慣性)
    float _tau = 0.1f; 

public:
    MotorSim(float minPWM, float satPWM, float maxR, int interval=50) ;
    void init() override;
    bool update() override;
    float getCurrRPM() override;

    void drive(int pwm) override;
    void stop() override;
};

#endif
