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
    // 提升系統允許的最大加速度，讓起步更為迅速 (線加速度設為 1.5 m/s^2，角加速度設為 4.0 rad/s^2)
    motionController.setLimits(initial_max_v, 3.0f, 1.5f, 4.0f);
    Serial.printf("Initial max speed set to %.1f RPM (%.2f m/s)\n", max_rpm_limit, initial_max_v);

    motionController.begin();
    odom.begin();
}

// --- 2. 核心架構：FreeRTOS 雙核分流 ---

// taskBaseController (Core 0 - 高優先級, 100Hz)
void taskBaseController(void *pvParameters) {
    // 確保硬體中斷 (Encoder, IMU) 註冊在 Core 0，避免被 Core 1 的 ROS UART 通訊干擾導致漏中斷
    initBaseControllerHardware();

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 10ms = 100Hz
    xLastWakeTime = xTaskGetTickCount();
    
    unsigned long last_debug_print = 0;
    float last_target_w = 0.0f; // 用於儲存上一個週期的平滑化目標角速度

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
        float safe_v = target_v;
        float safe_w = target_w;

        // 只有「前進」時才觸發前方超音波防撞 (避免倒車也被煞停，並防止死鎖)
        if (target_v > 0.0f) {
            reactiveBrake.filter(target_v, target_w, safe_v, safe_w);
        }

        // 3.2 讀取 IMU 姿態並計算角速度補償
        bool has_mpu_data = false;
        // 加入 digitalRead 防呆：若受馬達雜訊干擾漏掉 RISING 邊緣，避免中斷腳位永遠卡死 HIGH
        if (mpuDataReady || digitalRead(MPU_INT_PIN) == HIGH) {
            has_mpu_data = true;
            mpuDataReady = false; // 清除中斷旗標
        }
        
        // 3.2 讀取 IMU 姿態並計算角速度補償 (改用平滑化後的當前預期角速度)
        float raw_w_correction = angular_w_controller.update(last_target_w, has_mpu_data);

        // 3.3 靜止防護 (Anti-Windup) 與限幅
        float rpm_l = leftMotor.getCurrRPM();
        float rpm_r = rightMotor.getCurrRPM();

        if (abs(safe_v) < 0.001f && abs(safe_w) < 0.001f && 
            abs(rpm_l) < 3.0f && abs(rpm_r) < 3.0f) {
            // 車體準備靜止時，徹底清除累積誤差，防止馬達在停止時發出異音或亂抖
            angular_w_controller.reset();
            w_correction = 0.0f;
            leftPID.reset();
            rightPID.reset();
        } else {
            // 行進中，將補償量限制在安全範圍內 (-1.5 ~ +1.5 rad/s，因為左右輪物理差異較大，需放寬極限)
            w_correction = constrain(raw_w_correction, -1.5f, 1.5f);
        }

        // 3.4 運動學解算 (套用緩坡與最高速限制，並設定馬達目標 RPM)
        MotionTelemetry telemetry = motionController.update(safe_v, safe_w, w_correction);
        last_target_w = telemetry.current_w; // 紀錄本次平滑化後的目標角速度，供下個週期 IMU 比對使用

        // 3.5 執行 PID 運算與馬達輸出
        leftPID.update();
        rightPID.update();

        // 取得實際角速度，供 Odometry 與 ROS 使用
        float correct_actual_w = angular_w_controller.getActualW();

        // 3.6 更新里程計 (Odometry)
        odom.update(leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), correct_actual_w);

        // 4. 安全寫入上行遙測變數
        portENTER_CRITICAL(&mux);
        odom_x = odom.getX(); odom_y = odom.getY(); odom_theta = odom.getTheta();
        actual_v = odom.getLinearV(); actual_w = correct_actual_w;
        sonar_left_dist = reactiveBrake.getLeftDistance(); sonar_right_dist = reactiveBrake.getRightDistance();
        portEXIT_CRITICAL(&mux);

        // 5. 每 500ms 透過 USB Serial 印出 Debug 資訊 (完全不影響接在 Serial2 的 micro-ROS)
        unsigned long now = millis();
        if (now - last_debug_print >= 500) {
            last_debug_print = now;
            Serial.printf("[DEBUG] CMD(v:%.3f, w:%.3f) | Act(v:%.3f, w:%.3f) | RPM(L:%4.1f, R:%4.1f) | PWM(L:%4d, R:%4d) | w_corr:%.3f | Odom(x:%.3f, y:%.3f, th:%.3f)\n",
                          target_v, target_w, actual_v, actual_w,
                          leftMotor.getCurrRPM(), rightMotor.getCurrRPM(), leftPID.getLastPWM(), rightPID.getLastPWM(), w_correction, odom_x, odom_y, odom_theta);
        }

        // 精準延遲以確保 100Hz
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}