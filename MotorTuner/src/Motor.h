
/**
 * ============================================================================
 * 檔案: Motor.h
 * 描述: 真實硬體馬達實作 (含中斷處理)
 * ============================================================================
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "IMotor.h"

class Motor : public IMotor {
private:
    int pinA, pinB, pinDir, pinPWM;
    int cpr;
    int updateInterval;
    volatile long pos;
    float currRPM;
    unsigned long lastUpdate;
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

    static void IRAM_ATTR isrWrapper(void* arg);

public:
    Motor(int pa, int pb, int pd, int pwm, int _cpr, int interval = 10);
    void init() override;
    bool update() override;
    float getCurrRPM() override;
    void drive(int pwm) override;
    void stop() override;
};

#endif
