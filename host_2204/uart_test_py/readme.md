Ollie 專案：Raspberry Pi 3B 與 ESP32 UART 通訊測試

1. 目的
本測試旨在驗證上位機（Raspberry Pi 3B）與下位機（ESP32）之間，透過 UART (硬體序列埠) 進行雙向資料傳輸的穩定性。這將作為未來 Ollie 傳遞導航指令與接收底盤感測器數據的通訊基礎。

2. 準備環境

2.1 硬體需求
- 上位機：Raspberry Pi 3B
- 下位機：ESP32 開發板
- 線材：杜邦線（母對母）至少 3 條

2.2 軟體需求
- Raspberry Pi：Python 3，需安裝 pyserial 套件 (`pip install pyserial`)。
- ESP32：VS Code + PlatformIO 開發環境。

2.3 硬體接線方式
⚠️ 關鍵注意：必須「交叉接線 (TX 對 RX)」並且「共地 (GND)」。絕對不可將 RPi 的 5V 接到 ESP32 的 GPIO 上。

| Raspberry Pi 3B 引腳 (物理編號) | 訊號方向 | ESP32 GPIO 腳位 | 說明 |
| --- | --- | --- | --- |
| Pin 8 (GPIO 14 / TXD) | $\rightarrow$ | GPIO 16 (RX2) | RPi 發送，ESP32 接收 |
| Pin 10 (GPIO 15 / RXD) | $\leftarrow$ | GPIO 17 (TX2) | ESP32 發送，RPi 接收 |
| Pin 6 (GND) | -- | GND | 共地（必須連接，否則會產生亂碼） |

3. 實作

3.1 ESP32 端代碼 (C++ / PlatformIO)
將以下程式碼燒錄至 ESP32。此程式碼會每秒鐘發送一次遞增的測試字串，並監聽來自 RPi 的指令。

```cpp
#include <Arduino.h>

#define RXD2 16
#define TXD2 17

unsigned long lastSendTime = 0;
int testCounter = 0;

void setup() {
  // 連接電腦 Serial Monitor (115200)
  Serial.begin(115200); 
  
  // 初始化與 RPi 通訊的 UART2 (115200)
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("Ollie UART 通訊測試已啟動...");
}

void loop() {
  // 1. 接收區：讀取來自 RPi 的資料
  if (Serial2.available()) {
    String receivedData = Serial2.readStringUntil('\n'); 
    receivedData.trim(); 
    Serial.print("收到 RPi 訊息: ");
    Serial.println(receivedData);
  }

  // 2. 發送區：每秒發送一次測試字串
  if (millis() - lastSendTime >= 1000) {
    testCounter++;
    String dataToSend = "ESP32_Test_" + String(testCounter);
    
    // println 會自動加上 \r\n，RPi 端才能用 readline() 判斷結束
    Serial2.println(dataToSend); 
    Serial.print("已發送 -> ");
    Serial.println(dataToSend);

    lastSendTime = millis();
  }
}
```

3.2 Raspberry Pi 端自我迴圈測試 (uart_dev_test.py)
在正式與 ESP32 連接前，可先將 RPi 的 Pin 8 (TXD) 與 Pin 10 (RXD) 以杜邦線短接（Loopback），並執行 `uart_dev_test.py` 確認本機 UART 硬體是否正常：

```python
import serial
import time

# 這裡請嘗試 /dev/ttyS0 或 /dev/ttyAMA0 (或者是 /dev/serial0)
port = '/dev/serial0' 
baud = 115200

try:
    ser = serial.Serial(port, baud, timeout=1)
    print(f"正在測試 {port}... 請確保 Pin 8 與 Pin 10 已用杜邦線短路。")
    
    while True:
        test_str = "Ollie Check\n"
        ser.write(test_str.encode()) # 送出數據
        
        # 讀取數據
        if ser.in_waiting > 0:
            data = ser.readline().decode('utf-8')
            print(f"收到回傳: {data.strip()}")
        else:
            print("未收到回傳，請檢查腳位或配置。")
            
        time.sleep(1)
        
except Exception as e:
    print(f"錯誤: {e}")
```

