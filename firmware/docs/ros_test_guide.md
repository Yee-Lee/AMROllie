# ESP32 micro-ROS 通訊測試指南 (Raspberry Pi via USB-TTL)

本指南說明如何透過 USB-to-TTL 將 ESP32 (Base Controller) 連接至 Raspberry Pi 上的 micro-ROS Agent，並正確設定 ROS 2 環境進行通訊。

## 1. 硬體接線 (USB-to-TTL 轉接)

請確保 ESP32 與 USB 轉接模組的腳位正確交錯，然後將 USB 端插入 RPi：

| ESP32 Pin | 功能 | 對接 | USB-to-TTL 模組 | 功能 |
| :--- | :--- | :---: | :--- | :--- |
| GPIO 16 | RX2 | <--> | TXD | 發送端 |
| GPIO 17 | TX2 | <--> | RXD | 接收端 |
| GND | GND | <--> | GND | 共地 |

---

## 2. ESP32 韌體確認與燒錄

請確保韌體已完成以下設定：
1. **啟用 Serial2**：`main.cpp` 中初始化 `Serial2.begin(115200, SERIAL_8N1, 16, 17);`。
2. **綁定傳輸層**：`taskRos.cpp` 中設定 `set_microros_serial_transports(Serial2);`。
3. **指定 Domain ID**：`taskRos.cpp` 中設定 `rcl_init_options_set_domain_id(&init_options, 30);`。

在 PlatformIO 中選擇 `[env:main]` 環境進行編譯與燒錄。

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
  microros/micro-ros-agent:humble serial --dev /dev/ttyUSB0 -b 115200
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

# 3. 測試上行與下行收發
ros2 topic echo /odom
ros2 topic pub --once /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.5}}"
```