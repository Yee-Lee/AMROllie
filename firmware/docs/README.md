# Ollie AMR 韌體系統技術指南 (Firmware Architecture & Spec)

本文件詳述 Ollie AMR 機器人底層韌體 (ESP32) 的系統架構、控制演算法與安全機制。本文件可做為開發人員與 AI Agent 完整複刻與維護本專案的技術藍圖。

## 1. 系統架構：FreeRTOS 雙核分流 (System Architecture)

為了解決 micro-ROS `rclc_executor_spin_some()` 潛在的網路阻塞問題，並確保 PID 控制的絕對即時性，本韌體放棄傳統的 `setup()`/`loop()` 單執行緒邏輯，採用 FreeRTOS 雙核心任務綁定架構。

### 核心分配 (Task Assignment)
| 核心 (Core) | 任務名稱 | Stack 大小 | 優先級 | 頻率 (Hz) | 職責 (Responsibility) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Core 0** | `taskBaseController` | 8192 Bytes | `configMAX_PRIORITIES - 1` (高) | 100 Hz | 硬體驅動、PID 控制、運動學逆解、里程計計算、安全感測器讀取。不受網路延遲影響，允許 FPU 運算。 |
| **Core 1** | `taskROS` | 16384 Bytes | `configMAX_PRIORITIES - 2` (中) | 依事件驅動 | micro-ROS 狀態機管理、事件迴圈 (Spin Loop)、遙測資料發布 (20Hz)、LED 燈效控制。 |

*註解：主程式 (`main.cpp`) 的 `setup()` 在建立上述兩個任務後，會呼叫 `vTaskDelete(NULL)` 銷毀預設的 loopTask 以釋放記憶體。*

---

## 2. 硬體配置與外部依賴 (Hardware & Dependencies)

### 2.1 外部函式庫依賴 (PlatformIO)
- **`micro_ros_platformio`**：處理與 RPi 上位機的 ROS 2 通訊。
- **`FastLED`**：驅動 WS2812B 狀態指示燈。

### 2.2 腳位定義與機構參數 (Pinout & Mechanics)
系統所有的硬體腳位配置（馬達、編碼器、IMU、超音波感測器、LED）、通訊介面設定，以及物理機構參數（輪徑、輪距、編碼器解析度 CPR），皆集中管理於單一設定檔中。

**請直接查閱原始碼以獲取最新設定：**
👉 `src/baseController/config.h`

*(注意：程式內部對於編碼器使用 2x Decoding 雙沿觸發，因此實務運算的 CPR 會是設定檔中 `MOTOR_CPR` 的兩倍。)*

---

## 3. 底層控制系統實作細節 (Base Controller Implementation)

### 3.1 馬達驅動與編碼器 (Motor.cpp)
- **硬體 PWM**：使用 ESP32 `ledc` API 硬體加速，頻率設定為 5000Hz，8-bit 解析度 (0-255)。
- **編碼器讀取**：使用 `attachInterrupt` 綁定 A 相腳位，並設定為 `CHANGE` 觸發 (2x Decoding)。在中斷常式 (`isrWrapper`) 中透過 XOR 邏輯 `(stateA == stateB) ^ _isReversed` 判斷正反轉。

### 3.2 速度閉環控制 (PID Control)
- **頻率**：100Hz (使用 `vTaskDelayUntil` 確保週期精準)。
- **演算法**：位置式 PID 並整合前饋控制 (Feed-Forward) 與動態偏移 (Offset)。
- **前饋機制**：輸出公式為 `Final_PWM = PID_Output + (MOTOR_PID_OFFSET) + (targetRPM * 0.9)`。`OFFSET` (預設 105) 用於克服馬達啟動的靜摩擦力。
- **執行緒安全**：所有與 Core 1 共享的指令 (`cmd_target_v`, `cmd_target_w`) 與遙測變數 (`odom_x`, `actual_v` 等) 皆使用 `portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;` (Spinlock) 進行臨界區塊保護。

### 3.3 運動學處理 (Motion Controller)
- **加速度緩坡 (Slew Rate Limiter)**：限制最大線加速度 (1.5 m/s²) 與角加速度 (4.0 rad/s²)，避免馬達瞬間過載與車體搖晃。
- **運動學逆解**：將 `cmd_vel` 的線速度 (v) 與角速度 (w) 轉換為左右輪目標 RPM。
- **IMU 角速度補償**：結合 MPU6050 陀螺儀數據，透過獨立 PID (`AngularVelocityController`) 算出補償量 `w_correction`，加回目標角速度中，修正車體直線行駛時的左右輪物理差異，確保航向精準。並實作 Anti-Windup 機制，於靜止時清除累積誤差。

### 3.4 里程計 (Odometry)
- **計算方法**：結合雙輪編碼器換算的真實線速度與 IMU 讀取的真實航向角速度。
- **積分演算法**：採用 **二階 Runge-Kutta 積分 (中點法)**，相較於普通歐拉積分，在轉彎時擁有更高的軌跡推算精度。計算完畢後強制將角度正規化至 `[-PI, PI]` 範圍。

---

## 4. 安全機制與煞停系統 (Safety & Braking)

### 4.1 指令看門狗 (Command Watchdog)
- **監控對象**：上位機發送的 `/cmd_vel` 頻率。
- **逾時閾值**：**500 ms**。
- **動作**：若超過 500ms 未收到新指令，系統視為通訊異常或上位機當機，自動執行「安全停機」，將目標速度強制歸零。

