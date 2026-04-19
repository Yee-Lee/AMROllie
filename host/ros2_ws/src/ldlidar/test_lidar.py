import serial
import time

PORT = '/dev/ttyUSB0' 
BAUD_RATE = 230400

try:
    ser = serial.Serial(PORT, BAUD_RATE, timeout=1)
    print(f"成功開啟 {PORT}，等待 LD19 訊號中...")
    
    valid_packet_count = 0
    start_time = time.time()
    
    # 測試讀取 5 秒鐘
    while time.time() - start_time < 5.0:
        byte_data = ser.read(1)
        
        # 尋找 LD19 的 Frame Header 0x54
        if byte_data == b'\x54':
            valid_packet_count += 1
            if valid_packet_count % 10 == 0:
                print(f"收到 LD19 有效封包標頭 (0x54)！目前累計: {valid_packet_count} 個")

    print("\n--- 測試完成 ---")
    if valid_packet_count > 0:
        print("硬體接線與通訊完全正常！Ollie 看見世界啦！")
    else:
        print("有開啟 Port 但沒抓到 0x54 標頭，檢查一下 TX/RX 有沒有接反，或是共地沒接好。")
        
    ser.close()

except Exception as e:
    print(f"發生錯誤: {e}")
