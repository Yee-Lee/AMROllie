#include <Arduino.h>
#include <WiFi.h>

#include "Motor.h"
#include "MotorSim.h"
#include "JoystickWebUI.h"
#include "PIDController.h"
#include "MotionController.h" 
#include "AngularVelocityController.h"
#include "Odometry.h"
#include "config.h"

/* --- IMU 與角速度 PID --- */
// 宣告 MPU 的 INT 腳位與中斷旗標
#define MPU_INT_PIN 4 
volatile bool mpuDataReady = false;
void IRAM_ATTR mpuISR() {
    mpuDataReady = true;
}

// PID 參數 (p, i, d) 需要根據實際車體反應來調整
// 注意：如果您目前是「把車輪架空」進行測試，請暫時將 I 設為 0 (例如: 0.1, 0.0, 0.01)
// 否則陀螺儀讀不到轉動，I 項會無限累積導致馬達轉速暴走！
AngularVelocityController angular_w_controller(0.1, 0.05, 0.01, 50); 
float w_correction = 0.0f; // 用於儲存角速度 PID 的修正輸出

/* --- 執行機構與 PID 定義 --- */

// 模擬馬達 (用於測試環境)
// MotorSim leftMotor = MotorSim(120.0f, 220.0f, 300.0f, 10); 
// MotorSim rightMotor = MotorSim(100.0f, 180.0f, 240.0f, 10);
// PIDController leftPID(&leftMotor, 2.0, 3.5, 0.001, 50.0, 100); 
// PIDController rightPID(&rightMotor, 2.0, 3.5, 0.001, 50.0, 100); 

// 真實馬達配置 (上機測試時請取消註解，並註解掉模擬馬達部分)
Motor leftMotor = Motor(12, 14, 13, 18, 34, 292, false, 5);
Motor rightMotor = Motor(27, 26, 25, 19, 35, 292, true, 5);
PIDController leftPID(&leftMotor, 1.7, 3.8, 0.005, 50.0, 100); 
PIDController rightPID(&rightMotor, 1.7, 3.8, 0.005, 50.0, 100); 


// 控制組件實體
JoystickWebUI webUI;
MotionController motionController(&leftPID, &rightPID, 0.135f, 0.065f); // 輪距 13.5cm, 輪徑 6.5cm

// 里程計融合實體 (輪徑 6.5cm, 輪距 13.5cm)
Odometry odom(0.065f, 0.135f);

// 全局運動參數
float max_rpm_limit = 80.0f;
bool is_motion_test_mode = false;

unsigned long lastLogTime = 0;

void setup() {
    Serial.begin(115200);
    
    // 初始化並校正 IMU (內部自動處理 MPU-6500 繞過與 50Hz 硬體中斷設定)
    angular_w_controller.initMPU(MPU_INT_PIN);
    attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), mpuISR, RISING);

    // 初始化硬體
    leftMotor.init();
    rightMotor.init();

    // PID 基礎偏移設定
    leftPID.setOffset(90);
    rightPID.setOffset(90);

    // 根據預設 RPM 計算初始最高線速度 (m/s)
    float initial_max_v = (max_rpm_limit * PI * 0.065f) / 60.0f;
    motionController.setLimits(initial_max_v, 3.0f, 0.5f, 2.0f);
    Serial.printf("Initial max speed set to %.1f RPM (%.2f m/s)\n", max_rpm_limit, initial_max_v);

    // WiFi 連線邏輯
    webUI.begin(WIFI_SSID, WIFI_PASSWORD);

    // 啟動組件
    motionController.begin(); // 同步控制器的起始時間戳記
    odom.begin();             // 初始化里程計的時間戳記
}

void loop() {
    // 1. 維護網路通訊 (包含 WebSocket 逾時自動歸零邏輯)
    webUI.cleanup();

    // 檢查前端是否要求重置里程計
    if (webUI.request_odom_reset) {
        odom.reset();
        webUI.request_odom_reset = false;
        Serial.println("--> Odometry Reset to (0, 0, 0)");
    }

    // 檢查是否需要切換運動模式
    if (webUI.motion_test_active != is_motion_test_mode) {
        is_motion_test_mode = webUI.motion_test_active;
        if (is_motion_test_mode) {
            max_rpm_limit = 60.0f;
            Serial.println("--> Motion Test Mode ON: Max speed set to 60 RPM");
        } else {
            max_rpm_limit = 80.0f;
            Serial.println("--> Motion Test Mode OFF: Max speed set to 80 RPM");
        }
        // 重新計算 m/s 為單位的最高速度並更新控制器
        float new_max_v = (max_rpm_limit * PI * 0.065f) / 60.0f;
        motionController.setLimits(new_max_v, 3.0f, 0.5f, 2.0f);
        Serial.printf("    New max linear velocity: %.2f m/s\n", new_max_v);
    }

    // 2. 更新運動控制器
    // 將角速度 PID 的修正量 w_correction 加入運動學逆解中，實作直線與轉向補償
    MotionTelemetry tele = motionController.update(webUI.target_v, webUI.target_w, w_correction);

    // 3. 執行底層馬達的 PID 閉迴圈計算
    leftPID.update();
    rightPID.update();

    // 4. IMU 讀取與角速度 PID 控制
    bool has_new_imu_data = mpuDataReady;
    mpuDataReady = false;
    
    // 永遠保持 PID 與 IMU 更新，以確保硬體中斷被清除、actual_w 隨時為最新真實數值
    float raw_w_correction = angular_w_controller.update(tele.current_w, has_new_imu_data);

    // 5. 靜止狀態防護：如果搖桿歸零且車體已停下，清除 PID 積分避免 I-term 累積 (Windup)
    // 改為同時檢查目標指令為 0，且兩輪的實際轉速也降至極低 (< 2.0 RPM) 才算真正靜止
    if (abs(tele.current_v) < 0.001f && abs(tele.current_w) < 0.001f && 
        abs(leftMotor.getCurrRPM()) < 10.0f && abs(rightMotor.getCurrRPM()) < 10.0f) {
        
        angular_w_controller.reset();
        w_correction = 0.0f;
        leftPID.reset();  
        rightPID.reset();
    } else {
        // 行駛狀態下，才套用限幅保護後的修正量
        w_correction = constrain(raw_w_correction, -1.5f, 1.5f);
    }

    // 6. 更新全局里程計 (利用真實 Encoder 轉速計算位移，IMU 角速度計算旋轉)
    odom.update(leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), angular_w_controller.getActualW());

    // 7. 數據監控輸出 
    if (millis() - lastLogTime > 50) {
        // 加入 Joy 指令與 Odom 監測 (單位 X,Y 為公尺, Theta 為弧度)
        Serial.printf(">Joy(V:%5.2f W:%5.2f) Pose(X:%5.2f Y:%5.2f Th:%5.2f) | L_Act:%5.1f R_Act:%5.1f | Gyro_W:%5.2f Corr:%5.2f\n", 
                      webUI.target_v, webUI.target_w,
                      odom.getX(), odom.getY(), odom.getTheta(), 
                      leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), 
                      angular_w_controller.getActualW(), w_correction);

        // 同步推送到 Web UI (在網頁上顯示)
        webUI.broadcastOdom(odom.getX(), odom.getY(), odom.getTheta());

        lastLogTime = millis();
    }
}