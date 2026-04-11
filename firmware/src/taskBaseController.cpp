#include "task.h"
#include "baseController/config.h"
#include "baseController/Motor.h"
#include "baseController/PIDController.h"
#include "baseController/MotionController.h"
#include "baseController/AngularVelocityController.h"
#include "baseController/Odometry.h"
#include "baseController/ReactiveBrake.h"

// --- 硬體與控制組件實體 ---
volatile bool mpuDataReady = false;
void IRAM_ATTR mpuISR() {
    mpuDataReady = true;
}

AngularVelocityController angular_w_controller(IMU_PID_KP, IMU_PID_KI, IMU_PID_KD, IMU_PID_HZ);
float w_correction = 0.0f;

Motor leftMotor(MOTOR_L_IN1, MOTOR_L_IN2, MOTOR_L_PWM, MOTOR_L_ENC_A, MOTOR_L_ENC_B, MOTOR_CPR, false, 5);
Motor rightMotor(MOTOR_R_IN1, MOTOR_R_IN2, MOTOR_R_PWM, MOTOR_R_ENC_A, MOTOR_R_ENC_B, MOTOR_CPR, true, 5);
PIDController leftPID(&leftMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ);
PIDController rightPID(&rightMotor, MOTOR_PID_KP, MOTOR_PID_KI, MOTOR_PID_KD, MOTOR_PID_LIMIT, MOTOR_PID_HZ);

MotionController motionController(&leftPID, &rightPID, WHEEL_BASE, WHEEL_DIAMETER);
ReactiveBrake reactiveBrake(&leftPID, &rightPID);
Odometry odom(WHEEL_DIAMETER, WHEEL_BASE);

float max_rpm_limit = MAX_RPM_NORMAL;

// 引入 Motor.cpp 中的 Debug 變數
extern long global_debug_pulses_L;
extern long global_debug_pulses_R;

// --- 初始化函式 ---
void initBaseControllerHardware() {
    // 初始化 IMU 與中斷
    angular_w_controller.initMPU(MPU_INT_PIN);
    attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), mpuISR, RISING);

    // 初始化硬體
    leftMotor.init();
    rightMotor.init();

    // PID 基礎偏移設定
    leftPID.setOffset(MOTOR_PID_OFFSET);
    rightPID.setOffset(MOTOR_PID_OFFSET);

    // 初始化超音波感測器
    reactiveBrake.init();

    float initial_max_v = (max_rpm_limit * PI * WHEEL_DIAMETER) / 60.0f;
    motionController.setLimits(initial_max_v, 3.0f, 0.5f, 2.0f);

    motionController.begin();
    odom.begin();
}

// --- 2. 核心架構：FreeRTOS 雙核分流 ---

// taskBaseController (Core 0 - 高優先級, 100Hz)
void taskBaseController(void *pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms = 100Hz
    xLastWakeTime = xTaskGetTickCount();
    
    unsigned long last_debug_print = 0;

    while(1) {
        // 1. 安全讀取下行指令
        portENTER_CRITICAL(&mux);
        float target_v = cmd_target_v;
        float target_w = cmd_target_w;
        bool do_reset = flag_reset_odom;
        if (do_reset) flag_reset_odom = false; // 清除旗標
        portEXIT_CRITICAL(&mux);

        // 2. 處理重置請求
        if (do_reset) {
            odom.reset();
            // 註解掉 angular_w_controller 的重置，避免清空內部的歷史姿態 (last_yaw)，
            // 否則會在下個週期產生角速度 (W) 突波並被 odom 積分，導致姿態無法歸零。
        }

        // 3. 執行硬體即時控制
        // 3.1 超音波防撞更新與過濾
        reactiveBrake.updateSensors();
        float safe_v = 0.0f, safe_w = 0.0f;
        reactiveBrake.filter(target_v, target_w, safe_v, safe_w);

        // 3.2 讀取 IMU 姿態並計算角速度補償
        bool has_mpu_data = false;
        if (mpuDataReady) {
            has_mpu_data = true;
            mpuDataReady = false; // 清除中斷旗標
        }
        float w_correction = angular_w_controller.update(safe_w, has_mpu_data);

        // 3.3 運動學解算 (套用緩坡與最高速限制，並設定馬達目標 RPM)
        MotionTelemetry telemetry = motionController.update(safe_v, safe_w, w_correction);

        // 3.4 執行 PID 運算與馬達輸出
        leftPID.update();
        rightPID.update();

        // 3.5 更新里程計 (Odometry)
        odom.update(leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), angular_w_controller.getActualW());

        // 4. 安全寫入上行遙測變數
        portENTER_CRITICAL(&mux);
        odom_x = odom.getX(); odom_y = odom.getY(); odom_theta = odom.getTheta();
        actual_v = odom.getLinearV(); actual_w = angular_w_controller.getActualW();
        sonar_left_dist = reactiveBrake.getLeftDistance(); sonar_right_dist = reactiveBrake.getRightDistance();
        portEXIT_CRITICAL(&mux);

        // 5. 每 500ms 透過 USB Serial 印出 Debug 資訊 (完全不影響接在 Serial2 的 micro-ROS)
        unsigned long now = millis();
        if (now - last_debug_print >= 500) {
            last_debug_print = now;
            Serial.printf("[DEBUG] Pulses L: %ld, R: %ld | Odom X: %.3f, Y: %.3f | V: %.3f | RPM L: %.1f, R: %.1f\n",
                          global_debug_pulses_L, global_debug_pulses_R, odom_x, odom_y, actual_v, leftMotor.getCurrRPM(), rightMotor.getCurrRPM());
        }

        // 精準延遲以確保 100Hz
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}