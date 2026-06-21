# IMU 安裝與測試指南 (IMU Installation & Test Guide)

本目錄 (`example/imu_ut/`) 包含用於驗證 I2C 通訊以及 IMU 感測器 (MPU6050 / MPU6500) 基本功能的獨立測試專案。
本文件旨在指導您如何進行 **IMU 的實體安裝、接線以及軟體測試**。

---

## 1. 硬體介紹與安裝指南

### 1.1 實體安裝與車體朝向 (Orientation)
IMU (慣性測量單元) 的安裝位置與朝向對機器人的自適應煞車、里程計融合 (Odometry Fusion) 至關重要。
1. **水平固定**：請確保 IMU 模組**水平牢固地固定在車體底盤或控制板上**。任何微小的傾斜都會導致重力加速度分量分佈到 X/Y 軸，進而影響姿態估算。
2. **安裝位置**：建議安裝在**車體旋轉中心**（兩主動輪連線的中點），這樣在旋轉時能獲得最單純的角速度，避免產生額外的向心加速度。
3. **防震**：建議使用雙面泡棉膠、矽膠墊或尼龍螺絲固定，避免馬達震動直接傳導給 IMU 導致數據雜訊過大。
4. **軸向一致性 (符合 ROS / 右手定則基準)**：
   - **X 軸**：正方向指向車體**正前方**。
   - **Y 軸**：正方向指向車體**正左方**。
   - **Z 軸**：正方向指向車體**正上方**。
   - *注意：若實體安裝方向與上述不符，未來在主程式融合 (Odometry.h) 中必須進行軸向變換或座標系旋轉。*

---

## 2. 硬體接線對照表

ESP32 與 MPU6050 模組之間採用 **I2C 通訊** 以及一個 **GPIO 外部中斷** 進行資料同步。

| MPU6050 腳位 | ESP32 GPIO 腳位 | 功能描述 | 備註 |
| :---: | :---: | :---: | :--- |
| **VCC** | **3.3V** | 電源輸入 | 必須接 3.3V，若接 5V 可能損壞感測器或 ESP32 引腳 |
| **GND** | **GND** | 接地端 | 系統共地 |
| **SCL** | **GPIO 22** | I2C 時鐘線 | ESP32 預設硬體 I2C SCL 腳位 |
| **SDA** | **GPIO 21** | I2C 數據線 | ESP32 預設硬體 I2C SDA 腳位 |
| **INT** | **GPIO 4** | 外部中斷線 | 當 IMU 數據準備好 (Data Ready) 時發出訊號，避免輪詢 (Polling) 佔用 CPU |

> ⚠️ **注意事項**：
> - **INT (中斷腳位)**：程式中定義為 `GPIO 4`。
> - **避免使用 Strapping Pins**：請勿將 INT 接到 GPIO 15、GPIO 12 或 GPIO 2、GPIO 0 等引腳，這些是 ESP32 的啟動選擇引腳 (Strapping Pins)，在開機時若電位被拉高/拉低會導致 ESP32 無法正常開機或進入下載模式。

---

## 3. 軟體環境與依賴庫

本專案使用 **PlatformIO** 進行開發。在 `platformio.ini` 中已為您配置好所需的函式庫，無需手動下載：
```ini
lib_deps =
    adafruit/Adafruit MPU6050 @ ^2.2.4
    adafruit/Adafruit Unified Sensor @ ^1.1.14
    adafruit/Adafruit BusIO @ ^1.14.1
```

---

## 4. 測試步驟

我們提供兩個獨立的測試程式，請按順序執行，以確保硬體與軟體皆正常。

### 步驟 1：I2C 設備掃描測試 (`env:i2c_scan`)
此步驟用於驗證 ESP32 是否能透過 I2C 匯流排偵測到 MPU6050 晶片。

1. **切換環境**：在 PlatformIO 中，選擇環境 `env:i2c_scan`。
2. **編譯與上傳**：將 ESP32 連接至電腦，執行 **Upload**。
3. **開啟 Serial Monitor**：將 Baud Rate 設定為 `115200`。
4. **結果觀察**：
   - 若連線正確，您應該會看到：
     ```text
     Scanning...
     I2C device found at address 0x68  !
     done
     ```
   - 若顯示 `No I2C devices found`，請檢查 VCC/GND 是否接反、SDA/SCL 是否接錯或鬆脫。

