# Ollie 專案：硬體通訊除錯首要步驟 (UART 測試)

本測試旨在驗證上位機（Raspberry Pi 5B）與下位機（ESP32）之間，透過 UART (USB 轉 TTL 或硬體序列埠) 進行雙向資料傳輸的穩定性。

> **⚠️ 核心除錯 SOP**：當您發現 micro-ROS 無法連線時，請**首先**關閉 `ollie_microros.service`，然後執行本目錄的腳本。唯有確認底層 UART 硬體接線與通訊完全正常後，才去排查 micro-ROS 的軟體設定問題。

## 1. 準備環境

### 1.1 硬體與接線需求
- 上位機：Raspberry Pi 5B
- 下位機：ESP32 開發板
- 通訊媒介：建議使用 USB 轉 TTL 模組（避免 RPi 原生 GPIO 的時脈問題）。
- **關鍵注意**：必須「交叉接線 (TX 對 RX)」並且「共地 (GND)」。

### 1.2 確認通訊埠名稱
在 Ollie 專案中，我們已透過 `udev` 規則將 ESP32 的通訊埠固定命名為 `/dev/ollie_core`（詳見 `micro_ros/readme.md`）。請確保以下測試腳本皆指向此路徑。

---

## 2. 實作測試

### 2.1 ESP32 端代碼 (C++ / PlatformIO)
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

### 2.2 RPi 5B 端自我迴圈測試 (uart_dev_test.py)
在正式與 ESP32 連接前，可先將 USB TTL 模組的 TX 與 RX 以杜邦線短接（Loopback），並執行 `uart_dev_test.py` 確認 RPi 本機的發送/接收是否正常。

### 2.3 RPi 5B 端雙向通訊測試代碼 (uart_test.py)
與 ESP32 接妥後（注意 TX/RX 交叉接線並共地），在 RPi 5B 上執行 `uart_test.py`。此腳本會持續監聽 `/dev/ollie_core`，並且每兩秒發送一次測試訊息給 ESP32。

```python
import serial
import time

# 設定 Serial Port 為 udev 綁定的名稱
port_name = '/dev/ollie_core'
ser = serial.Serial(port_name, 115200, timeout=0.1)
ser.flush()

print(f"Ollie 的 UART 雙向測試 ({port_name}) 已啟動...")
print("程式將每 2 秒發送一次測試訊息，並持續監聽接收到的數據。")
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
            test_msg = f"Hello ESP32, this is Ollie RPI! (Count: {send_count})\n"
            ser.write(test_msg.encode('utf-8'))
            print(f"[TX 發送] -> {test_msg.strip()}")
            last_send_time = current_time
            send_count += 1
            
        # ==========================================
        # 2. RX 接收區塊 (持續檢查)
        # ==========================================
        if ser.in_waiting > 0:
            raw_data = ser.readline()
            try:
                line = raw_data.decode('utf-8').rstrip()
                print(f"[RX 接收] <- 收到文字: {line}")
            except UnicodeDecodeError:
                print(f"[RX 接收] <- 收到非文字數據 (Hex): {raw_data.hex()}")
                
        time.sleep(0.01)

except KeyboardInterrupt:
    print("\n停止 UART 測試")
finally:
    ser.close()
    print("Serial Port 已關閉")
```

## 3. 偵錯方法 (Troubleshooting)

若通訊失敗，請檢查以下項目：

| 症狀 | 可能原因 | 解決方式 |
| --- | --- | --- |
| RPi 收到大量 ff 或亂碼 | 1. 未共地。 2. 接線鬆脫。 | 1. 檢查 TTL 模組與 ESP32 的 GND 是否接妥。 |
| 完全收不到任何數據 | TX/RX 接反，或鮑率不一致。 | 確認 TX 必須接對方的 RX。確認雙方鮑率皆為 115200。 |
| 出現 Permission Denied | udev 權限未設定。 | 檢查 `/etc/udev/rules.d/99-ollie-core.rules` 是否有加上 `MODE="0666"`。 |
