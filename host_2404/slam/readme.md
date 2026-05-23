# Ollie SLAM 建圖系統 (Slam Toolbox)

本目錄包含 Ollie 機器人進行 2D 空間建圖（SLAM）的相關設定與啟動腳本。
我們使用 ROS 2 Jazzy 的 `slam_toolbox` 搭配非同步線上模式 (`online_async`) 進行建圖。

## 1. 系統依賴與安裝

在上位機 (Host) 上執行建圖前，請確保已安裝必要的 ROS 2 套件：
```bash
sudo apt update
sudo apt install ros-jazzy-slam-toolbox
sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup
```

## 2. 啟動建圖流程

### 2.1 啟動 SLAM 節點
本目錄的 `ollie_slam_launch.py` 已整合 **Lifecycle 自動化邏輯**。啟動後會自動進行 `Configure` 與 `Activate`，無需手動輸入指令。

```bash
cd ~/workspace/AMROllie/host_2404/slam/
ros2 launch ollie_slam_launch.py
```

**啟動成功的標誌：**
- 終端機顯示 `[slam_toolbox]: Activating`。
- 執行 `ros2 topic list` 應能看到 `/map` 话题。

### 2.2 在 RViz2 中觀察
1. 在遠端開發機開啟 RViz2。
2. **Global Options**: 將 `Fixed Frame` 設定為 `map`。
3. **Add 插件**:
   - **Map**: 話題選擇 `/map`。
   - **LaserScan**: 話題選擇 `/scan`。
   - **RobotModel**: 觀察車體位置。

### 2.3 儲存地圖
當建圖完成後，執行以下指令將地圖存檔：

```bash
# 建立目錄
mkdir -p ~/workspace/AMROllie/host_2404/slam/maps

# 執行存檔 (注意：路徑大小寫必須正確)
ros2 run nav2_map_server map_saver_cli -f ~/workspace/AMROllie/host_2404/slam/maps/my_home_map
```

---

## 3. 進階參數說明 (`mapper_params.yaml`)

- **解析度 (Resolution)**: 目前設定為 `0.03` (3cm)，提供更精細的邊緣。
- **更新頻率**: `map_update_interval` 為 `2.0` 秒，讓 Rviz 顯示更即時。
- **參數類型**: ROS 2 嚴格要求類型匹配（例如 `scan_buffer_maximum_scan_distance: 10.0` 必須帶小數點）。

---

## 4. 常見問題與除錯 (Troubleshooting)

### 如何針對 Launch 進行 Debug？
如果啟動後看不到 `/map`，請依照以下順序檢查 Launch 終端機的輸出：
1. **檢查啟動 Log**: 查看 `Caught exception in callback`。
   - 如果提到 `Wrong parameter type`，代表 `mapper_params.yaml` 裡的數字格式錯誤（整數/浮點數不分）。
2. **檢查 Lifecycle 狀態**: 執行 `ros2 node info /slam_toolbox`。
   - 如果沒看到 Publishers 包含 `/map`，代表節點卡在 `Unconfigured` 狀態。
3. **檢查 TF 樹**: 執行 `ros2 run tf2_tools view_frames`。
   - 必須確保 `map -> odom -> base_link -> base_laser` 完整連通。

### 訊息丟棄警告 (Message Filter dropping message)
**現象**：看到 `discarding message because the queue is full`。
**原因**：計算量（3cm 解析度）大於處理速度，導致緩存區滿載。
**解決**：移動機器人時請保持緩慢，或在參數中增加 `throttle_scans` 數值以跳幀處理。


