
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
    float maxRPM;
    int updateInterval;
    float currRPM;
    int currentPWM;
    unsigned long lastUpdate;

public:
    MotorSim(float maxR, int interval = 10);
    void init() override;
    bool update() override;
    float getCurrRPM() override;
    void drive(int pwm) override;
    void stop() override;
};

#endif
