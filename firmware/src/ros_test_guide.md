# micro-ROS 通訊測試指南 (ESP32 <-> Raspberry Pi)

本指南說明如何透過 UART 將 ESP32 (Base Controller) 連接至 Raspberry Pi 上的 micro-ROS Agent，並正確設定 ROS 2 環境進行通訊。

## 1. 硬體接線 (UART 交錯連接)

請確保 ESP32 與 RPi 的腳位正確交錯且**必須共地**：

| ESP32 Pin | 功能 | 對接 | Raspberry Pi Pin | 功能 |
| :--- | :--- | :---: | :--- | :--- |
| GPIO 16 | RX2 | <--> | Pin 8 (GPIO 14) | TXD (serial0) |
| GPIO 17 | TX2 | <--> | Pin 10 (GPIO 15)| RXD (serial0) |
| GND | GND | <--> | 任一 GND | GND |

---

## 2. ESP32 韌體確認與燒錄

請確保韌體已完成以下設定 (目前程式碼已設定好)：
1. **啟用 Serial2**：`main.cpp` 中初始化 `Serial2.begin(460800, SERIAL_8N1, 16, 17);`。
2. **綁定傳輸層**：`taskRos.cpp` 中設定 `set_microros_serial_transports(Serial2);`。
3. **指定 Domain ID**：`taskRos.cpp` 中設定 `rcl_init_options_set_domain_id(&init_options, 30);`。

編譯並燒錄至 ESP32 (`[env:main]`)。

---

## 3. 啟動 RPi 上的 micro-ROS Agent (Docker)

為了避免 Docker 內 `root` 產生的 FastDDS 共享記憶體檔案造成主機端存取權限衝突，我們**不掛載 `/dev/shm`**，強制走主機網路的 UDP 多播。

請在 RPi 終端機執行：

```bash
# 1. 清理可能卡住的 root 權限 DDS 快取
sudo rm -rf /dev/shm/fastrtps*

# 2. 啟動 Docker Agent (指定 DOMAIN_ID=30)
docker run -it --rm \
  --net=host --privileged \
  -v /dev:/dev \
  -e ROS_DOMAIN_ID=30 \
  microros/micro-ros-agent:humble serial --dev /dev/serial0 -b 460800
```

> **注意**：啟動後，若遲遲未見連線，請按一下 ESP32 板子上的 `EN/RST` 重置按鈕，直到看見綠色的 `session established` 訊息。

---

## 4. 主機端驗證與測試

開啟另一個 RPi 終端機，確保環境變數一致，並重啟 ROS 2 Daemon：

```bash
# 1. 同步 Domain ID 並重啟服務
export ROS_DOMAIN_ID=30
ros2 daemon stop
ros2 daemon start

# 2. 確認 Topic 是否正常顯示
ros2 topic list

# 3. 測試收發
ros2 topic echo /odom
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.5}}"
```