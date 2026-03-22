# OdoTracker (AMR Ollie)

OdoTracker 是一個基於 ESP32 的雙輪差速自主移動機器人 (AMR) 底盤控制系統。本專案致力於實現高精度的閉迴路運動控制，結合了底層的輪速 PID 控制與高層的 IMU 航向角速度 PID 修正，並提供一個具備安全機制的低延遲 WebSocket 網頁搖桿介面。

## 🎯 專案目標

1. **精準的底盤運動學控制**：透過正逆運動學解算，將期望的線速度 ($V$) 與角速度 ($W$) 轉換為左右輪的目標轉速 (RPM)。
2. **雙層閉迴路 PID 架構 (Dual-Loop PID)**：
   - **內迴圈 (Inner Loop)**：讀取光電/霍爾編碼器，控制實體馬達的 RPM 穩定度。
   - **外迴圈 (Outer Loop)**：讀取 MPU6050 陀螺儀 (Z軸角速度)，自動修正因地面摩擦力不均或馬達硬體差異造成的偏航，確保直線行駛與精準轉向。
3. **平滑與安全性 (Safety & Smoothness)**：
   - 實作緩坡限制 (Slew Rate Limiter) 防止加減速過猛導致翻車或馬達過載。
   - 具備通訊看門狗 (Watchdog) 機制，當控制端斷線時車體能自動安全煞停。
4. **高度模組化 (Modular Design)**：將硬體驅動、PID 演算法、運動學解算與通訊介面徹底解耦，便於未來擴充 (例如加入 ROS 2 micro-ROS 節點)。

---

## 🛠️ 硬體架構

- **微控制器**：ESP32 Development Board
- **慣性感測器 (IMU)**：MPU-6050 (或相容的 MPU-6500，系統已內建硬體 Bypass 辨識機制)
- **致動器**：帶有正交編碼器 (Quadrature Encoder) 的直流減速馬達 x2
- **馬達驅動器**：支援 IN1, IN2, PWM 控制邏輯的驅動板 (如 L298N, TB6612 或 MOS 驅動)

### 腳位配置 (Pinout)
| 元件 / 功能 | 左輪 (Left Motor) | 右輪 (Right Motor) | IMU (MPU6050) |
| :--- | :--- | :--- | :--- |
| **IN1 (前進)** | GPIO 12 | GPIO 27 | - |
| **IN2 (後退)** | GPIO 14 | GPIO 26 | - |
| **PWM (調速)** | GPIO 13 | GPIO 25 | - |
| **Encoder A**| GPIO 18 | GPIO 19 | - |
| **Encoder B**| GPIO 34 | GPIO 35 | - |
| **SDA (I2C)** | - | - | GPIO 21 (預設) |
| **SCL (I2C)** | - | - | GPIO 22 (預設) |
| **INT (中斷)** | - | - | GPIO 4 |

> **注意**：IMU 的中斷腳位設定為 GPIO 4，刻意避開了 ESP32 的 Strapping Pins (如 GPIO 15)，以防止開機異常。

---

## 🧠 軟體架構與實作細節

### 1. 馬達與編碼器控制 (`Motor.cpp` / `PIDController.h`)
- 使用 ESP32 的 **LEDC 硬體計時器**產生平順的 5kHz PWM 訊號。
- 編碼器採用 **雙沿觸發 (CHANGE)** 中斷，將解析度提升兩倍。內部具備一階低通濾波器來平滑 RPM 讀數。
- 底層 `PIDController` 實作了 **積分限幅 (Anti-windup)**、過零點重置與**前饋控制 (Feed-forward offsetPWM)**，用以克服馬達的靜摩擦力死區 (Deadband)。

### 2. 運動控制器 (`MotionController.h`)
- 接收來自控制端的目標 $V$ (m/s) 與 $W$ (rad/s)。
- 執行**加速度限制 (Apply Ramp)**，根據 `dt` 計算每一幀允許的最大速度變化，實現平滑起步與煞車。
- 利用差速運動學公式計算左右輪目標 RPM：
  $$V_{left} = V - (W \times \frac{WheelBase}{2}) - W_{correction}$$
  $$V_{right} = V + (W \times \frac{WheelBase}{2}) + W_{correction}$$

### 3. IMU 與角速度外迴圈 (`AngularVelocityController.h`)
- 採用 **硬體中斷驅動 (Data Ready Interrupt)**，強制感測器以精準的 50Hz (Sample Rate Divisor = 19) 觸發 ESP32 讀取資料，確保 PID 的微分項 ($dt$) 極度穩定。
- **靜態零點校正 (Static Calibration)**：系統開機的前 1 秒會收集 50 筆靜止數據計算陀螺儀的系統誤差 (Zero-rate Bias)，並在執行期間扣除，確保角速度在靜止時完美歸零。
- 若檢測到市面常見的 MPU-6500 偽裝晶片 (WHO_AM_I = 0x70)，會自動發送硬體喚醒指令繞過函式庫驗證，提升硬體相容性。

### 4. 網頁搖桿介面 (`JoystickWebUI.h`)
- 基於 `ESPAsyncWebServer` 實作。
- 前端網頁包含虛擬雙搖桿與即時遙測儀表板。
- 具備 **Keep-Alive (心跳包) 機制**：前端每 200ms 發送一次狀態。
- 具備 **Watchdog (看門狗) 機制**：後端若超過 1000ms 未收到訊號 (例如手機鎖屏或斷網)，將強制把目標速度歸零並煞停。變數採用 `volatile` 宣告以確保多核心非同步處理下的記憶體一致性。

---

## 💻 環境設定與開發指南

### 1. 開發環境
- 安裝 Visual Studio Code。
- 安裝 **PlatformIO IDE** 擴充套件。

### 2. 依賴函式庫 (於 `platformio.ini` 中設定)
- `mathieucarbou/ESPAsyncWebServer` (非同步網頁伺服器)
- `adafruit/Adafruit MPU6050` & `adafruit/Adafruit Unified Sensor` (IMU 讀取)
- `bblanchon/ArduinoJson` (JSON 處理與通訊擴充)

### 3. 初始設定
1. 在 `src/` 目錄下建立 `config.h` 檔案：
   ```cpp
   #ifndef CONFIG_H
   #define CONFIG_H
   const char* WIFI_SSID = "YOUR_WIFI_SSID";
   const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
   #endif
   ```
2. 透過 USB 連接 ESP32，在 PlatformIO 點擊 **Upload and Monitor**。

### 4. 系統啟動流程與測試
1. **保持車體靜止**：通電後的前 2 秒鐘，系統正在進行 MPU6050 的靜態零點校正，請勿晃動車體。
2. **觀察 Serial Monitor**：等待印出 `Calibration done!` 以及 `WiFi Connected: 192.168.x.x`。
3. **連線操控**：在手機或電腦瀏覽器輸入 ESP32 的 IP 地址，點擊網頁右上角的 **OFF / ON** 按鈕即可開始推動搖桿操控。


