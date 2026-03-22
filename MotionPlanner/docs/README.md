# MotionPlanner

## 專案目標

本專案為一個用於兩輪差速驅動機器人（類似 Ollie）的運動規劃與控制器。系統核心為一顆 ESP32 開發板，它提供一個基於 WiFi 的網頁介面，讓使用者可以透過虛擬搖桿即時控制機器人的線速度與角速度。控制器會根據使用者的輸入，執行運動學逆解，並透過 PID 閉迴路控制來精準驅動左右兩個馬達，實現平滑且可控的運動。

## 系統架構

![](https://i.imgur.com/8i5a2eS.png)

1.  **使用者介面 (JoystickWebUI)**:
    *   ESP32 作為 Web Server，提供一個包含雙虛擬搖桿的網頁。
    *   網頁前端使用 HTML/CSS/JavaScript 撰寫，透過 WebSocket 與後端 ESP32 進行即時通訊。
    *   左搖桿控制線速度 (v)，右搖桿控制角速度 (w)。

2.  **運動控制器 (MotionController)**:
    *   接收來自網頁的目標線速度 `v` 與角速度 `w`。
    *   對目標速度進行緩坡處理（Slew Rate Limiting），以限制加速度，防止機器人急衝或急停。
    *   執行**運動學逆解 (Inverse Kinematics)**，將整體的 `(v, w)` 轉換為左右兩輪各自的目標轉速 (RPM)。
        *   `v_left = v - (w * wheel_base / 2)`
        *   `v_right = v + (w * wheel_base / 2)`
    *   將計算出的目標 RPM 設定給對應的 PID 控制器。

3.  **PID 控制器 (PIDController)**:
    *   這是一個閉迴路速度控制器。它會比較**目標 RPM** 與從馬達編碼器回饋的**目前 RPM**。
    *   根據兩者之間的誤差 (Error)，透過 PID 演算法（比例、積分、微分）計算出應施加於馬達的驅動力 (PWM)。
    *   此 PID 控制器具備積分飽和保護 (Anti-windup) 與方向翻轉重置等特性，以提高控制穩定性。

4.  **馬達介面 (IMotor)**:
    *   為馬達定義了一個抽象介面，統一了 `drive()`, `getCurrRPM()` 等操作。
    *   這種設計讓上層的 PID 控制器可以與任何實現了此介面的馬達類別協同工作，提高了程式的模組化與可擴充性。
    *   專案中包含了兩種實作：
        *   `Motor`: 用於驅動真實的直流馬達（包含 PWM 控制與編碼器讀取）。
        *   `MotorSim`: 一個模擬馬達，用於在沒有硬體的情況下進行邏輯測試與開發。

## 核心元件

*   **硬體**: ESP32 開發板
*   **開發環境**: PlatformIO
*   **框架**: Arduino
*   **主要程式庫**:
    *   `ESPAsyncWebServer`: 提供高效能的非同步網頁伺服器功能。
    *   `AsyncTCP`: `ESPAsyncWebServer` 的底層依賴。
    *   `ArduinoJson`: 用於處理 JSON 格式的資料（在此專案中未使用，但可能為未來擴充預留）。

## 如何開始

1.  **硬體連接**:
    *   根據 `src/main.cpp` 中定義的腳位，將 ESP32 與您的馬達驅動板、馬達編碼器連接。

2.  **環境設定**:
    *   安裝 Visual Studio Code 與 PlatformIO 擴充功能。
    *   開啟此專案資料夾。

3.  **軟體設定**:
    *   將 `src/config.h.default` 複製一份並重新命名為 `src/config.h`。
    *   在 `src/config.h` 中填入您的 WiFi SSID 與密碼。

4.  **編譯與上傳**:
    *   使用 PlatformIO 的 "Upload" 功能將程式碼上傳至您的 ESP32 開發板。

5.  **操作**:
    *   上傳成功後，打開序列埠監控視窗（Baud Rate: 115200），您將會看到 ESP32 連上 WiFi 後所分配到的 IP 位址。
    *   在同一區域網路下的手機或電腦瀏覽器中，輸入該 IP 位址。
    *   點擊網頁右上角的 "OFF" 按鈕使其變為 "ON"，即可開始透過虛擬搖桿操控。
