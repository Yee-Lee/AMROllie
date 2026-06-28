#ifndef MOTOR_SCANNER_H
#define MOTOR_SCANNER_H

#include <Arduino.h>
#include "../../src/IMotor.h"

class MotorScanner {
public:
    enum ScannerState {
        IDLE = 0,
        SCAN = 1,
        DONE = 2
    };

private:
    IMotor* _motor;
    int _currentPWM;
    unsigned long _timerStart;
    ScannerState _scannerState;

    int _startPWM;
    int _endPWM;
    int _stepSize;
    int _interval;
    bool _reverse;

public:
    MotorScanner(IMotor* motor = nullptr) : _motor(motor), _currentPWM(0), _timerStart(0), _scannerState(IDLE),
                                            _startPWM(100), _endPWM(255), _stepSize(5), _interval(500), _reverse(false) {}

    void begin(IMotor *motor, bool reverse = false) {
        _motor = motor;
        _reverse = reverse;
        _currentPWM = _startPWM;
        _timerStart = millis();
        _scannerState = SCAN;
        Serial.printf("Scanner Begin (reverse: %s)\n", _reverse ? "true" : "false");
        if (_motor) _motor->drive(_reverse ? -_currentPWM : _currentPWM);
    }

    void run() {
        if (_scannerState != SCAN || !_motor) return;

        unsigned long now = millis();
        if (now - _timerStart >= _interval) {
            _currentPWM += _stepSize;
            if (_currentPWM >= _endPWM) {
                _currentPWM = 0;
                _scannerState = DONE;
                Serial.println("Scanner Done");
                _motor->stop();
            } else {
                _motor->drive(_reverse ? -_currentPWM : _currentPWM);
            }
            _timerStart = now;
        }
    }

    void reset() {
        _currentPWM = 0;
        _scannerState = IDLE;
        _timerStart = 0;
        Serial.println("Scanner Reset");
        if (_motor) _motor->stop();
    }

    int getCurrentPWM() { return _currentPWM; }
    ScannerState getScannerState() { return _scannerState; }
};

#endif