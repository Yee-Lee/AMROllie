# Ollie AMR 韌體系統架構與實作指引 (ESP32 Firmware Spec)

## 1. 專案概述 (Project Context)

- **機器人名稱**：Ollie (AMR)
- **硬體架構**：
  - **上位機 (Host)**：Raspberry Pi 3B (負責高階運算、導航、Web UI、Joystick 處理)
  - **下位機 (Firmware)**：ESP32 (負責馬達即時控制、感測器讀取、micro-ROS 通訊)
- **核心哲學**：「上帝的歸上帝，凱撒的歸凱撒」。ESP32 專注於高即時性、高穩定性的底層硬體控制與 ROS 封包轉發；任何耗費資源的高階應用（如 Web Server、資料庫、複雜演算法）皆交由上位機 RPI3B 處理。

## 2. 核心架構設計：FreeRTOS 雙核任務分流 (Dual-Core RTOS Strategy)

為了解決 micro-ROS `rclc_executor_spin_some()` 潛在的阻塞問題，並確保 PID 控制的絕對即時性，ESP32 將放棄傳統 Arduino `setup()`/`loop()` 的單執行緒邏輯，改採 FreeRTOS 雙核心任務綁定架構。

### 2.1 核心分配 (Core Assignment)

#### Core 0 (PRO_CPU) - 即時控制與系統層
- **底層系統**：WiFi / Bluetooth 協議棧 (系統預設中等優先級)。
- **taskBaseController (底層硬體控制任務)**：
  - **優先級**：高 (High Priority)。
  - **頻率**：100Hz (每 10ms 執行一次，使用 `vTaskDelayUntil` 確保精準)。
  - **職責**：執行超音波防撞過濾 (`ReactiveBrake`)、讀取 IMU 姿態與角速度補償 (`AngularVelocityController`)、運動學解算 (`MotionController`)、執行雙輪 PID 控制 (`PIDController`) 以及更新真實里程計 (`Odometry`)。
  - **優勢**：做為獨立的 FreeRTOS Task，允許安全的浮點數運算 (FPU)，且不會受到 Core 1 網路通訊阻塞的影響。(註：已排除使用 Hardware Timer ISR 來執行 PID，以避免 ISR 中的 FPU Core Panic 風險)。

#### Core 1 (APP_CPU) - 通訊與事件層
- **taskROS (通訊代理任務)**：
  - **優先級**：中等 (Medium Priority)。
  - **頻率**：依 Executor 與 Timer 設定 (10Hz 發佈 Odometry 與超音波資料)。
  - **職責**：
    - 運行 `rclc_executor_spin_some(&executor, timeout)` 處理 ROS 2 收發。
    - 執行 timer_callback (10Hz)：讀取 `taskBaseController` 算出的最新位姿資料與感測器距離，打包發布至 `/odom` (含四元數轉換)、`/sonar/left` 與 `/sonar/right`。
    - 執行訂閱回呼 (如 `/cmd_vel`)：接收上位機速度指令，並更新給 taskPID 的目標值。
    - 執行服務回呼 (如 `/reset_odom`)：接收上位機請求，重置底層里程計與 IMU 積分。
- **中斷服務常式 (ISRs)**：
  - **感測器/編碼器中斷**：透過 `attachInterrupt` 註冊 (如 `mpuISR`)。
  - **規範**：必須加上 `IRAM_ATTR`，內容極度輕量 (僅做 ticks++ 等整數運算)，變數需宣告為 `volatile`。

### 2.2 任務發射台 (Task Launcher)

傳統的 `setup()` 僅用於：硬體腳位初始化、micro-ROS 初始化、呼叫 `xTaskCreatePinnedToCore` 建立上述兩個 Task。

建立完畢後，在 `setup()` 尾端呼叫 `vTaskDelete(NULL)` 殺掉預設的 loopTask 釋放記憶體。傳統的 `loop()` 函數保持空白。

## 3. 資料共享與執行緒安全 (Thread Safety)

