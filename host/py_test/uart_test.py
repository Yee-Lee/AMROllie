import serial

ser = serial.Serial('/dev/serial0', 115200, timeout=1)
ser.flush()

print("Ollie 正在等待數據...")

try:
    while True:
        if ser.in_waiting > 0:
            # 1. 先讀取原始位元組
            raw_data = ser.readline()
            
            try:
                # 2. 嘗試解碼
                line = raw_data.decode('utf-8').rstrip()
                print(f"收到文字: {line}")
            except UnicodeDecodeError:
                # 3. 如果解碼失敗，印出原始 16 進位數值供除錯
                print(f"收到非文字數據 (Hex): {raw_data.hex()}")

except KeyboardInterrupt:
    print("\n停止監測")
finally:
    ser.close()
