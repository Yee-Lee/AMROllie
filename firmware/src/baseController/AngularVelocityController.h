#ifndef ANGULAR_VELOCITY_CONTROLLER_H
#define ANGULAR_VELOCITY_CONTROLLER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

/**
 * @class AngularVelocityController
 * @brief 使用第二層 PID 控制器來閉迴路控制車體的角速度
 *
 * 這個類別實作了一個獨立的 PID 控制器，不依賴於專案中其他的 `PIDController` 或 `IMotor`。
 * 它的目標是根據 IMU 回饋的實際角速度，計算出一個修正量，用於調整輪速差。
 */
class AngularVelocityController {
private:
    // PID 參數
    float kp, ki, kd;
    
    // PID 內部狀態
    float integral;
    float last_error;
    float target_w_last;
    
    // 時間管理
    unsigned long last_time;
    float interval_s; // 計算週期的秒數

    // 狀態旗標
    bool first_run;
    
    // IMU 硬體相關
    Adafruit_MPU6050 mpu;
    float gyroZ_offset;
    float actual_w;
    float last_output;

public:
    /**
     * @brief 建構子
     * @param p P-gain
     * @param i I-gain
     * @param d D-gain
     * @param hz 控制頻率
     */
    AngularVelocityController(float p, float i, float d, int hz = 50) 
        : kp(p), ki(i), kd(d), integral(0), last_error(0), first_run(true),
          gyroZ_offset(0.0f), actual_w(0.0f), last_output(0.0f), target_w_last(0.0f) {
        
        if (hz > 0) {
            interval_s = 1.0f / hz;
        } else {
            interval_s = 0.02f; // 預設 50Hz
        }
        last_time = micros();
    }

    /**
     * @brief 初始化 MPU6050 硬體、設定中斷並進行靜態零點校正
     * @param int_pin 接收中斷訊號的 ESP32 GPIO 腳位
     */
    void initMPU(uint8_t int_pin) {
        Wire.begin();

        // 讀取晶片 ID 以判斷是否為 MPU-6500
        Wire.beginTransmission(0x68);
        Wire.write(0x75);
        Wire.endTransmission(false);
        Wire.requestFrom((uint16_t)0x68, (uint8_t)1);
        uint8_t whoAmI = Wire.read();

        // 初始化 MPU6050，若為 MPU-6500 (0x70) 則套用手動喚醒繞過方案
        if (!mpu.begin(0x68)) {
            if (whoAmI == 0x70) {
                Wire.beginTransmission(0x68);
                Wire.write(0x6B); // PWR_MGMT_1 暫存器
                Wire.write(0x01); 
                Wire.endTransmission();
                delay(10);
            } else {
                Serial.println("Failed to find MPU chip");
                while (1) { delay(10); }
            }
        }
        Serial.println("IMU Initialized!");

        // 配置感測器參數
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
        mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
        mpu.setSampleRateDivisor(19); // 約 50Hz

        // 設定中斷與腳位
        pinMode(int_pin, INPUT_PULLDOWN);
        Wire.beginTransmission(0x68); 
        Wire.write(0x38); 
        Wire.write(0x01); 
        Wire.endTransmission();

        // 靜態零點校正 (利用 Polling 讀取剛才開啟的中斷訊號)
        Serial.println("Calibrating Gyro Z... Please keep the sensor stationary for 1 second.");
        float sumZ = 0;
        int sampleCount = 0;
        unsigned long startTime = millis();
        
        while (millis() - startTime < 1000) {
            if (digitalRead(int_pin)) {
                sensors_event_t a, g, temp;
                mpu.getEvent(&a, &g, &temp);
                sumZ += g.gyro.z;
                sampleCount++;
            }
        }
        if (sampleCount > 0) gyroZ_offset = sumZ / sampleCount;
        Serial.printf("Calibration done! Samples: %d, Gyro Z Offset: %.4f\n", sampleCount, gyroZ_offset);
    }

    /**
     * @brief 重置 PID 內部狀態
     */
    void reset() {
        integral = 0;
        last_error = 0;
        first_run = true;
        last_output = 0.0f;
        target_w_last = 0.0f;
    }

    /**
     * @brief 動態調整 PID 參數
     */
    void setTunings(float p, float i, float d) {
        kp = p;
        ki = i;
        kd = d;
    }

    /**
     * @brief 取得目前校正後的最新角速度
     */
    float getActualW() const {
        return actual_w;
    }

    /**
     * @brief 更新角速度 PID 控制器
     * @param target_w 目標角速度 (rad/s)
     * @param has_new_data 是否有新的硬體中斷數據準備就緒
     * @return float 修正值 (一個無單位的控制量，用於調整輪速)
     */
    float update(float target_w, bool has_new_data) {
        unsigned long now = micros();

        // 1. 安全機制：超過 200ms 沒更新 (IMU 當機或中斷遺失)，強制角速度歸零，防止卡在轉彎狀態無限積分
        if ((now - last_time) > 200000) {
            actual_w = 0.0f;
        }

        if (!has_new_data) {
            // 若無新數據，直接沿用上次的修正量，避免錯誤的微分與時間計算
            return last_output;
        }

        // 讀取最新數據並扣除硬體零點偏移量
        sensors_event_t a, g, temp;
        if (!mpu.getEvent(&a, &g, &temp)) {
            // 若 I2C 讀取失敗 (例如受到馬達突波干擾)，沿用上次輸出，避免產生異常跳變
            return last_output;
        }
        actual_w = g.gyro.z - gyroZ_offset;

        // 2. 靜態死區 (Deadband)：消除 MPU6050 轉彎後常見的零偏殘留與漂移
        if (abs(actual_w) < 0.02f) {
            actual_w = 0.0f;
        }

        float dt = (now - last_time) / 1000000.0f;
        
        // 確保至少有一個有效的時間間隔
        if (dt <= 0) {
            dt = interval_s;
        }
        last_time = now;

        // 計算誤差
        float error = target_w - actual_w;

        // --- 積分項 (作為直線航向鎖定 Heading Lock) ---
        // 當目標角速度為 0 時 (直線行駛)，角速度誤差的積分即為「航向角度誤差」
        if (abs(target_w) < 0.001f) {
            if (abs(target_w_last) >= 0.001f) {
                // 剛結束轉向，重置積分以鎖定當前的新航向
                integral = 0.0f;
            }
            integral += error * dt;
            // 限制最大航向累積誤差 (0.5 弧度約為 28 度)，防止被外力硬轉後過度修正打滑
            integral = constrain(integral, -0.5f, 0.5f);
        } else {
            // 手動轉向時，不累積積分，避免搖桿放開時產生強烈的回彈 (Windup)
            integral = 0.0f;
        }
        target_w_last = target_w;

        // --- 微分項 ---
        // 首次執行時，避免 D 項衝擊
        if (first_run) {
            last_error = error;
            first_run = false;
        }
        float derivative = (error - last_error) / dt;
        last_error = error;
        
        // --- 計算 PID 總輸出 ---
        last_output = (kp * error) + (ki * integral) + (kd * derivative);
        
        return last_output;
    }
};

#endif // ANGULAR_VELOCITY_CONTROLLER_H