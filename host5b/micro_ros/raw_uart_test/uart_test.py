import serial
import time

# 設定 Serial Port，將 timeout 稍微調短，避免 readline 卡住太久
SERIAL_PORT = '/dev/ollie_core'
ser = serial.Serial(SERIAL_PORT, 115200, timeout=0.1)
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
