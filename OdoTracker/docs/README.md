# OdoTracker: ESP32 差速驅動機器人控制器

本專案是一個基於 ESP32 開發的二輪差速驅動機器人韌體。它整合了精密的運動控制系統、即時里程計追蹤，以及一個用於遠端遙控和數據視覺化的網頁介面。

## 核心功能

-   **網頁遠端遙控**: 提供一個適合行動裝置操作的網頁介面，使用者可透過虛擬搖桿，經由 WebSocket 控制機器人的線速度與角速度。
-   **即時里程計 (Odometry)**: 融合輪式編碼器與 IMU 的數據，即時計算機器人的 `(X, Y, θ)` 姿態，並將當前位置同步顯示於網頁介面上。
-   **進階運動控制**:
    -   **逆向運動學解算**: 將期望的線速度和角速度，轉換為左右兩輪的目標轉速(RPM)。
    -   **馬達 PID 閉環控制**: 為每個馬達配備獨立的 PID 控制器，確保馬達轉速能精準追蹤目標。
    -   **角速度 PID 閉環控制**: 使用陀螺儀 (MPU-6050) 來修正角速度的偏差，實現精確的轉向。
    -   **航向鎖定 (Heading Lock)**: 當沒有轉向指令時，角速度控制器會主動維持當前車身方向，實現穩定直線行駛。
-   **即時數據圖表**: 內建一個數據監控網頁 (`/stats`)，可繪製關鍵的即時數據（目標速度 vs. 實際速度、里程計、馬達轉速、PID輸出等），是參數調校與偵錯的絕佳工具。
-   **彈性的參數配置**: 所有硬體腳位、機器人尺寸、PID 常數等都集中在 `config.h` 中，方便使用者根據自己的硬體進行設定。
-   **PlatformIO 專案管理**: 使用 PlatformIO 作為開發環境，並為主要應用程式和附屬的診斷工具設定了不同的編譯環境。

## 硬體需求

-   **主控單元**: ESP32 開發板 (例如：ESP32-DevKitC)
-   **機器人車體**: 一個二輪差速驅動的機器人底盤。
-   **馬達**: 兩個帶有正交編碼器的直流馬達。
-   **馬達驅動板**: 一個雙 H 橋馬達驅動器 (例如：TB6612FNG, L298N)。
-   **慣性測量單元 (IMU)**: MPU-6050 加速規/陀螺儀模組。

## 軟體與函式庫

本專案使用 PlatformIO IDE 進行開發。所有必要的函式庫都已在 `platformio.ini` 中定義，PlatformIO 會自動進行管理：

-   `ESPAsyncWebServer` & `AsyncTCP`: 用於驅動網頁伺服器。
-   `ArduinoJson`: 用於 WebSocket 的數據序列化。
-   `Adafruit MPU6050`: 用於與 IMU 模組通訊。
-   `Adafruit Unified Sensor` & `Adafruit BusIO`: MPU6050 函式庫的依賴項。

## 如何編譯與執行

### 1. 安裝開發環境
請先在 Visual Studio Code 中安裝 [PlatformIO IDE 擴充套件](https://platformio.org/install/ide?install=vscode)。

### 2. 複製專案
將本專案複製到您的本機電腦。

### 3. 建立與設定 `config.h`
-   複製 `src/config.h.default` 檔案，並將其重新命名為 `src/config.h`。
-   打開 `src/config.h` 並根據您的硬體配置進行修改：
    -   設定您的 WiFi 名稱 (`WIFI_SSID`) 和密碼 (`WIFI_PASSWORD`)。
    -   定義您的馬達驅動板、編碼器以及 MPU-6050 所連接的正確 GPIO 腳位。
    -   測量並設定您的機器人實體尺寸 (`WHEEL_DIAMETER`, `WHEEL_BASE`)。
    -   (可選) 調整 PID 與其他控制參數。

### 4. 編譯與上傳主程式 (`odoTracker`)
-   使用 USB 將您的 ESP32 開發板連接到電腦。
-   在 VS Code 的 PlatformIO 視窗中，確認當前環境為 `odoTracker` (通常是預設)。
-   點擊 **Build** (編譯) 與 **Upload** (上傳)。

### 5. 連線與操作
-   上傳完成後，打開 PlatformIO 的 **Serial Monitor** (序列埠監控視窗)。機器人成功連上 WiFi 後，會在此處顯示其取得的 IP 位址。
-   在同一區域網路下的任何裝置（電腦、手機）的瀏覽器中，輸入該 IP 位址。
-   您會看到虛擬搖桿介面，此時即可開始遙控。
-   若要查看即時數據圖表，請訪問 `http://<YOUR_ROBOT_IP>/stats`。

## 如何使用附屬工具 (子專案)

本專案在 `platformio.ini` 中預設了兩個用於硬體偵錯的獨立編譯環境。您可以在 VS Code 底部的狀態列，或 PlatformIO 的專案任務視窗中切換環境。

### I2C 掃描器 (`scanner`)
-   **用途**: 當您不確定 MPU-6050 是否被主板正確偵測時，可以使用此工具。它會掃描 I2C 匯流排上所有已連接的設備位址。
-   **使用方法**:
    1.  在 PlatformIO 環境中切換到 `scanner`。
    2.  編譯並上傳。
    3.  打開 Serial Monitor，您應該會看到類似 `I2C device found at address 0x68 !` 的訊息，這表示 MPU-6050 已被正確識別。

### MPU6050 測試器 (`mpu6050_test`)
-   **用途**: 用於單獨測試 MPU-6050 是否能正常運作並回傳數據。
-   **使用方法**:
    1.  在 PlatformIO 環境中切換到 `mpu6050_test`。
    2.  編譯並上傳。
    3.  打開 Serial Monitor，您會看到程式持續輸出從 MPU-6050 讀取的陀螺儀與加速計原始數據。
