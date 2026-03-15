# 專案實作文件 (PIDTest)

## 1. 專案目標
本專案旨在為 ESP32 平台開發一套直流馬達 (DC Motor) 的 PID 轉速控制系統。系統包含硬體驅動層、PID 控制算法核心以及自動化測試工具，用於調校 PID 參數並驗證馬達的步階響應 (Step Response) 性能。

## 2. 系統架構
系統採用物件導向設計，主要模組如下：

*   **Motor / MotorSim**: 硬體抽象層 (HAL)，負責讀取編碼器回授 (RPM) 與輸出 PWM。
*   **PIDController**: 控制算法核心，計算誤差並輸出控制量。
*   **PIDTest**: 測試管理器，產生測試訊號 (如階波) 並記錄數據。
*   **Main**: 整合各模組，執行控制迴圈。

## 3. 模組詳細說明

### 3.1 PID 控制器 (PIDController)
*   **功能**: 實現標準 PID 算法，將目標轉速 (Target RPM) 與實際轉速 (Current RPM) 的誤差轉換為 PWM 輸出。
*   **特點**:
    *   **頻率控制**: 內建計時器，確保計算週期 (如 10ms/100Hz) 穩定，不受 loop 執行速度影響。
    *   **積分抗飽和 (Anti-windup)**: 使用 Clamping 方法限制積分項範圍，避免因長時間誤差累積導致系統失控。同時在目標歸零或方向改變時重置積分。
    *   **防微分衝擊 (Derivative Kick)**: 在目標值劇烈變化或首次運行時，重置微分項計算，減少輸出突波。
    *   **除錯功能**: 支援輸出 P, I, D 各項分量與最終 PWM 值。

### 3.2 馬達驅動 (Motor / MotorSim)
*   **Motor (實體)**:
    *   **PWM 生成**: 使用 ESP32 的 `ledc` 函式庫產生高頻 PWM 訊號，並透過 IN1/IN2 控制方向。
    *   **編碼器讀取**: 利用硬體中斷 (ISR) 讀取 A/B 相訊號，計算位置計數 (Ticks)。
    *   **轉速計算**: 基於時間差與計數差計算 RPM，並套用低通濾波 (Exponential Moving Average, alpha=0.3) 平滑數值。
*   **MotorSim (模擬)**:
    *   模擬馬達物理特性，包含死區 (Dead zone)、飽和區 (Saturation) 與慣性 (一階滯後系統, Tau=0.1s)。
    *   用於在無硬體連接時驗證 PID 邏輯與測試框架。

### 3.3 自動化測試 (PIDTest)
*   **功能**: 自動產生測試序列，協助使用者觀察系統響應。
*   **測試模式**:
    *   **Step Response**: 在設定的時間間隔 (`intervalMs`) 內，將目標轉速從 `startRpm` 逐步增加到 `endRpm`。
*   **數據輸出**: 透過 Serial 印出 `[Time] Target, Current` 格式數據，可配合 Serial Plotter 繪製響應曲線。

## 4. 軟體流程 (Main Loop)
1.  **初始化**: 設定馬達接腳、PID 參數 (Kp, Ki, Kd) 與測試條件。
2.  **狀態機 (StateMachine)**:
    *   **IDLE**: 等待指令或測試結束，PID 重置。
    *   **RUNNING**: 執行 PIDTest 邏輯，更新目標轉速。
3.  **控制迴圈**:
    *   呼叫 `motor.update()` 計算當前轉速。
    *   呼叫 `pid.update()` 計算 PWM 輸出並驅動馬達。
    *   若處於測試狀態，記錄遙測數據。

## 5. 硬體配置 (參考)
*   **MCU**: ESP32
*   **馬達驅動**: H-Bridge (如 L298N, TB6612)。
*   **編碼器**: 增量式編碼器 (A/B 相)。