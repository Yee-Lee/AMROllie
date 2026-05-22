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