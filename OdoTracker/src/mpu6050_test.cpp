/**
 * ============================================================================
 * 檔案: mpu6050_test.cpp
 * 描述: MPU-6050 感測器基礎讀取測試。
 *       初始化 MPU-6050 並在 Serial Monitor 上持續印出加速度計、
 *       陀螺儀和溫度的讀數。
 * 如何使用:
 *   1. PlatformIO: 切換到 'mpu6050_test' 環境 (platformio run -e mpu6050_test -t upload)
 *   2. Arduino IDE: 直接編譯並上傳此檔案。
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// 定義 MPU6050 的 INT 腳位連接到 ESP32 的哪一個 GPIO 
// (避免使用 GPIO 15 等 Strapping Pin，以免干擾 ESP32 開機)
#define MPU_INT_PIN 4 

// 宣告一個 volatile 變數作為中斷旗標
volatile bool mpuDataReady = false;

// 中斷服務常式 (ISR)：當 INT 腳位電位改變時會觸發這個短暫的函式
void IRAM_ATTR mpuISR() {
    mpuDataReady = true;
}

// 儲存陀螺儀的零點偏移量
float gyroX_offset = 0.0f;
float gyroY_offset = 0.0f;
float gyroZ_offset = 0.0f;

void setup(void) {
    Serial.begin(115200);
    while (!Serial) {
        delay(10); // 等待 Serial 連線
    }

    Wire.begin();

    // 讀取晶片 ID 以判斷是否為 MPU-6500
    Wire.beginTransmission(0x68);
    Wire.write(0x75);
    Wire.endTransmission(false);
    Wire.requestFrom(0x68, 1);
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
            while (1) {
                delay(10);
            }
        }
    }
    Serial.println("MPU Initialized!");

    // 配置感測器參數
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
    mpu.setSampleRateDivisor(19); // 設定為約 50Hz

    pinMode(MPU_INT_PIN, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(MPU_INT_PIN), mpuISR, RISING);

    // 開啟 Data Ready Interrupt
    Wire.beginTransmission(0x68); 
    Wire.write(0x38); 
    Wire.write(0x01); 
    Wire.endTransmission();
    
    Serial.println("Interrupt enabled.");

    // --- 靜態零點校正 (Calibration) ---
    Serial.println("Calibrating Gyro... Please keep the sensor stationary for 1 second.");
    float sumX = 0, sumY = 0, sumZ = 0;
    int sampleCount = 0;
    unsigned long startTime = millis();
    
    // 收集 1 秒鐘的數據 (因為頻率是 50Hz，預期會收集到約 50 筆)
    while (millis() - startTime < 1000) {
        if (mpuDataReady) {
            mpuDataReady = false;
            sensors_event_t a, g, temp;
            mpu.getEvent(&a, &g, &temp);
            sumX += g.gyro.x;
            sumY += g.gyro.y;
            sumZ += g.gyro.z;
            sampleCount++;
        }
    }
    
    if (sampleCount > 0) {
        gyroX_offset = sumX / sampleCount;
        gyroY_offset = sumY / sampleCount;
        gyroZ_offset = sumZ / sampleCount;
    }
    Serial.printf("Calibration done! Samples collected: %d\n", sampleCount);
    Serial.printf("Gyro Offsets [rad/s]: X:%.4f, Y:%.4f, Z:%.4f\n\n", gyroX_offset, gyroY_offset, gyroZ_offset);
}

void loop() {
    static unsigned long lastPrintTime = 0;

    if (mpuDataReady) {
        mpuDataReady = false;
        
        // 必須每次中斷都讀取，以清除感測器內部的中斷狀態
        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        // 扣除零點偏移量，獲得校正後的真實角速度
        float calib_gyro_x = g.gyro.x - gyroX_offset;
        float calib_gyro_y = g.gyro.y - gyroY_offset;
        float calib_gyro_z = g.gyro.z - gyroZ_offset;

        // 限制每 500ms 輸出一次
        if (millis() - lastPrintTime >= 500) {
            lastPrintTime = millis();
            
            Serial.printf("Accel [m/s^2]: X:%7.4f, Y:%7.4f, Z:%7.4f | ", a.acceleration.x, a.acceleration.y, a.acceleration.z);
            Serial.printf("Calib Gyro [rad/s]: X:%7.4f, Y:%7.4f, Z:%7.4f | ", calib_gyro_x, calib_gyro_y, calib_gyro_z);
            Serial.printf("Temp: %.2f C\n", temp.temperature);
        }
    }
}