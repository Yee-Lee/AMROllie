/**
 * ============================================================================
 * 檔案: MotorScanner.cpp
 * 描述: 馬達掃描類別，用於自動掃描馬達的 PWM-RPM 特性
 * ============================================================================
 */
#include "MotorScanner.h"

MotorScanner::MotorScanner(IMotor* motor) : _motor(motor), _currentPWM(0), _timerStart(0), _scannerState(IDLE),
                                            _startPWM(100), _endPWM(250), _stepSize(5), _interval(500) {
}

void MotorScanner::begin(IMotor *motor) {
    _motor = motor;
    _currentPWM = _startPWM;
    _timerStart = millis();
    _scannerState = SCAN;
    Serial.println("Scanner Begin");
    _motor->drive(_currentPWM);
}

void MotorScanner::run() {
    if (_scannerState != SCAN) return;

    unsigned long now = millis();
    if (now - _timerStart >= _interval) {
        _currentPWM += _stepSize;
        if (_currentPWM >= _endPWM) {
            _currentPWM = 0;
            _scannerState = DONE;
            Serial.println("Scanner Done");
            _motor->stop();
        }
        _motor->drive(_currentPWM);
        _timerStart = now;
    }
}

void MotorScanner::reset() {
    _currentPWM = 0;
    _scannerState = IDLE;
    _timerStart = 0;
    Serial.println("Scanner Reset");
    _motor->stop();
}

int MotorScanner::getCurrentPWM() {
    return _currentPWM;
}

MotorScanner::ScannerState getScannerState();
MotorScanner::ScannerState MotorScanner::getScannerState() { return _scannerState; }