3.3 Raspberry Pi 端雙向通訊測試代碼 (uart_test.py)
與 ESP32 接妥後（注意 TX/RX 交叉接線並共地），在 RPi 上執行 `uart_test.py`。此腳本會持續監聽 `/dev/serial0`，並且每兩秒發送一次測試訊息給 ESP32。

```python
import serial
import time

# 設定 Serial Port，將 timeout 稍微調短，避免 readline 卡住太久
ser = serial.Serial('/dev/serial0', 115200, timeout=0.1)
ser.flush()

print("Ollie 的 UART 雙向測試 (TX/RX) 已啟動...")
print("程式將每 2 秒發送一次測試訊息，並持續監聽接收到的數據。")
print("提示：如果你把 RPI 的 TX 和 RX 短接 (Loopback測試)，你發送的內容會立刻收到。")
print("-" * 50)

last_send_time = time.time()
send_count = 1

try:
    while True:
        # ==========================================
        # 1. TX 發送區塊 (每 2 秒發送一次)
        # ==========================================
        current_time = time.time()
        if current_time - last_send_time >= 2.0:
            # 準備要發送的測試字串 (附帶換行符號 \n)
            test_msg = f"Hello ESP32, this is Ollie RPI! (Count: {send_count})\n"
            
            # 將字串編碼成 UTF-8 位元組後發送
            ser.write(test_msg.encode('utf-8'))
            print(f"[TX 發送] -> {test_msg.strip()}")
            
            last_send_time = current_time
            send_count += 1
            
        # ==========================================
        # 2. RX 接收區塊 (持續檢查)
        # ==========================================
        if ser.in_waiting > 0:
            # 讀取原始位元組
            raw_data = ser.readline()
            
            try:
                # 嘗試解碼
                line = raw_data.decode('utf-8').rstrip()
                print(f"[RX 接收] <- 收到文字: {line}")
            except UnicodeDecodeError:
                # 如果解碼失敗，印出原始 16 進位數值供除錯
                print(f"[RX 接收] <- 收到非文字數據 (Hex): {raw_data.hex()}")
                
        # 稍微暫停 10 毫秒，避免 while 迴圈把 RPI 的 CPU 吃滿
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\n停止 UART 測試")
finally:
    ser.close()
    print("Serial Port 已關閉")
```

4. 預期結果

ESP32 序列埠監控視窗 (Serial Monitor)：每秒會印出 `已發送 -> ESP32_Test_X`。

Raspberry Pi 終端機：執行 Python 腳本後，會穩定顯示以下輸出，不會中斷或報錯：
```text
Ollie 的 UART 雙向測試 (TX/RX) 已啟動...
[TX 發送] -> Hello ESP32, this is Ollie RPI! (Count: 1)
[RX 接收] <- 收到文字: ESP32_Test_1
[RX 接收] <- 收到文字: ESP32_Test_2
```

5. 偵錯方法 (Troubleshooting)

若通訊失敗或出現異常，請依序檢查以下項目：

| 症狀 | 可能原因 | 解決方式 |
| --- | --- | --- |
| Python 噴出 UnicodeDecodeError 錯誤 | 啟動瞬間雜訊導致解碼失敗。 | 確保 Python 代碼中的 decode 加入 errors='ignore'。 |
| RPi 收到大量 ff 或亂碼 | 1. 未共地。  2. RPi 3B mini-UART 時脈飄移。 | 1. 檢查 Pin 6 (GND) 是否接妥。  2. 修改 `/boot/config.txt`，加入 `enable_uart=1` 並重啟 RPi。 |
| 完全收不到任何數據 | TX/RX 接反，或鮑率不一致。 | 確認 RPi Pin 8 接 ESP32 RX，Pin 10 接 TX。確認雙方鮑率皆為 115200。 |
| PlatformIO 編譯報錯 (Serial was not declared) | 缺少 Arduino 核心定義。 | 確認 `.cpp` 檔案第一行有加入 `#include <Arduino.h>`。 |
