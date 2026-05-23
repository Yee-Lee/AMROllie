# Ollie SLAM 建圖系統 (Slam Toolbox)

本目錄包含 Ollie 機器人進行 2D 空間建圖（SLAM）的相關設定與啟動腳本。
我們使用 ROS2 的 `slam_toolbox` 搭配非同步線上模式 (`online_async`) 進行建圖，適合 Ubuntu 24.04 上位機 (Host) 執行。

## 1. 系統依賴與安裝

在上位機 (Host) 上執行建圖前，請確保已安裝必要的 ROS2 套件：
```bash
sudo apt update
sudo apt install ros-$ROS_DISTRO-slam-toolbox
sudo apt install ros-$ROS_DISTRO-navigation2 ros-$ROS_DISTRO-nav2-bringup
```

## 2. 啟動建圖流程

為了確保 `slam_toolbox` 能正確辨識 Ollie 的實體座標（`base_link`），我們使用本目錄下的自定義啟動腳本與參數檔。

### Step 2.1 啟動底層感測器與控制
在啟動 SLAM 之前，請確認以下節點已正常運行：
1. **ESP32 (micro-ROS agent)**：提供輪式里程計 `/odom` 與 `/tf` 座標轉換。
2. **LiDAR 驅動**：提供雷射點雲 `/scan`。
3. **Robot State Publisher**：透過 URDF 發佈 `base_link` 到各感測器（如雷射、車輪等）的靜態 TF 轉換。

### Step 2.2 啟動 SLAM 節點
執行本目錄下的專屬啟動腳本，此腳本會載入正確的參數：

```bash
# 確保已進入工作目錄並 source 環境
cd ~/Workspace/AMROllie/host_2404/slam/
ros2 launch ollie_slam_launch.py
```

### Step 2.3 開始建圖
1. 在開發機（或遠端連接的電腦）上啟動 RViz2。
2. 加入 `Map`、`LaserScan` 與 `TF` 插件以觀察建圖狀態。
3. 使用 PS4 手把以**低速**遙控 Ollie 繞行空間，完成環境探索。

### Step 2.4 儲存地圖
當建圖完成後，建立 `maps` 目錄（如果不存在）並執行以下指令將地圖存檔：
```bash
mkdir -p ~/Workspace/AMROllie/host_2404/slam/maps
ros2 run nav2_map_server map_saver_cli -f ~/Workspace/AMROllie/host_2404/slam/maps/my_home_map
```

---

## 3. 常見問題與除錯紀錄 (Troubleshooting)

### 問題：無法建圖且出現 `[WARN] Failed to compute odom pose`
**現象描述**：
SLAM 啟動後，RViz2 未顯示地圖，且終端機持續跳出警告訊息 `Failed to compute odom pose`。

**除錯與驗證步驟**：
1. 首先，檢查 TF 座標樹的連通性：
   ```bash
   ros2 run tf2_tools view_frames
   ```
   確認座標樹是否能正確連起 `map` -> `odom` -> `base_link` -> `base_laser`。如果樹狀結構完整但仍報錯，進入下一步。

2. 檢查 `slam_toolbox` 的內部參數，確認它嘗試尋找的車體座標系名稱：
   ```bash
   ros2 param get /slam_toolbox base_frame
   ```
   **根本原因**：如果上述指令回傳 `String value is: base_footprint`，表示預設設定檔與 Ollie 的實際設計衝突。Ollie 的 URDF 定義車體中心為 `base_link`，導致 SLAM 演算法找不到正確的座標系而卡死。

**解決方案**：
已建立自定義的參數設定檔 `mapper_params.yaml`，確保檔案中包含以下結構來覆蓋預設值：
```yaml
slam_toolbox:
  ros__parameters:
    odom_frame: odom
    map_frame: map
    base_frame: base_link  # 修正為正確的車體坐標系名稱
```

並透過自定義的 `ollie_slam_launch.py` 強制載入此設定檔。
