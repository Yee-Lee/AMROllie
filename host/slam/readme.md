# Ollie SLAM 建圖系統 (Slam Toolbox)

本目錄包含 Ollie 機器人進行 2D 空間建圖（SLAM）的相關設定與啟動腳本。
我們使用 ROS2 的 `slam_toolbox` 搭配非同步線上模式 (`online_async`) 進行建圖，適合運算資源有限的 Raspberry Pi 3B 上位機。

## 1. 系統依賴與安裝

在 RPI3B (上位機) 上執行建圖前，請確保已安裝必要的 ROS2 套件：
```bash
sudo apt update
sudo apt install ros-$ROS_DISTRO-slam-toolbox
sudo apt install ros-$ROS_DISTRO-navigation2 ros-$ROS_DISTRO-nav2-bringup
```

## 2. 啟動建圖流程

為了確保 `slam_toolbox` 能正確辨識 Ollie 的實體座標（`base_link`），我們不使用系統預設的啟動檔，而是使用本目錄下的自定義啟動腳本與參數檔。

### Step 2.1 啟動底層感測器與控制
在啟動 SLAM 之前，請確認以下節點已正常運行：
1. **ESP32 (micro-ROS agent)**：提供輪式里程計 `/odom` 與 `/tf` 座標轉換。
2. **LiDAR 驅動**：提供雷射點雲 `/scan`。
3. **Robot State Publisher**：透過 URDF 發佈 `base_link` 到各感測器（如雷射、車輪等）的靜態 TF 轉換。

### Step 2.2 啟動 SLAM 節點
執行本目錄下的專屬啟動腳本，此腳本會強制載入正確的參數：

```bash
cd ~/workspace/AMROllie/host/slam/
ros2 launch ollie_slam_launch.py
```

### Step 2.3 開始建圖
1. 在 macOS 開發機上啟動 Foxglove Studio。
2. 訂閱 `/map`、`/scan` 與 `/tf` 以觀察建圖狀態。
3. 使用 PS4 手把以**低速**遙控 Ollie 繞行空間，完成環境探索。

### Step 2.4 儲存地圖
當建圖完成後，開啟新終端機執行以下指令將地圖存檔。這將產生 `.pgm` (圖像) 與 `.yaml` (描述檔) 供後續 Nav2 導航使用：
```bash
ros2 run nav2_map_server map_saver_cli -f ~/workspace/AMROllie/host/slam/maps/my_home_map
```

---

## 3. 常見問題與除錯紀錄 (Troubleshooting)

### 問題：無法建圖且出現 `[WARN] Failed to compute odom pose`
**現象描述**：
SLAM 啟動後，Foxglove 未顯示地圖，且終端機持續跳出警告訊息 `Failed to compute odom pose`。

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