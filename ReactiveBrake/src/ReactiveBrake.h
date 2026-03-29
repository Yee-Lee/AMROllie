#ifndef REACTIVE_BRAKE_H
#define REACTIVE_BRAKE_H

#include <Arduino.h>
#include "PIDController.h"

// --- 硬體接腳定義 ---
#define TRIG_PIN_L 32
#define ECHO_PIN_L 33
#define TRIG_PIN_R 23
#define ECHO_PIN_R 5 // Spec says 52, which is invalid. Using 5.

class ReactiveBrake {
private:
    // 內部全域/靜態狀態 (供 ISR 使用)
    static volatile unsigned long _echo_start_time_L;
    static volatile unsigned long _echo_start_time_R;
    static volatile unsigned long _duration_L;
    static volatile unsigned long _duration_R;

    PIDController* _leftPID;
    PIDController* _rightPID;

    // 演算法參數
    const float DANGER_ZONE = 15.0f;  // cm
    const float WARNING_ZONE = 50.0f; // cm

    // 感測器更新計時
    unsigned long _last_trigger_time = 0;
    bool _is_left_sensor_next = true;

    // 靜態中斷服務常式 (ISR 必須為 static 才能綁定 attachInterrupt)
    static void IRAM_ATTR left_echo_isr();
    static void IRAM_ATTR right_echo_isr();

public:
    ReactiveBrake(PIDController* pid_l, PIDController* pid_r)
        : _leftPID(pid_l), _rightPID(pid_r) {}

    /**
     * @brief 初始化 ReactiveBrake 模組，設定腳位模式與中斷
     */
    void init() {
        pinMode(TRIG_PIN_L, OUTPUT);
        pinMode(ECHO_PIN_L, INPUT_PULLDOWN); 

        pinMode(TRIG_PIN_R, OUTPUT);
        pinMode(ECHO_PIN_R, INPUT_PULLDOWN);

        digitalWrite(TRIG_PIN_L, LOW);
        digitalWrite(TRIG_PIN_R, LOW);

        // 綁定中斷，監聽 ECHO 腳位的電位變化 (RISING and FALLING)
        attachInterrupt(digitalPinToInterrupt(ECHO_PIN_L), left_echo_isr, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ECHO_PIN_R), right_echo_isr, CHANGE);
        
        Serial.println("ReactiveBrake module initialized.");
    }

    /**
     * @brief 非阻塞式地更新感測器，應在主迴圈中以高頻率呼叫
     * 每 50ms 交錯觸發一次左右感測器
     */
    void updateSensors() {
        if (millis() - _last_trigger_time > 50) {
            _last_trigger_time = millis();

            if (_is_left_sensor_next) {
                digitalWrite(TRIG_PIN_L, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_PIN_L, LOW);
            } else {
                digitalWrite(TRIG_PIN_R, HIGH);
                delayMicroseconds(10);
                digitalWrite(TRIG_PIN_R, LOW);
            }
            _is_left_sensor_next = !_is_left_sensor_next;
        }
    }

    // 取得當前距離 (封裝的 Getter)
    float getLeftDistance() const { return _duration_L * 0.01715f; }
    float getRightDistance() const { return _duration_R * 0.01715f; }

    /**
     * @brief 根據感測器距離過濾目標速度
     * @param v_in  輸入的目標線速度
     * @param w_in  輸入的目標角速度
     * @param v_out 過濾後輸出的安全線速度
     * @param w_out 過濾後輸出的安全角速度
     */
    void filter(float v_in, float w_in, float& v_out, float& w_out) {
        float dist_l = getLeftDistance();
        float dist_r = getRightDistance();
        float d_min = min(dist_l, dist_r);

        // 安全區: 距離足夠遠，信任所有指令
        if (d_min > WARNING_ZONE) {
            v_out = v_in;
            w_out = w_in;
            return;
        }

        // 危險區: 強制停止並重置 PID 積分
        if (d_min < DANGER_ZONE) {
            v_out = 0.0f;
            w_out = 0.0f;
            // 重置 PID 積分項，避免解除危險後暴衝
            if (_leftPID) _leftPID->reset();
            if (_rightPID) _rightPID->reset();
            return;
        }

        // 警戒區: 進行線性減速 (Soft Stop)
        // 確保 d_min 介於 DANGER_ZONE 和 WARNING_ZONE 之間
        float k = (d_min - DANGER_ZONE) / (WARNING_ZONE - DANGER_ZONE);
        k = constrain(k, 0.0f, 1.0f); // 確保 k 在 0-1 範圍

        v_out = v_in * k;
        w_out = w_in; // 暫不修改角速度
    }
};

// --- 靜態成員變數初始化 ---
volatile unsigned long ReactiveBrake::_echo_start_time_L = 0;
volatile unsigned long ReactiveBrake::_echo_start_time_R = 0;
// 預設一個極大的微秒值，確保初始距離為安全範圍 (約 999cm / 0.01715)
volatile unsigned long ReactiveBrake::_duration_L = 58250;
volatile unsigned long ReactiveBrake::_duration_R = 58250;

// --- 靜態中斷服務常式 (ISR) 實體 ---
void IRAM_ATTR ReactiveBrake::left_echo_isr() {
    uint32_t now = micros();
    if (GPIO.in1.val & (1UL << (ECHO_PIN_L - 32))) { 
        _echo_start_time_L = now;
    } else { 
        // 確保真的有先觸發過上升沿才計算
        if (_echo_start_time_L != 0) {
            unsigned long duration = now - _echo_start_time_L;
            if (duration < 40000) { // 限制最大合理時間為 40ms (約 680cm)
                _duration_L = duration;
            } else {
                _duration_L = 58250; // 超時或雜訊，重置為預設安全極大值 (999cm)
            }
            _echo_start_time_L = 0; // 重置狀態，避免下一個雜訊配對到舊時間
        }
    }
}

void IRAM_ATTR ReactiveBrake::right_echo_isr() {
    uint32_t now = micros();
    if (GPIO.in & (1UL << ECHO_PIN_R)) { 
        _echo_start_time_R = now;
    } else { 
        if (_echo_start_time_R != 0) {
            unsigned long duration = now - _echo_start_time_R;
            if (duration < 40000) {
                _duration_R = duration;
            } else {
                _duration_R = 58250;
            }
            _echo_start_time_R = 0;
        }
    }
}

#endif // REACTIVE_BRAKE_H
