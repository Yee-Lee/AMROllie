#include <Arduino.h>

#define RXD2 16
#define TXD2 17

unsigned long lastSendTime = 0;
int testCounter = 1;

void setup() {
  // 連接電腦 Serial Monitor (用於監控)，Baud rate 設為 115200
  Serial.begin(115200); 
  
  // 連接 RPi 的 UART (用於實際通訊)，設定 RX 為 GPIO16，TX 為 GPIO17
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  
  // 縮短 Serial2 的 Timeout 時間為 100 毫秒
  // 避免 readStringUntil 沒收到 '\n' 時卡住整個 loop 太久
  Serial2.setTimeout(100);

  Serial.println("====================================");
  Serial.println("Ollie 的 ESP32 UART 雙向測試已啟動...");
  Serial.println("====================================");
}

void loop() {
  // ==========================================
  // 1. 接收區 (RX)：讀取來自 RPi 的資料
  // ==========================================
  if (Serial2.available()) {
    // 讀取字串直到遇到換行符號 (\n)
    String receivedData = Serial2.readStringUntil('\n'); 
    
    // 清除字串前後多餘的空白或換行符號 (包含 \r)
    receivedData.trim(); 
    
    // 確保清空後不是空字串才印出來
    if (receivedData.length() > 0) {
      Serial.print("[RX 接收] <- 收到 RPi 訊息: ");
      Serial.println(receivedData);
    }
  }

  // ==========================================
  // 2. 發送區 (TX)：每 2 秒發送一次測試字串
  // ==========================================
  // 使用 millis() 來計時，不使用 delay()，這樣才不會卡住 RX 的接收
  if (millis() - lastSendTime >= 2000) {
    // 組合測試字串
    String dataToSend = "Hello RPI, this is Ollie ESP32! (Count: " + String(testCounter) + ")";
    
    // 發送給 RPi (println 會自動在尾巴加上 \r\n)
    Serial2.println(dataToSend); 
    
    // 在電腦端 Serial Monitor 打印，確認已送出
    Serial.print("[TX 發送] -> ");
    Serial.println(dataToSend);

    testCounter++;
    lastSendTime = millis();
  }
}