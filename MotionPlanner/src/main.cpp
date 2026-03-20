/**
 * ============================================================================
 * 檔案: main.cpp
 * 描述: AMR 搖桿控制主程式
 * 功能: 整合 MotionController 類別，實作加速度緩坡、限速與安全停止
 * ============================================================================
 */
#include <Arduino.h>
#include <WiFi.h>
#include "Motor.h"
#include "MotorSim.h"
#include "JoystickWebUI.h"
#include "PIDController.h"
#include "MotionController.h" 
#include "config.h"

/* --- 執行機構與 PID 定義 --- */

// 模擬馬達 (用於測試環境)
MotorSim leftMotor = MotorSim(120.0f, 220.0f, 300.0f, 10); 
MotorSim rightMotor = MotorSim(100.0f, 180.0f, 240.0f, 10);
PIDController leftPID(&leftMotor, 2.0, 3.5, 0.001, 50.0, 100); 
PIDController rightPID(&rightMotor, 2.0, 3.5, 0.001, 50.0, 100); 

/* 真實馬達配置 (上機測試時請取消註解，並註解掉模擬馬達部分)
Motor leftMotor = Motor(12, 14, 13, 18, 34, 146, false, 10);
Motor rightMotor = Motor(27, 26, 25, 19, 35, 146, true, 10);
PIDController leftPID(&leftMotor, 2.0, 3.0, 0.005, 50.0, 100); 
PIDController rightPID(&rightMotor, 2.0, 3.0, 0.005, 50.0, 100); 
*/

// 控制組件實體
JoystickWebUI webUI;
MotionController motionController(&leftPID, &rightPID, 0.15f, 0.065f); // 輪距 15cm, 輪徑 6.5cm

unsigned long lastLogTime = 0;

void setup() {
    Serial.begin(115200);
    
    // 初始化硬體
    leftMotor.init();
    rightMotor.init();

    // PID 基礎偏移設定
    leftPID.setOffset(110);
    rightPID.setOffset(90);

    // 設定運動限制 (在此處微調加速手感)
    // 參數: max_v(m/s), max_w(rad/s), max_accel_v(m/s^2), max_accel_w(rad/s^2)
    motionController.setLimits(1.2f, 3.0f, 1.0f, 2.0f);

    // WiFi 連線邏輯
    Serial.print("系統正在連線 WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi 已連線！");
    Serial.print("控制介面位址: http://");
    Serial.println(WiFi.localIP());

    // 啟動組件
    webUI.begin();
    motionController.begin(); // 同步控制器的起始時間戳記
}

void loop() {
    // 1. 維護網路通訊 (包含 WebSocket 逾時自動歸零邏輯)
    webUI.update();

    // 2. 更新運動控制器邏輯
    // update() 內部會完成：限速 -> 緩坡平滑 -> 逆解算 RPM -> 更新左、右 PID 的目標值
    MotionTelemetry tele = motionController.update(webUI.target_v, webUI.target_w);

    // 3. 執行 PID 閉迴圈計算
    leftPID.update();
    rightPID.update();

    // 4. 數據監控輸出 (每 50ms 輸出一次，專為 Serial Plotter 設計)
    // 格式化輸出給 Teleplot
    if (millis() - lastLogTime > 50) {
        Serial.printf(">RawV:%.2f|g\n", tele.raw_v);
        Serial.printf(">CurV:%.2f|g\n", tele.current_v);
        Serial.printf(">RawW:%.2f|g\n", tele.raw_w);
        Serial.printf(">CurW:%.2f|g\n", tele.current_w);
        Serial.printf(">RPML:%.1f|g\n", tele.target_rpm_l);
        Serial.printf(">RPMR:%.1f|g\n", tele.target_rpm_r);
        lastLogTime = millis();
    }

    // 5. 序列埠緊急停止監控
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 's' || cmd == 'S') {
            // 直接呼叫控制器停止邏輯並輸出警告
            motionController.stop();
            Serial.println("!!! 緊急停止命令已下達 !!!");
        }
    }
}