# ollie_description

Ollie AMR 的視覺化與通訊核心套件。負責接收下位機 (ESP32) 的硬體狀態，轉換為 ROS 2 座標系統，並透過 Foxglove Bridge 提供遠端 3D 即時監控。

## 目錄結構

- `urdf/ollie.urdf` : Ollie 實體的 3D 模型藍圖 (純英文，避免解析崩潰)。
- `scripts/real_ollie_core.py` : 里程計座標 (TF) 轉換與關節狀態 (Joint States) 推算節點。
- `launch/ollie_visual.launch.py` : 一鍵啟動模型發布、核心轉換與 Foxglove 通訊橋樑。

## 編譯指南

在工作空間的根目錄 (例如 `~/workspace/AMROllie/host/ros2_ws`) 執行：

```bash
# 建議加上 --symlink-install，這樣未來修改 Python 或 URDF 檔就不需要一直重新編譯
colcon build --packages-select ollie_description --symlink-install
```

## 啟動與使用流程

為了讓 Ollie 順利運作，必須先建立下位機通訊，再啟動 ROS 2 視覺化核心。

### 1. 建立硬體通訊 (Terminal 1)

使用 Docker 啟動 `micro_ros_agent` 接管實體序列埠，並設定與 ROS 2 系統相同的 `ROS_DOMAIN_ID`。

```bash
sudo docker run -it --rm \
  --net=host \
  --privileged \
  -v /dev:/dev \
  -e ROS_DOMAIN_ID=30 \
  microros/micro-ros-agent:humble \
  serial --dev /dev/serial0 -b 460800 -v6
```
> 啟動後，按下 ESP32 的 **EN** 按鈕重啟，確認畫面出現綠色的連線建立日誌。

### 2. 啟動視覺化核心 (Terminal 2)

設定好環境變數後，啟動 Launch 檔。

```bash
export ROS_DOMAIN_ID=30
source install/setup.bash
ros2 launch ollie_description ollie_visual.launch.py
```

### 3. Foxglove 遠端監看

在 Mac 或 PC 端開啟 Foxglove Studio：
1. 新增 **Foxglove WebSocket** 連線。
2. 網址輸入：`ws://ollie.local:8765` (或直接輸入 RPI IP)。
3. 在 3D 面板左側設定中：
   - 將 **Global Frame** 切換為 `odom` (切換為上帝視角)。
   - 開啟 Topics 中的 `/robot_description` (顯示實體藍色車身)。
4. 轉動實體輪胎，畫面即會同步更新。

---

## 常用除錯指令

在新的終端機分頁中，必須先執行 `export ROS_DOMAIN_ID=30` 才能與 Ollie 通訊。

### 檢查資料發布頻率 (Hz)
確保里程計傳輸順暢 (健康指標約為 20Hz)：
```bash
ros2 topic hz /odom
```

### 檢視即時數據
查看底盤傳來的精確坐標與速度：
```bash
ros2 topic echo /odom
```

### 歸零里程計 (重設 Odom)
呼叫 Service 將下位機的累積里程計瞬間歸零 (需 ESP32 韌體端有實作 `reset_odom_callback`)：
```bash
ros2 service call /reset_odom std_srvs/srv/Trigger
```

### 遙控車體 (Teleop)
啟動鍵盤控制節點，發送 `/cmd_vel` 指令給 ESP32：
```bash
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```