由於 `taskBaseController` (Core 0) 會頻繁寫入輪速與位姿，而 `taskROS` (Core 1) 會定時讀取這些資料，存在併發讀寫風險 (Race Condition)。

**實作要求**：在讀寫共用資料結構 (如 OdoTracker 的變數或 `target_velocity`) 時，必須使用 FreeRTOS 的 Mutex (互斥鎖) 或 `portMUX_TYPE` (自旋鎖 / Spinlock) 進行保護，確保資料的一致性。

## 4. 系統安全機制 (Safety Mechanisms)

- **緊急停止函數 `stopMotors()`**：
  - **必須實作**：一個全域的 `stopMotors()` 函數。
  - **動作**：將所有馬達控制腳位 (如 `MOTOR_LEFT_PIN`, `MOTOR_RIGHT_PIN`) 設為 LOW，並將所有 PWM 訊號透過 `ledcWrite` (或其他硬體 API) 設為 0。
- **觸發機制**：
  - 可建立一個輕量級的獨立安全任務 (或在 `taskROS` 中快速輪詢)，監聽 Serial 埠。
  - 若收到字元 's' 或 'S'，立即觸發 `stopMotors()` 並鎖定馬達輸出直到系統重置或收到解除指令。

## 5. 目前進度與下一步 (Current Status & Next Steps)

### 已完成 (Completed)
1. **FreeRTOS 雙核架構**：成功將馬達控制 (`taskBaseController`, Core 0, 100Hz) 與通訊代理 (`taskROS`, Core 1, 10Hz) 分離，並使用 `vTaskDelayUntil` 確保精準頻率。
2. **Spinlock 保護**：雙核共用變數 (如 `cmd_target_v`, `odom_x` 等) 已使用 `portMUX_TYPE` 保護，無競態條件風險。
3. **micro-ROS UART 通訊**：目前底層 Transport 綁定至預設 USB `Serial` (UART0)，可直接透過 USB 與 PC 進行快速 Agent 連線測試。若要實際安裝至車體與 RPi 對接，只需將程式碼改為 `Serial2` (TX=17/RX=16) 並透過硬體線路連接即可。
4. **Topic 收發驗證**：
   - 成功發佈 `/odom` (並完成 Yaw 到四元數 Quaternion 轉換) 以及 `/sonar/left`, `/sonar/right`。
   - 成功訂閱 `/cmd_vel`，可正確解析上位機下達的 `Twist` 速度指令。
   - 成功註冊 `/reset_odom` 服務，可隨時重置里程計與 IMU 積分。
5. **安全機制**：實作了基礎的 `stopMotors()` 斷電保護。
6. **硬體實作整合**：完成 `Motor`, `PIDController`, `MotionController`, `Odometry`, `ReactiveBrake` 等底層模組的封裝與整合。

### 下一步實作 (Next Steps)
1. **實車調適與參數最佳化**：使用實際載具微調 `config.h` 中的 `MOTOR_PID_KP/KI/KD` 參數，確保運動與防撞平順。
2. **上位機 (Raspberry Pi) 整合**：開發 ROS 2 導航堆疊 (Nav2) 節點，結合 LiDAR 進行 SLAM 與路徑規劃。

## 6. 專案目錄結構 (Directory Structure)

```text
firmware/
├── platformio.ini               # PlatformIO 專案與環境設定 (含 micro-ROS 依賴)
├── docs/                        # 專案文件 (README, 測試指南)
└── src/
    ├── main.cpp                 # 程式進入點、任務建立
    ├── task.h                   # 跨核心共享變數與 Spinlock 宣告
    ├── taskBaseController.cpp   # Core 0: 100Hz 即時硬體控制邏輯
    ├── taskRos.cpp              # Core 1: micro-ROS 通訊代理節點
    └── baseController/          # 硬體抽象封裝模組 (IMU, PID, 運動學, 超音波等)
        └── config.h             # 腳位與參數設定
```