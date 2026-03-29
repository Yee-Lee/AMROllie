/**
 * ============================================================================
 * 檔案: Motor.h
 * 描述: 真實硬體馬達實作 (適配 IN1, IN2, PWM 驅動架構)
 * ============================================================================
 */
#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include "IMotor.h"

class Motor : public IMotor {
private:
    // 控制引腳
    int _pinIN1, _pinIN2, _pinPWM;
    // 編碼器引腳
    int _pinEncA, _pinEncB;
    int _ledcChannel;
    
    int _cpr;
    int _updateInterval;
    bool _isReversed;

    volatile long _pos;
    float _currRPM;
    unsigned long _lastUpdate;
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

    static void IRAM_ATTR isrWrapper(void* arg);

public:
    // 建構子參數：IN1, IN2, PWM, EncA, EncB, CPR, 更新頻率
    Motor(int in1, int in2, int pwm, int encA, int encB, int cpr, bool reverse, int interval = 10);

    void init() override;
    bool update() override;
    float getCurrRPM() override;
    void drive(int pwm) override;
    void stop() override;
};

#endif