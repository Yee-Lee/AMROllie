
/**
 * ============================================================================
 * 檔案: MotorSim.cpp
 * ============================================================================
 */
#include "MotorSim.h"

MotorSim::MotorSim(float minPWM, float satPWM, float maxR, int interval) 
    : _minPWM(minPWM), _satPWM(satPWM), _maxRPM(maxR), _updateInterval(interval) {}

void MotorSim::init() { 
    _lastUpdate = 0;
    _currentPWM = 0;
    _currRPM = 0;
}

bool MotorSim::update() {
        unsigned long now = millis();
        if (now - _lastUpdate >= _updateInterval) {
            int absPWM = abs(_currentPWM);

            if (absPWM < _minPWM) {
                _currRPM = 0.0; // 死區
            } 
            else if (absPWM > _satPWM) {
                _currRPM = _maxRPM; // 飽和區
            } 
            else {
                // 線性映射公式
                float targetRPM = (float)(absPWM - _minPWM) * _maxRPM / (_satPWM - _minPWM);
                // 一階慣性模擬：current = current + (target - current) * (dt / tau)
                // 這會讓轉速平滑地趨近目標
                _currRPM += (targetRPM - _currRPM) * (((now - _lastUpdate)/1000.0f) / _tau );
        
            }
            _lastUpdate = now;
            return true;
        }
        return false;
    }

float MotorSim::getCurrRPM() { return _currRPM; }
void MotorSim::drive(int pwm) { _currentPWM = pwm; }
void MotorSim::stop() { _currentPWM = 0; _currRPM = 0;}
