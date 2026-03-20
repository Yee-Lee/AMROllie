/**
 * ============================================================================
 * 檔案: MotorSim.cpp
 * 描述: 模擬馬達邏輯實作 - 支援一階慣性與平滑死區滑行
 * ============================================================================
 */
#include "MotorSim.h"

MotorSim::MotorSim(float minPWM, float satPWM, float maxR, int interval) 
    : _minPWM(minPWM), _satPWM(satPWM), _maxRPM(maxR), _updateInterval(interval) {
    _tau = 0.7f; // 設定慣性時間常數為 0.7 秒
    _currRPM = 0;
    _currentPWM = 0;
    _lastUpdate = millis();
}

void MotorSim::init() { 
    _lastUpdate = millis();
    _currentPWM = 0;
    _currRPM = 0;
}

bool MotorSim::update() {
    unsigned long now = millis();
    unsigned long dt_ms = now - _lastUpdate;

    if (dt_ms >= (unsigned long)_updateInterval) {
        int absPWM = abs(_currentPWM);
        float targetRPM = 0.0;

        // 1. 決定目標轉速 (推力層級)
        if (absPWM < _minPWM) {
            // 低於死區：推力為 0，但轉速會依慣性慢慢減少
            targetRPM = 0.0;
        } 
        else if (absPWM > _satPWM) {
            // 飽和區
            targetRPM = _maxRPM;
        } 
        else {
            // 線性映射公式
            targetRPM = (float)(absPWM - _minPWM) * _maxRPM / (_satPWM - _minPWM);
        }

        // 2. 一階慣性模擬：趨近目標轉速
        // 公式：current = current + (target - current) * (dt / tau)
        float dt_sec = (float)dt_ms / 1000.0f;
        _currRPM += (targetRPM - _currRPM) * (dt_sec / _tau);

        // 3. 極小值校正，避免浮點數抖動
        if (abs(_currRPM) < 0.05f) {
            _currRPM = 0.0;
        }

        _lastUpdate = now;
        return true;
    }
    return false;
}

float MotorSim::getCurrRPM() {
    return _currRPM; 
}

void MotorSim::drive(int pwm) {
    _currentPWM = pwm; 
}

void MotorSim::stop() {
    _currentPWM = 0;
    // 移除直接設 _currRPM = 0，讓 update() 中的慣性邏輯使其自然滑行停止
}