# ESP32 與 RPi UART 通訊測試

本測試專案提供了一個簡單且獨立的 UART 測試環境，用於驗證 ESP32 與 Raspberry Pi (RPi) 之間的硬體雙向序列通訊是否正常運作。

## 硬體接線定義

通訊使用 ESP32 的 `Serial2`，通訊速率 (Baud Rate) 統一設定為 **115200**。

| ESP32 腳位 | 腳位功能 | 對接至 Raspberry Pi |
| :--- | :--- | :--- |
| **GPIO 16** | RXD2 (接收 RPi 訊號) | UART TXD |
| **GPIO 17** | TXD2 (發送訊號至 RPi) | UART RXD |
| **GND** | 接地 | GND |

> **⚠️ 注意**：ESP32 與 Raspberry Pi 的 GND 必須互相連接（共地），以確保通訊的電位基準一致。

## 程式運作說明 (`main_uart_test.cpp`)

本測試程式包含以下兩大功能：

1. **接收測試 (RX)**：
   - 持續監聽來自 RPi 的資料。
   - 會讀取字串直到遇到換行符號 (`\n`)，並自動清除多餘的空白或結尾字元 (`\r`)。
   - 將收到的 RPi 訊息顯示於電腦端的 Serial Monitor。

2. **發送測試 (TX)**：
   - 透過 `millis()` 計算時間，每隔兩秒鐘 (2000 ms) 主動向 RPi 發送一筆訊息。
   - 測試訊息格式為遞增字串：`Hello RPI, this is Ollie ESP32! (Count: <計數器>)`。
   - 同步在電腦端 Serial Monitor 顯示發送狀態，以便觀察與比對。

## 如何編譯與測試

專案的 `platformio.ini` 中已建立了獨立的 `[env:uart_test]` 環境，並且設定了 `build_src_filter` 以確保只編譯本測試程式碼，不會干擾原本的主程式。

1. 開啟 PlatformIO，將專案環境 (Project Environment) 切換至 `env:uart_test`。
2. 執行 **Build** 與 **Upload** 將程式碼燒錄至開發板。
3. 開啟 **Serial Monitor** (Baud rate: 115200)。
4. 確認看到 `Ollie 的 ESP32 UART 雙向測試已啟動...`，接著即可觀察每兩秒送出的測試訊息，以及接收到的 RPi 回應。