### 4.2 感測器反應式煞車 (Reactive Brake)
利用兩個前方超音波感測器進行即時避障。使用硬體中斷 (`CHANGE`) 量測 ECHO 脈寬，不阻塞主迴圈。距離過濾分為三個區域：
- **安全區 (> 35cm)**：完全信任上位機指令。
- **警告區 (18cm - 35cm)**：根據距離比例執行 **線性減速 (Soft Stop)**。
- **危險區 (< 18cm)**：觸發 **強制煞停 (Hard Stop)**，立即將目標速度歸零並清除底層 PID 積分，防止馬達持續推擠障礙物。

### 4.3 零秒煞停 (Zero-Second Braking)
- **觸發條件**：當 micro-ROS 狀態進入 `AGENT_LOSING` 或 `AGENT_DISCONNECTED` 時。
- **邏輯**：系統判斷網路已不穩定，為了防止失控，Core 0 底層控制會立刻切斷馬達動力，而非等待 500ms 看門狗觸發。

---

## 5. micro-ROS 通訊與狀態機 (Communication State Machine)

`taskROS` (Core 1) 運行一個四狀態非阻塞狀態機，確保在網路波動時系統仍能自主運作並妥善管理記憶體資源。

### 狀態機流轉邏輯
1.  **WAITING_AGENT**：低頻 (每 3 秒) Ping 測試 Agent。若等待超過 **30 秒** 無回應，自動觸發 `ESP.restart()` 避免系統假死。
2.  **AGENT_CONNECTED**：正常通訊。執行 `rclc_executor_spin_some` 與 20Hz 的定時器 (發布 Odom 與超音波資料)，並進行時間同步 (`rmw_uros_sync_session`)。
3.  **AGENT_LOSING**：當 Ping 失敗時進入此緩衝期 (Debounce)。持續 **5 秒** (每秒 Ping 一次) 嘗試恢復連線，期間觸發 Core 0 的零秒煞停。
4.  **AGENT_DISCONNECTED**：確認斷線。呼叫 `destroy_entities()` 釋放所有 ROS 資源，清除殘留的速度快取，隨後重置計時器並回到 `WAITING_AGENT`。

### Topic 與 Service 定義
- **Publisher**: `/odom` (nav_msgs/Odometry, 包含四元數轉換), `/sonar/left`, `/sonar/right` (sensor_msgs/Range)。
- **Subscriber**: `/cmd_vel` (geometry_msgs/Twist)。
- **Service**: `/reset_odom` (std_srvs/Trigger, 用於重置里程計與累積誤差)。

---

## 6. 使用者介面與診斷 (UI & Diagnostics)

### 6.1 LED 狀態燈號說明 (WS2812B)
LED 狀態邏輯實作於 `updateLED()` 中，採用雙優先級顯示策略，將「系統故障」與「環境警告」解耦：

| 優先級 | 燈號顏色 | 顯示狀態 | 物理意義 |
| :--- | :--- | :--- | :--- |
| **1 (連線)** | **藍色呼吸** | `WAITING_AGENT` | 正在尋找上位機微控制器代理。 |
| **1 (連線)** | **黃色閃爍 (2Hz)**| `AGENT_LOSING` | 網路不穩，進入 5 秒斷線容錯緩衝。 |
| **1 (連線)** | **紅色常亮** | `AGENT_DISCONNECTED`| 斷線資源清理瞬間 (極短暫)。 |
| **2 (感測器)**| **綠色常亮** | `SENSOR_SAFE` | 連線正常，前方路徑安全 (>35cm)。 |
| **2 (感測器)**| **橘色常亮** | `SENSOR_WARNING` | 進入減速區 (18-35cm)，車速受限。 |
| **2 (感測器)**| **紅色急閃 (5Hz)**| `SENSOR_BRAKE` | 進入危險區 (<18cm)，感測器已強制煞停。 |

### 6.2 USB Serial Debug 資訊
- **發送頻率**：2 Hz (每 500ms 輸出一次至 `Serial`)。
- **內容格式**：
  `[DEBUG] CMD(v, w) | Act(v, w) | RPM(L, R) | PWM(L, R) | w_corr | Odom(x, y, th) | Sonar(L, R)`
- **主要用途**：即時監控馬達 PID 追蹤狀況、補償量、里程計漂移與超音波數值。不影響 `Serial2` 的 micro-ROS 傳輸。

---

## 7. 專案目錄結構 (Directory Structure)

```text
firmware/
├── platformio.ini               # PlatformIO 專案與環境設定 (含 micro-ROS 依賴)
├── docs/                        # 技術文件夾
│   └── README.md                # 本技術指南 (設計藍圖)
└── src/
    ├── main.cpp                 # 系統進入點、任務建立、全域變數初始化
    ├── task.h                   # 跨核心共享變數定義、Spinlock、狀態機枚舉
    ├── taskBaseController.cpp   # Core 0: 100Hz 即時硬體控制任務實作
    ├── taskRos.cpp              # Core 1: micro-ROS 狀態機與通訊實作
    └── baseController/          # 核心組件封裝
        ├── config.h             # 腳位、PID、物理機構參數設定
        ├── Motor.cpp/h          # 馬達驅動與編碼器計數
        ├── PIDController.h      # 速度閉環與前饋控制
        ├── MotionController.h   # 加速度緩坡與運動學逆解
        ├── Odometry.h           # 里程計航位推算 (RK2 積分)
        ├── ReactiveBrake.h      # 超音波反應式煞車邏輯
        └── AngularVelocityController.h # IMU 姿態融合與角速度補償
```
