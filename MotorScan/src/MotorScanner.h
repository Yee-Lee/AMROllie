/**
 * ============================================================================
 * 檔案: MotorScanner.h
 * 描述: 馬達掃描類別，用於自動掃描馬達的 PWM-RPM 特性
 * ============================================================================
 */
#ifndef MOTOR_SCANNER_H
#define MOTOR_SCANNER_H

#include <Arduino.h>
#include "IMotor.h"


class MotorScanner {
private:
    IMotor* _motor;
    int _currentPWM;
    unsigned long _timerStart;

public:
    enum ScannerState {
        IDLE = 0,
        SCAN = 1,
        DONE = 2
  };
private:
    ScannerState _scannerState;

    int _startPWM;
    int _endPWM;
    int _stepSize;
    int _interval;
public:
    MotorScanner(IMotor* motor);
    void begin(IMotor* motor);
    void run();
    void reset();
    int getCurrentPWM();
    ScannerState getScannerState() ;
};
#endif