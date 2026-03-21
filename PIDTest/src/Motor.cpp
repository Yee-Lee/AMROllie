/**
 * ============================================================================
 * 檔案: Motor.cpp
 * 描述: 實體馬達驅動實作，採用 ESP32 LEDC (PWM) 硬體加速。
 * 修正: 實作 _isReversed 邏輯，統一物理運動方向與數據正負號。
 * ============================================================================
 */
#include "Motor.h"

static uint8_t next_ledc_channel = 0; 

Motor::Motor(int in1, int in2, int pwm, int encA, int encB, int cpr, bool reverse, int interval) 
    : _pinIN1(in1), _pinIN2(in2), _pinPWM(pwm),
      _pinEncA(encA), _pinEncB(encB),
      _cpr(cpr), _isReversed(reverse), _updateInterval(interval),
      _pos(0), _currRPM(0), _lastUpdate(0) {

    _ledcChannel = next_ledc_channel++;
}

void IRAM_ATTR Motor::isrWrapper(void* arg) {
    Motor* instance = (Motor*)arg;
    // 讀取 A, B 相判斷方向 (雙沿觸發)
    bool stateA = digitalRead(instance->_pinEncA);
    bool stateB = digitalRead(instance->_pinEncB);

    // 如果設定了反相，則將計數方向取反
    if (instance->_isReversed) {
        (stateA == stateB) ? instance->_pos-- : instance->_pos++;
    } else {
        (stateA == stateB) ? instance->_pos++ : instance->_pos--;
    }
}

void Motor::init() {
    pinMode(_pinIN1, OUTPUT);
    pinMode(_pinIN2, OUTPUT);
    
    ledcSetup(_ledcChannel, 5000, 8);
    ledcAttachPin(_pinPWM, _ledcChannel);
    
    pinMode(_pinEncA, INPUT_PULLUP);
    pinMode(_pinEncB, INPUT_PULLUP);
    
    attachInterruptArg(digitalPinToInterrupt(_pinEncA), isrWrapper, this, CHANGE);

    stop(); 
    _lastUpdate = millis();
}

bool Motor::update() {
    unsigned long now = millis();
    if (now - _lastUpdate >= _updateInterval) {
        portENTER_CRITICAL(&_mux);
        long count = _pos;
        _pos = 0; 
        portEXIT_CRITICAL(&_mux);

        float dt = (float)(now - _lastUpdate) / 1000.0f;
        float rawRPM = ((float)count / (float)_cpr) / dt * 60.0f;

        if (abs(rawRPM) < 2000.0f) {
            _currRPM = (_currRPM * 0.7f) + (rawRPM * 0.3f);
        }

        _lastUpdate = now;
        return true;
    }
    return false;
}

float Motor::getCurrRPM() { return _currRPM; }

void Motor::drive(int pwm) {
    int activePWM = pwm;

    activePWM = constrain(activePWM, -255, 255);

    if (activePWM > 0) {
        digitalWrite(_pinIN1, HIGH);
        digitalWrite(_pinIN2, LOW);
    } else if (activePWM < 0) {
        digitalWrite(_pinIN1, LOW);
        digitalWrite(_pinIN2, HIGH);
    } else {
        stop();
        return;
    }

    ledcWrite(_ledcChannel, abs(activePWM));
}

void Motor::stop() {
    digitalWrite(_pinIN1, LOW);
    digitalWrite(_pinIN2, LOW);
    ledcWrite(_ledcChannel, 0);

}