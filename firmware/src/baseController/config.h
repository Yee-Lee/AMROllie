/**
 * ============================================================================
 * 檔案: config.h
 * 描述: 底層控制器硬體配置參數。
 * ============================================================================
 */
#ifndef CONFIG_H
#define CONFIG_H

// ========================================
// 硬體腳位定義 (Pin Definitions)
// ========================================
// IMU (MPU6050)
#define MPU_INT_PIN 4

// 左馬達 (Left Motor)
#define MOTOR_L_IN1 12
#define MOTOR_L_IN2 14
#define MOTOR_L_PWM 13
#define MOTOR_L_ENC_A 18
#define MOTOR_L_ENC_B 34

// 右馬達 (Right Motor)
#define MOTOR_R_IN1 27
#define MOTOR_R_IN2 26
#define MOTOR_R_PWM 25
#define MOTOR_R_ENC_A 19
#define MOTOR_R_ENC_B 35

// 超音波感測器 (Ultrasonic Sensors)
#define SONAR_L_TRIG 32
#define SONAR_L_ECHO 33
#define SONAR_R_TRIG 23
#define SONAR_R_ECHO 5

// ========================================
// 實體機構參數 (Physical Mechanics)
// ========================================
#define WHEEL_DIAMETER 0.065f // 輪徑 (m)
#define WHEEL_BASE     0.135f // 輪距 (m)
#define MOTOR_CPR      292    // 馬達編碼器解析度 (CPR)

// ========================================
// 控制器與 PID 參數 (Control & PID)
// ========================================
// 馬達 PID 參數
#define MOTOR_PID_KP 1.7f
#define MOTOR_PID_KI 3.8f
#define MOTOR_PID_KD 0.005f
#define MOTOR_PID_LIMIT 50.0f
#define MOTOR_PID_HZ 100
#define MOTOR_PID_OFFSET 90.0f

// 角速度 (IMU) PID 參數
#define IMU_PID_KP 0.4f
#define IMU_PID_KI 0.9f
#define IMU_PID_KD 0.2f
#define IMU_PID_HZ 50

// 運動限制 (Motion Limits)
#define MAX_RPM_NORMAL 80.0f

// 反應式煞車距離設定 (Reactive Brake)
#define SONAR_DANGER_ZONE 18.0f  // 危險煞停距離 (cm)
#define SONAR_WARNING_ZONE 35.0f // 警告減速距離 (cm)

#endif // CONFIG_H