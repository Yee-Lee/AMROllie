#include <Arduino.h>
#include <WiFi.h>

#include "Motor.h"
#include "PIDController.h"
#include "MotionController.h" 
#include "AngularVelocityController.h"
#include "Odometry.h"
#include "config.h"
#include "DataLogger.h"
#include "ReactiveBrake.h"

#include "WebServerManager.h"
#include "web/indexPage.h"
#include "web/statPage.h"
#include "web/JoystickPage.h"

#include <freertos/FreeRTOS.h> // For portMUX_TYPE

// --- IMU 與角速度 PID ---
volatile bool mpuDataReady = false;
void IRAM_ATTR mpuISR() {
    mpuDataReady = true;
}

// 調整 PID 參數以獲得更平滑的航向鎖定 (Heading Lock)
// Kp: 降低比例增益，減少對瞬間誤差的過度反應。
// Ki: 稍微降低積分增益，因為 P 和 D 的協同作用會更有效。
// Kd: 顯著提高微分增益，以抑制震盪 (蛇行)。
AngularVelocityController angular_w_controller(IMU_PID_KP, IMU_PID_KI, IMU_PID_KD, IMU_PID_HZ); 
float w_correction = 0.0f; 

// --- 執行機構與 PID 定義 ---
Motor leftMotor = Motor(MOTOR_L_IN1, MOTOR_L_IN2, MOTOR_L_PWM, MOTOR_L_ENC_A, MOTOR_L_ENC_B, MOTOR_CPR, false, 5);
Motor rightMotor = Motor(MOTOR_R_IN1, MOTOR_R_IN2, MOTOR_R_PWM, MOTOR_R_ENC_A, MOTOR_R_ENC_B, MOTOR_CPR, true, 5);
PIDController leftPID(&leftMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ); 
PIDController rightPID(&rightMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ); 


// 控制組件實體
WebServerManager webServer;
IndexPage indexPage;
JoystickWebUI joystickPage;
MotionController motionController(&leftPID, &rightPID, WHEEL_BASE, WHEEL_DIAMETER);

// 超音波與反應式煞車 (先傳入 PID 但暫不使用其 filter 煞車功能，僅讀取距離)
ReactiveBrake reactiveBrake(&leftPID, &rightPID);

// 里程計融合實體
Odometry odom(WHEEL_DIAMETER, WHEEL_BASE);

// 全局運動參數
float max_rpm_limit = MAX_RPM_NORMAL;
bool is_motion_test_mode = false;

unsigned long lastLogTime = 0;
unsigned long lastStatusTime = 0;
unsigned long lastDataLogTime = 0;
DataLogger dataLogger;
static portMUX_TYPE dataLoggerMux = portMUX_INITIALIZER_UNLOCKED;
StatWebUI statPage(&dataLogger, &dataLoggerMux);

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

    // 初始化超音波感測器
    reactiveBrake.init();

    float initial_max_v = (max_rpm_limit * PI * WHEEL_DIAMETER) / 60.0f;
    motionController.setLimits(initial_max_v, 3.0f, 0.5f, 2.0f);
    Serial.printf("Initial max speed set to %.1f RPM (%.2f m/s)\n", max_rpm_limit, initial_max_v);

    // WiFi 連線與核心 Web Server 初始化
    webServer.begin(WIFI_SSID, WIFI_PASSWORD);

    // 註冊各個 UI 頁面 (Pages)
    indexPage.attachToServer(webServer.getServer());
    joystickPage.attachToServer(webServer.getServer());
    statPage.attachToServer(webServer.getServer());

    // 啟動 Web Server
    webServer.start();

    motionController.begin();
    odom.begin();
}

void loop() {
    // 1. 維護網路通訊 (處理 WebSocket 等連線清理)
    joystickPage.cleanup();

    if (joystickPage.request_odom_reset) {
        odom.reset();
        joystickPage.request_odom_reset = false;
        Serial.println("--> Odometry Reset to (0, 0, 0)");
    }

    if (joystickPage.motion_test_active != is_motion_test_mode) {
        is_motion_test_mode = joystickPage.motion_test_active;
        if (is_motion_test_mode) {
            max_rpm_limit = MAX_RPM_TEST;
            Serial.printf("--> Motion Test Mode ON: Max speed set to %.1f RPM\n", MAX_RPM_TEST);
        } else {
            max_rpm_limit = MAX_RPM_NORMAL;
            Serial.printf("--> Motion Test Mode OFF: Max speed set to %.1f RPM\n", MAX_RPM_NORMAL);
        }
        float new_max_v = (max_rpm_limit * PI * WHEEL_DIAMETER) / 60.0f;
        motionController.setLimits(new_max_v, 3.0f, 0.5f, 2.0f);
        Serial.printf("    New max linear velocity: %.2f m/s\n", new_max_v);
    }

    // 2. 超音波更新與 WebUI 推送
    reactiveBrake.updateSensors();
    joystickPage.updateSonar(reactiveBrake.getLeftDistance(), reactiveBrake.getRightDistance());

    // 3. 反應式煞車邏輯 (Reactive Brake Filter)
    float safe_v = joystickPage.target_v;
    float safe_w = joystickPage.target_w;
    
    // 只有「前進」時才觸發前方超音波防撞 (避免倒車也被煞停)
    if (joystickPage.target_v > 0) {
        reactiveBrake.filter(joystickPage.target_v, joystickPage.target_w, safe_v, safe_w);
    }

    // 4. 運動學解算與馬達 PID 更新 (使用過濾後的安全速度 safe_v, safe_w)
    MotionTelemetry tele = motionController.update(safe_v, safe_w, w_correction);
    leftPID.update();
    rightPID.update();

    // 5. IMU 角速度 PID 控制
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
        // 如果在測試模式 (架空測試)，強制關閉 IMU 補償，避免因為車體沒轉而導致 PID 積分暴衝
        if (is_motion_test_mode) {
            w_correction = 0.0f;
        } else {
            // 平時下修最大修正極限，避免瞬間修正過猛導致車身抖動或暴衝
            w_correction = constrain(raw_w_correction, -0.8f, 0.8f);
        }
    }

    // 6. 更新全局里程計
    odom.update(leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), angular_w_controller.getActualW());
    joystickPage.updateOdom(odom.getX(), odom.getY(), odom.getTheta());


    // 6.5 定期紀錄狀態數據 (每 100ms 紀錄一筆，可涵蓋最近 51.2 秒的軌跡狀態)
    if (millis() - lastDataLogTime > 100) {
        portENTER_CRITICAL(&dataLoggerMux);
        dataLogger.logData(
            joystickPage.target_v, joystickPage.target_w, tele.current_v, tele.current_w,
            odom.getX(), odom.getY(), odom.getTheta(),
            leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), angular_w_controller.getActualW(), w_correction
        );
        portEXIT_CRITICAL(&dataLoggerMux);
        lastDataLogTime = millis();
    }

    // 7. 遙測數據輸出 (50ms)
    if (millis() - lastLogTime > 50) {
        Serial.printf("[%6lu] >Joy(V:%5.2f W:%5.2f) Pose(X:%5.2f Y:%5.2f Th:%5.2f) | L_Act:%5.1f R_Act:%5.1f | Gyro_W:%5.2f Corr:%5.2f\n", 
                      millis(),
                      joystickPage.target_v, joystickPage.target_w,
                      odom.getX(), odom.getY(), odom.getTheta(), 
                      leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), 
                      angular_w_controller.getActualW(), w_correction);
        lastLogTime = millis();
    }


}