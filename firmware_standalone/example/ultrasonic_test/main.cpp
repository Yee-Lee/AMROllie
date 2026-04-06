/**
 * ============================================================================
 * @file main_ultrasonic_test.cpp
 * @brief 專用於測試超音波感測器與 WebUI 即時狀態顯示的主程式
 * ============================================================================
 */
#include <Arduino.h>
#include "ReactiveBrake.h"

// 我們只需測試超音波，直接傳入 nullptr。
// ReactiveBrake 內部的方法已做好 nullptr 的防呆檢查。
ReactiveBrake reactiveBrake(nullptr, nullptr);

unsigned long last_print_time = 0;

void setup() {
    Serial.begin(115200);
    
    // 初始化超音波感測器模組
    reactiveBrake.init();
    Serial.println("Ultrasonic Sensor Serial Test Started! (Dual Sensors)");
}

void loop() {
    // 1. 持續觸發超音波感測器
    reactiveBrake.updateSensors();

    // 2. 每 100ms 在 Serial 打印一次感測器數據
    if (millis() - last_print_time > 100) {
        last_print_time = millis();
        
        float dist_L = reactiveBrake.getLeftDistance();
        float dist_R = reactiveBrake.getRightDistance();
        
        Serial.printf("Sonar Left: %5.1f cm | Sonar Right: %5.1f cm\n", dist_L, dist_R);
    }
}
