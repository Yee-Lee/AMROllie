#include <Arduino.h>
#include <WiFi.h>

#include "Motor.h"
#include "JoystickWebUI.h"
#include "PIDController.h"
#include "MotionController.h" 
#include "AngularVelocityController.h"
#include "Odometry.h"
#include "config.h"

// --- IMU 與角速度 PID ---
#define MPU_INT_PIN 4 
volatile bool mpuDataReady = false;
void IRAM_ATTR mpuISR() {
    mpuDataReady = true;
}

AngularVelocityController angular_w_controller(0.6, 0.0, 0.02, 50); 
float w_correction = 0.0f; 

// --- 執行機構與 PID 定義 ---
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
unsigned long lastStatusTime = 0;

void setup() {
    Serial.begin(115200);
    
    // 初始化 IMU 與中斷
    angular_w_controller.initMPU(MPU_INT_PIN);
    attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), mpuISR, RISING);

    // 初始化硬體
    leftMotor.init();
    rightMotor.init();

    // PID 基礎偏移設定
    leftPID.setOffset(90);
    rightPID.setOffset(90);

    float initial_max_v = (max_rpm_limit * PI * 0.065f) / 60.0f;
    motionController.setLimits(initial_max_v, 3.0f, 0.5f, 2.0f);
    Serial.printf("Initial max speed set to %.1f RPM (%.2f m/s)\n", max_rpm_limit, initial_max_v);

    // WiFi 連線邏輯
    webUI.begin(WIFI_SSID, WIFI_PASSWORD);

    motionController.begin();
    odom.begin();
}

void loop() {
    // 1. 維護網路通訊
    webUI.cleanup();

    if (webUI.request_odom_reset) {
        odom.reset();
        webUI.request_odom_reset = false;
        Serial.println("--> Odometry Reset to (0, 0, 0)");
    }

    if (webUI.motion_test_active != is_motion_test_mode) {
        is_motion_test_mode = webUI.motion_test_active;
        if (is_motion_test_mode) {
            max_rpm_limit = 60.0f;
            Serial.println("--> Motion Test Mode ON: Max speed set to 60 RPM");
        } else {
            max_rpm_limit = 80.0f;
            Serial.println("--> Motion Test Mode OFF: Max speed set to 80 RPM");
        }
        float new_max_v = (max_rpm_limit * PI * 0.065f) / 60.0f;
        motionController.setLimits(new_max_v, 3.0f, 0.5f, 2.0f);
        Serial.printf("    New max linear velocity: %.2f m/s\n", new_max_v);
    }

    // 2. 運動學解算與更新
    MotionTelemetry tele = motionController.update(webUI.target_v, webUI.target_w, w_correction);

    // 3. 馬達 PID 更新
    leftPID.update();
    rightPID.update();

    // 4. IMU 角速度 PID 控制
    bool has_new_imu_data = mpuDataReady;
    mpuDataReady = false;
    
    float raw_w_correction = angular_w_controller.update(tele.current_w, has_new_imu_data);

    // 5. 靜止防護 (Anti-Windup)
    if (abs(tele.current_v) < 0.001f && abs(tele.current_w) < 0.001f && 
        abs(leftMotor.getCurrRPM()) < 10.0f && abs(rightMotor.getCurrRPM()) < 10.0f) {
        
        angular_w_controller.reset();
        w_correction = 0.0f;
        leftPID.reset();  
        rightPID.reset();
    } else {
        w_correction = constrain(raw_w_correction, -1.5f, 1.5f);
    }

    // 6. 更新全局里程計
    odom.update(leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), angular_w_controller.getActualW());
    webUI.updateOdom(odom.getX(), odom.getY(), odom.getTheta());

    // 7. 遙測數據輸出 (50ms)
    if (millis() - lastLogTime > 50) {
        Serial.printf(">Joy(V:%5.2f W:%5.2f) Pose(X:%5.2f Y:%5.2f Th:%5.2f) | L_Act:%5.1f R_Act:%5.1f | Gyro_W:%5.2f Corr:%5.2f\n", 
                      webUI.target_v, webUI.target_w,
                      odom.getX(), odom.getY(), odom.getTheta(), 
                      leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), 
                      angular_w_controller.getActualW(), w_correction);
        lastLogTime = millis();
    }

    // 8. 系統狀態監控 (2s)
    if (millis() - lastStatusTime > 2000) {
        // Serial.printf("[%lu] [System Status] Free Heap: %u bytes | Active WS Clients: %u\n", millis(), ESP.getFreeHeap(), webUI.getClientCount());
        lastStatusTime = millis();
    }
}