### 步驟 2：IMU 數據校正與讀取測試 (`env:mpu6050_test`)
此步驟將進行晶片初始化、靜態零點校正 (Calibration) 並讀取即時數據。

1. **切換環境**：在 PlatformIO 中，選擇環境 `env:mpu6050_test`。
2. **靜置車體**：**在點擊上傳或重啟 ESP32 前，請務必將車體靜止放置在水平地面上。**
3. **編譯與上傳**：執行 **Upload**。
4. **結果觀察**：
   - 開啟 Serial Monitor (`115200`)，您將會看到：
     ```text
     MPU Initialized!
     Interrupt enabled.
     Calibrating Gyro... Please keep the sensor stationary for 1 second.
     Calibration done! Samples collected: 51
     Gyro Offsets [rad/s]: X:-0.0123, Y:0.0456, Z:0.0012
     ```
   - 隨後會以 500ms 的頻率輸出即時數據：
     ```text
     Accel [m/s^2]: X: 0.0210, Y:-0.0543, Z: 9.8012 | Calib Gyro [rad/s]: X: 0.0000, Y: 0.0000, Z: 0.0000 | Temp: 28.50 C
     ```
   - **數值驗證**：
     - **水平靜止狀態下**：`Accel Z` 應接近重力加速度 `9.8 m/s^2`，而 `Accel X` 與 `Y` 應接近 `0`。
     - **角速度校正**：由於扣除了 `Gyro Offsets`（靜態零點），靜止時的 `Calib Gyro` X/Y/Z 三軸數值應非常接近 `0.0000`。
     - **動態測試**：手持車體搖晃，觀察加速度與角速度是否有劇烈且合理的變化。
     - **旋轉方向驗證 (極重要)！**：
       - **車頭向左轉 (逆時針)**：`Calib Gyro` 的 **Z 軸** 應輸出 **正值 (+)**。
       - **車頭向右轉 (順時針)**：`Calib Gyro` 的 **Z 軸** 應輸出 **負值 (-)**。
       - *原理說明*：這符合 ROS 與機器人學採用的**右手定則 (Right-Hand Rule)**。當右手大拇指朝上（Z 軸正上方），其餘四指彎曲的方向即為旋轉的正方向（逆時針/左轉）。若您的測試結果相反，說明 IMU 被上下顛倒安裝或固定方向有誤，需重新檢查，以免影響後續里程計融合算法。

---

## 5. 常見問題排查 (Troubleshooting)

#### Q1: Serial Monitor 出現 `Failed to find MPU chip` 
- **原因 1：實體接線鬆脫**。請重新檢查 SDA、SCL、VCC、GND 連接。
- **原因 2：晶片為 MPU-6500 且未被正常識別**。本測試代碼已內建 MPU-6500 手動喚醒機制 (讀取 WHO_AM_I 暫存器 `0x75`，若為 `0x70` 則手動寫入電源管理暫存器)。若依然失敗，請確認您的晶片型號或 I2C 位址是否為非預設的 `0x69` (若 AD0 腳接高電位，位址會變為 `0x69`)，可在代碼中將 `mpu.begin(0x68)` 改為 `mpu.begin(0x69)`。

#### Q2: 執行 `mpu6050_test` 時，數值完全不動或卡在 `Interrupt enabled.`
- **原因：中斷腳位 (INT) 未正確連接**。
- `mpu6050_test.cpp` 採用了硬體外部中斷（Sensing Data Ready）。如果 MPU6050 的 `INT` 腳位沒有接到 ESP32 的 `GPIO 4`，或者接錯，則 `mpuDataReady` 永遠不會變為 `true`。
- 解決方法：確保實體一條杜邦線將 MPU6050 的 `INT` 腳連接至 ESP32 的 `GPIO 4`。

#### Q3: 為什麼我的陀螺儀角速度一直在飄移 (Drift)？
- **原因：未在靜止狀態下進行開機校正**。
- 陀螺儀存在物理偏置 (Bias)。本程式在 `setup()` 中會收集 1 秒鐘的數據作為零點偏移量並扣除。如果您在 ESP32 開機或重置時晃動了車子，會導致校正值錯誤，進而產生極大的飄移。
- 解決方法：將車子平放靜止，按下 ESP32 的 `EN` (Reset) 按鈕重新開機校正即可。
