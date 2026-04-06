# PIDTest

## 專案目標

本專案是一個互動式的 PID 控制器調校工具。PID 控制是機器人運動控制的核心，其參數 `Kp`, `Ki`, `Kd` 的設定直接影響系統的穩定性、響應速度與準確性。此工具旨在提供一個視覺化的平台，讓開發者可以即時修改 PID 參數，並觀察馬達在一個階躍響應 (Step Response) 測試下的行為，從而找到最佳的參數組合。

系統核心為一顆 ESP32 開發板，它提供一個 WiFi 網頁介面。使用者可以在網頁上輸入 PID 參數與目標轉速 (RPM)，啟動測試後，網頁會即時繪製出「目標轉速」、「實際轉速」與「PID 輸出 (PWM)」三條曲線，讓調校過程更加直觀與高效。

## 系統架構

![](https://i.imgur.com/uR7vspT.png)

1.  **使用者介面 (WebServerManager & PIDTestPage)**:
    *   ESP32 作為 Web Server，採用主專案的 `WebServerManager` 架構，提供一個包含參數輸入與即時圖表的網頁 (`/pid`)。
    *   使用者可以設定：
        *   **PID 參數**: 左/右馬達各自的 P (比例), I (積分), D (微分)。
        *   **測試配置 (Test Profile)**: 起始轉速、結束轉速、階躍轉速 (Step RPM)、間隔時間與持續時間。
    *   網頁上有「START/STOP」按鈕來控制測試程序。
    *   啟動後，前端會即時繪製從後端透過 WebSocket 傳來的時序資料，包含 Target RPM, Left RPM, Right RPM 曲線。

2.  **PID 測試驅動主程式 (main.cpp)**:
    *   測試環境的進入點，從網頁接收使用者設定的 PID 參數與測試配置。
    *   套用參數至主專案的核心 `PIDController` 實體。
    *   當測試啟動時，會依據設定的 Profile 動態更新目標 RPM (產生階躍響應)，並進行高速的閉迴路控制。
    *   在循環中，頻繁 (約 50ms) 將 `(時間, 目標 RPM, 左馬達 RPM, 右馬達 RPM)` 的數據推播給前端網頁。

3.  **核心控制器與真實馬達 (PIDController & Motor)**:
    *   為了確保測試結果與實際運行完全一致，本專案直接引入主專案 `src/` 底下的 `PIDController.h` 與 `Motor.h`。
    *   硬體參數與腳位配置皆統一讀取自共用的 `src/config.h`。

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
    *   請確保在 PlatformIO 中選擇 **`env:pidtest`** 作為目標編譯環境。
    *   使用 "Upload" 功能將程式碼上傳至您的 ESP32 開發板。

5.  **操作**:
    *   上傳成功後，打開序列埠監控視窗（Baud Rate: 115200），您將會看到 ESP32 連上 WiFi 後所分配到的 IP 位址。
    *   在同一區域網路下的瀏覽器中，輸入該 IP 位址 (或透過 mDNS `http://esp32-ollie.local` 進入)。
    *   在首頁點選「PID Tuner UI」進入測試介面。
    *   在網頁中輸入您想測試的 P, I, D 參數與測試 Profile，然後點擊 "START"。
    *   觀察圖表中的曲線行為，判斷系統的響應。一個好的響應通常是快速到達目標值且超調量 (Overshoot) 小、無持續震盪 (Oscillation)。
