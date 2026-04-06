# IMU Unit Test (Example)

本目錄 (`example/imu_ut/`) 是一個獨立的測試專案，用於驗證 I2C 通訊以及 IMU (MPU6050) 感測器的基本功能。
符合 AMROllie 韌體「將測試代碼與主程式分離」的架構原則。

## 專案目標

在組裝或除錯 AMROllie 機器人時，確認 IMU 感測器是否正常運作以及 I2C 匯流排連線是否正確是非常重要的第一步。本專案提供兩個獨立的小型測試程式：

1. **I2C Scanner (`i2cScanner.cpp`)**: 
   - 掃描 ESP32 的 I2C 匯流排，列出所有成功回應的裝置位址。
   - 用於檢查 MPU6050 (通常為 `0x68`) 或其他 I2C 設備是否被正確識別，排除硬體接線或短路問題。

2. **MPU6050 Test (`mpu6050_test.cpp`)**:
   - 針對 MPU6050 進行初始化與基礎數據讀取測試。
   - 定期透過 Serial 輸出加速度 (Accel)、角速度 (Gyro) 以及溫度 (Temp) 數據。
   - 幫助驗證感測器本身是否健康，以及數據是否能正常解析。

## 系統架構

這些測試不需要啟動 WiFi 或 Web Server，完全依賴 Serial Monitor (序列埠監控視窗) 進行文字輸出，是最輕量級的底層硬體驗證。

## 如何開始

1. **硬體連接**:
   - 確保 MPU6050 感測器的 VCC/GND 正常供電。
   - 確認 I2C 的 SDA 與 SCL 腳位正確連接至 ESP32 (預設 SDA=21, SCL=22，或依據您硬體的實際接線)。

2. **進行 I2C 掃描測試 (`env:i2c_scan`)**:
   - 在 PlatformIO 中，選擇編譯環境為 `env:i2c_scan`。
   - 編譯並上傳至 ESP32。
   - 打開 Serial Monitor (Baud Rate 通常設為 115200)，觀察是否有掃描到裝置位址 (如 `0x68`)。

3. **進行 IMU 數據測試 (`env:mpu6050_test`)**:
   - 在 PlatformIO 中，選擇編譯環境為 `env:mpu6050_test`。
   - 編譯並上傳至 ESP32。
   - 打開 Serial Monitor (Baud Rate 115200)，靜置車體，觀察輸出的 X/Y/Z 軸加速度與角速度是否穩定，並在搖晃車體時確認數值是否有相應變化。

> **注意**: 如果在 `mpu6050_test` 中出現 `Failed to find MPU6050 chip` 錯誤，請先退回執行 `env:i2c_scan` 確認接線與位址。