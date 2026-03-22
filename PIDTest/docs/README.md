# PIDTest

## 專案目標

本專案是一個互動式的 PID 控制器調校工具。PID 控制是機器人運動控制的核心，其參數 `Kp`, `Ki`, `Kd` 的設定直接影響系統的穩定性、響應速度與準確性。此工具旨在提供一個視覺化的平台，讓開發者可以即時修改 PID 參數，並觀察馬達在一個階躍響應 (Step Response) 測試下的行為，從而找到最佳的參數組合。

系統核心為一顆 ESP32 開發板，它提供一個 WiFi 網頁介面。使用者可以在網頁上輸入 PID 參數與目標轉速 (RPM)，啟動測試後，網頁會即時繪製出「目標轉速」、「實際轉速」與「PID 輸出 (PWM)」三條曲線，讓調校過程更加直觀與高效。

## 系統架構

![](https://i.imgur.com/uR7vspT.png)

1.  **使用者介面 (MotorWebUI)**:
    *   ESP32 作為 Web Server，提供一個包含參數輸入與即時圖表的網頁。
    *   使用者可以設定：
        *   **PID 參數**: P (比例), I (積分), D (微分)
        *   **目標轉速 (Target RPM)**: 作為 PID 控制器的目標值。
    *   網頁上有「啟動/停止」按鈕來控制測試程序。
    *   啟動後，前端會使用圖表庫（例如 Chart.js）即時繪製從後端傳來的時序資料，包含 Target RPM, Current RPM, PWM output。
    *   通訊方式為 WebSocket。

2.  **PID 測試管理器 (PIDTest)**:
    *   從網頁接收使用者設定的 PID 參數與目標 RPM。
    *   初始化一個 `PIDController` 實例。
    *   當測試啟動時，它會將目標 RPM 設定給 PID 控制器，並進入一個高速的閉迴路控制循環。
    *   在循環中，它會：
        1.  從馬達編碼器讀取**目前 RPM**。
        2.  將目標與目前 RPM 輸入 PID 控制器，計算出應輸出的 **PWM** 值。
        3.  使用此 PWM 值驅動馬達。
        4.  將 `(時間, 目標 RPM, 目前 RPM, PWM)` 的數據組透過 WebSocket 傳送給前端網頁。

3.  **PID 控制器 (PIDController)**:
    *   與 MotionPlanner 專案共用的閉迴路速度控制器。它會比較**目標 RPM** 與**目前 RPM**，根據誤差計算出應施加於馬達的驅動力 (PWM)。
    *   具備積分飽和保護 (Anti-windup) 功能。

4.  **馬達介面 (IMotor)**:
    *   與 MotionPlanner 專案共用相同的馬達抽象介面，讓上層的 PID 測試與控制器可以與真實馬達 (`Motor`) 或模擬馬達 (`MotorSim`) 協同工作。

## 核心元件

*   **硬體**: ESP32 開發板
*   **開發環境**: PlatformIO
*   **框架**: Arduino
*   **主要程式庫**:
    *   `ESPAsyncWebServer`: 提供高效能的非同步網頁伺服器功能。
    *   `AsyncTCP`: `ESPAsyncWebServer` 的底層依賴。
    *   `WebSockets`: 用於實現 ESP32 與網頁前端之間的即時雙向通訊。

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
    *   在網頁中輸入您想測試的 P, I, D 參數與目標 RPM，然後點擊 "Start Test"。
    *   觀察圖表中的曲線行為，判斷系統的響應。一個好的響應通常是快速到達目標值且超調量 (Overshoot) 小、無持續震盪 (Oscillation)。
