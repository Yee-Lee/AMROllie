/**
 * ============================================================================
 * 檔案: Odometry.h
 * 描述: 里程計 (Odometry) 運算模組，融合輪速編碼器與 IMU 陀螺儀數據，
 *       估測車體在二維空間中的全局座標 (X, Y, Theta)。
 * ============================================================================
 */
#ifndef ODOMETRY_H
#define ODOMETRY_H

#include <Arduino.h>

class Odometry {
private:
    float x;
    float y;
    float theta;
    float linear_v;
    
    float wheel_radius;
    float wheel_base;
    unsigned long last_time;

public:
    // 初始化傳入輪徑 (m) 與 輪距 (m)
    Odometry(float wheel_diameter = 0.065f, float wheel_base_dist = 0.135f) 
        : x(0.0f), y(0.0f), theta(0.0f), linear_v(0.0f),
          wheel_radius(wheel_diameter / 2.0f), wheel_base(wheel_base_dist), 
          last_time(0) {}

    void begin() {
        last_time = micros();
    }

    // 重置里程計 (用於地面測試歸零)
    void reset() {
        x = 0.0f;
        y = 0.0f;
        theta = 0.0f;
        linear_v = 0.0f;
    }

    // 核心更新：傳入左輪 RPM、右輪 RPM 與 IMU 測量的真實角速度 (rad/s)
    void update(float left_rpm, float right_rpm, float gyro_w) {
        unsigned long now = micros();
        float dt = (now - last_time) / 1000000.0f; // 轉換為秒
        last_time = now;

        if (dt <= 0.0f) return;

        // 1. 將 RPM 轉換為輪子真實線速度 (m/s) -> V = RPM * (2 * PI * R) / 60
        float v_left = left_rpm * (TWO_PI * wheel_radius) / 60.0f;
        float v_right = right_rpm * (TWO_PI * wheel_radius) / 60.0f;

        // 2. 底盤當前總線速度 (m/s)
        float v = (v_left + v_right) / 2.0f;
        linear_v = v;

        // 3. 計算位移變化量
        float dS = v * dt;
        float dTheta = gyro_w * dt; // 直接採用 IMU 數據避免打滑帶來的航向誤差

        // 4. 二階 Runge-Kutta 積分 (中點法)，大幅提高轉彎時的位置精度
        float mid_theta = theta + (dTheta / 2.0f);
        x += dS * cos(mid_theta);
        y += dS * sin(mid_theta);
        theta += dTheta;

        // 5. 角度正規化至 [-PI, PI] 範圍
        while (theta > PI) theta -= TWO_PI;
        while (theta < -PI) theta += TWO_PI;
    }

    float getX() const { return x; }
    float getY() const { return y; }
    float getTheta() const { return theta; }
    float getLinearV() const { return linear_v; }
};

#endif