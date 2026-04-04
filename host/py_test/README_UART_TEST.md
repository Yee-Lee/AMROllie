Ollie 專案：Raspberry Pi 3B 與 ESP32 UART 通訊測試1. 目的本測試旨在驗證上位機（Raspberry Pi 3B）與下位機（ESP32）之間，透過 UART (硬體序列埠) 進行雙向資料傳輸的穩定性。這將作為未來 Ollie 傳遞導航指令與接收底盤感測器數據的通訊基礎。2. 準備環境2.1 硬體需求上位機：Raspberry Pi 3B下位機：ESP32 開發板線材：杜邦線（母對母）至少 3 條2.2 軟體需求Raspberry Pi：Python 3，需安裝 pyserial 套件 (pip install pyserial)。ESP32：VS Code + PlatformIO 開發環境。2.3 硬體接線方式⚠️ 關鍵注意：必須「交叉接線 (TX 對 RX)」並且「共地 (GND)」。絕對不可將 RPi 的 5V 接到 ESP32 的 GPIO 上。|| Raspberry Pi 3B 引腳 (物理編號) | 訊號方向 | ESP32 GPIO 腳位 | 說明 || Pin 8 (GPIO 14 / TXD) | $\rightarrow$ | GPIO 16 (RX2) | RPi 發送，ESP32 接收 || Pin 10 (GPIO 15 / RXD) | $\leftarrow$ | GPIO 17 (TX2) | ESP32 發送，RPi 接收 || Pin 6 (GND) | -- | GND | 共地（必須連接，否則會產生亂碼） |3. 實作3.1 ESP32 端代碼 (C++ / PlatformIO)將以下程式碼燒錄至 ESP32。此程式碼會每秒鐘發送一次遞增的測試字串，並監聽來自 RPi 的指令。#include <Arduino.h>

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

3.2 Raspberry Pi 端代碼 (Python)在 RPi 上建立 uart_test.py 並執行。此腳本會持續監聽 /dev/serial0。import serial

# 初始化序列埠 (RPi 3B 預設 GPIO UART 為 /dev/serial0)
ser = serial.Serial('/dev/serial0', 115200, timeout=1)
ser.flush()

print("Ollie 正在等待 ESP32 數據...")

try:
    while True:
        if ser.in_waiting > 0:
            # 讀取一行，並使用 errors='ignore' 避免雜訊導致解碼崩潰
            line = ser.readline().decode('utf-8', errors='ignore').rstrip()
            if line:
                print(f"收到來自 ESP32 的指令: {line}")

except KeyboardInterrupt:
    print("\n通訊程式已停止")
finally:
    ser.close()

4. 預期結果ESP32 序列埠監控視窗 (Serial Monitor)：每秒會印出 已發送 -> ESP32_Test_X。Raspberry Pi 終端機：執行 Python 腳本後，會穩定顯示以下輸出，不會中斷或報錯：Ollie 正在等待 ESP32 數據...
收到來自 ESP32 的指令: ESP32_Test_1
收到來自 ESP32 的指令: ESP32_Test_2
收到來自 ESP32 的指令: ESP32_Test_3

5. 偵錯方法 (Troubleshooting)若通訊失敗或出現異常，請依序檢查以下項目：| 症狀 | 可能原因 | 解決方式 || Python 噴出 UnicodeDecodeError 錯誤 | 啟動瞬間雜訊導致解碼失敗。 | 確保 Python 代碼中的 decode 加入 errors='ignore'。 || RPi 收到大量 ff 或亂碼 | 1. 未共地。  2. RPi 3B mini-UART 時脈飄移。 | 1. 檢查 Pin 6 (GND) 是否接妥。  2. 修改 /boot/config.txt，加入 enable_uart=1 並重啟 RPi。 || 完全收不到任何數據 | TX/RX 接反，或鮑率不一致。 | 確認 RPi Pin 8 接 ESP32 RX，Pin 10 接 TX。確認雙方鮑率皆為 115200。 || PlatformIO 編譯報錯 (Serial was not declared) | 缺少 Arduino 核心定義。 | 確認 .cpp 檔案第一行有加入 #include <Arduino.h>。 |

