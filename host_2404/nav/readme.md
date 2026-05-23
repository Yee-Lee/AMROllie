# Ollie 導航系統 (Nav2)

本目錄包含 Ollie 機器人進行自主導航的相關設定與啟動腳本。
我們使用 ROS 2 Jazzy 的 `Navigation2 (Nav2)` 框架。

## 1. 系統依賴與安裝

在上位機 (Host) 上執行導航前，請確保已安裝 Nav2 套件：
```bash
sudo apt update
sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup
```

## 2. 準備地圖

導航需要預先建好的地圖。請將 SLAM 產生的 `.yaml` 與 `.pgm` 檔案放置於 `nav/maps/` 目錄下。

## 3. 啟動導航流程

### 3.1 啟動導航節點
執行 `start_nav.sh` 腳本，該腳本會加載預設地圖並啟動 Nav2 堆疊。

```bash
cd ~/workspace/AMROllie/host_2404/nav/
./start_nav.sh
```

**啟動成功的標誌：**
當所有 Lifecycle 節點完成 Configure 與 Activate 後，您應會在終端機看到類似以下的日誌，特別是出現 `Creating bond timer...` 時代表導航伺服器已完全就緒：

```text
[lifecycle_manager_navigation]: Managed nodes are active
[lifecycle_manager_navigation]: Creating bond timer...
```

### 3.2 在 RViz2 中進行導航

1. **地圖顯示設定 (QoS)**：
   在 RViz2 中加入 Map 插件並選擇 `/map` 話題時，因 Nav2 地圖發布的 QoS 設定，必須將 RViz2 中 Map 插件的 **Durability** 策略設為 **Transient Local**（若為預設的 Volatile 會收不到地圖）。

2. **設定初始姿勢 (Initial Pose)**：
   在 RViz2 工具列點選 `2D Pose Estimate`，並在地圖上標定機器人當前真實的位置與面向角度。

3. **發送目標點 (Navigation Goal)**：
   點選 `Nav2 Goal`，在地圖上選擇目的地。

---

## 4. 系統健康檢查 (Health Check)

若導航無法正常運作，請依序檢查以下項目：

### 4.1 檢查關鍵話題
確認感測器與地圖數據是否有正常流動：
```bash
# 檢查地圖資料 (應輸出一次地圖資訊)
ros2 topic echo /map --count 1

# 檢查代價地圖更新頻率 (預期應有數 Hz)
ros2 topic hz /local_costmap/costmap
ros2 topic hz /global_costmap/costmap

# 檢查雷達數據
ros2 topic hz /scan
```

### 4.2 檢查動作伺服器 (Actions)
導航核心透過 Action 運作，預期應看到 `/navigate_to_pose` 等：
```bash
ros2 action list
# 預期包含: /navigate_to_pose, /compute_path_to_pose, /follow_path, /spin, /wait...
```

### 4.3 檢查座標轉換 (TF Tree)
這是導航最常失敗的原因。必須確保 `map` 到 `base_link` 的鏈接是完整的：
```bash
# 檢查 map -> base_link 的轉換 (需先完成 Initial Pose 初始化)
ros2 run tf2_ros tf2_echo map base_link
```

### 4.4 檢查節點生命週期
確認導航大腦 `bt_navigator` 是否處於 active 狀態：
```bash
ros2 lifecycle get /bt_navigator
# 預期輸出: active [3]
```

---

## 5. 目錄結構說明

- `maps/`: 存放導航用的地圖檔案。
- `nav2_params.yaml`: 導航參數設定（包括局部/全域規劃器、代價地圖等）。
- `ollie_nav_launch.py`: 導航啟動腳本。
- `start_nav.sh`: 便捷啟動入口。
