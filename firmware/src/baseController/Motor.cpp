/**
 * ============================================================================
 * 檔案: Motor.cpp
 * 描述: 實體馬達驅動實作，採用 ESP32 LEDC (PWM) 硬體加速。
 * 修正: 完善負數 PWM 處理邏輯與方向控制。
 * ============================================================================
 */
#include "Motor.h"

static uint8_t next_ledc_channel = 0; 

// 用於 Debug 的全域累加脈衝計數器
long global_debug_pulses_L = 0;
long global_debug_pulses_R = 0;

Motor::Motor(int in1, int in2, int pwm, int encA, int encB, int cpr, bool reverse, int interval) 
    : _pinIN1(in1), _pinIN2(in2), _pinPWM(pwm),
      _pinEncA(encA), _pinEncB(encB),
      _cpr(cpr), _isReversed(reverse), _updateInterval(interval),
      _pos(0), _currRPM(0), _lastUpdate(0) {

    _ledcChannel = next_ledc_channel++;
}

/**
 * 讀取 A, B 相判斷方向 (雙沿觸發 2x Decoding)
 * 邏輯：
 * 當 A 變動時，若 A == B 則為一向，A != B 則為另一向。
 * 結合 _isReversed 修正物理安裝方向。
 */
void IRAM_ATTR Motor::isrWrapper(void* arg) {
    Motor* instance = (Motor*)arg;
    bool stateA = digitalRead(instance->_pinEncA);
    bool stateB = digitalRead(instance->_pinEncB);

    // 判斷旋轉方向，並結合 _isReversed 修正物理安裝方向 (使用 XOR 邏輯)
    bool forward = (stateA == stateB) ^ instance->_isReversed;
    
    instance->_pos += forward ? 1 : -1;
}

void Motor::init() {
    pinMode(_pinIN1, OUTPUT);
    pinMode(_pinIN2, OUTPUT);
    
    // 設定 ESP32 PWM (LEDC)
    ledcSetup(_ledcChannel, 5000, 8); // 5kHz, 8-bit 分辨率
    ledcAttachPin(_pinPWM, _ledcChannel);
    
    pinMode(_pinEncA, INPUT_PULLUP);
    pinMode(_pinEncB, INPUT_PULLUP);
    
    // 設定 A 相為雙沿觸發 (CHANGE)，以獲得 2 倍解析度
    attachInterruptArg(digitalPinToInterrupt(_pinEncA), isrWrapper, this, CHANGE);

    stop(); 
    _lastUpdate = millis();
}

bool Motor::update() {
    unsigned long now = millis();
    if (now - _lastUpdate >= _updateInterval) {
        // 使用原子操作讀取並清除計數值
        portENTER_CRITICAL(&_mux);
        long count = _pos;
        _pos = 0; 
        portEXIT_CRITICAL(&_mux);
        
        // 攔截並累加原始脈衝 (Channel 0 為左輪，1 為右輪)
        if (_ledcChannel == 0) global_debug_pulses_L += count;
        if (_ledcChannel == 1) global_debug_pulses_R += count;

        float dt = (float)(now - _lastUpdate) / 1000.0f;
        // RPM 計算公式: (脈衝數 / 每圈脈衝數) / 時間(分)
        // 注意：因為是雙沿觸發，_cpr 應該定義為原本單沿 PPR * 2
        float rawRPM = ((float)count / (float)_cpr) / dt * 60.0f;

        // 簡單的一階低通濾波，平滑轉速數據並過濾極端跳變
        if (abs(rawRPM) < 2000.0f) {
            _currRPM = (_currRPM * 0.7f) + (rawRPM * 0.3f);
        }

        _lastUpdate = now;
        return true;
    }
    return false;
}

float Motor::getCurrRPM() { return _currRPM; }

/**
 * 驅動馬達
 * @param pwm 數值範圍 -255 到 255 (正數前進，負數後退)
 */
void Motor::drive(int pwm) {
    // 限制範圍
    int activePWM = constrain(pwm, -255, 255);

    if (activePWM > 0) {
        // 正轉：IN1 高, IN2 低
        digitalWrite(_pinIN1, HIGH);
        digitalWrite(_pinIN2, LOW);
        ledcWrite(_ledcChannel, activePWM);
    } 
    else if (activePWM < 0) {
        // 反轉：IN1 低, IN2 高
        digitalWrite(_pinIN1, LOW);
        digitalWrite(_pinIN2, HIGH);
        // PWM 控制必須輸入正值絕對值
        ledcWrite(_ledcChannel, abs(activePWM));
    } 
    else {
        // 輸出為 0，進入停止/剎車狀態
        stop();
    }
}

/**
 * 立即停止馬達輸出
 */
void Motor::stop() {
    digitalWrite(_pinIN1, LOW);
    digitalWrite(_pinIN2, LOW);
    ledcWrite(_ledcChannel, 0);
}