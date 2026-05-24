# Ollie 導航系統 (Nav2)

本目錄包含 Ollie 機器人進行自主導航的相關設定與啟動腳本。系統基於 ROS 2 Jazzy 的 `Navigation2 (Nav2)` 框架建構。

## 1. 系統依賴與安裝

在上位機 (Host) 執行導航前，請確保已安裝必要的 Nav2 組件：
```bash
sudo apt update
sudo apt install ros-jazzy-navigation2 ros-jazzy-nav2-bringup
```

## 2. 準備地圖數據

導航系統需要預建地圖作為參考。請將 SLAM 產生的 `.yaml` 與 `.pgm` 檔案放置於 `nav/maps/` 目錄下。

## 3. 啟動與操作流程

### 3.1 啟動導航節點
執行 `start_nav.sh` 腳本，該腳本會自動加載地圖並初始化 Nav2 堆疊。

```bash
cd ~/workspace/AMROllie/host_2404/nav/
./start_nav.sh
```

**確認啟動成功：**
當所有 Lifecycle 節點完成組態 (Configure) 並激活 (Activate) 後，終端機應顯示 `Managed nodes are active` 及 `Creating bond timer...`，這代表導航伺服器已準備就緒。

### 3.2 RViz2 操作指引

1. **地圖顯示 (QoS 設定)**：
   在 RViz2 中訂閱 `/map` 時，請將 Map 插件的 **Durability** 策略改為 **Transient Local**，否則將無法接收到地圖數據。

2. **初始位置校正 (2D Pose Estimate)**：
   使用 RViz2 工具列的 `2D Pose Estimate`，在地圖上標定機器人的實際位置與朝向。

3. **發送導航目標 (Nav2 Goal)**：
   使用 `Nav2 Goal` 工具在地圖上點擊目的地，機器人即開始規劃路徑並移動。

---

## 4. 關鍵修復與優化記錄 (Fix Log)

本系統針對 Raspberry Pi 5 的硬體特性與 Ollie 的機身設計進行了深度調優：

### 4.1 生命週期管理 (Lifecycle Management)
*   **顯性節點控制**：在 `nav2_params.yaml` 中明確定義 `lifecycle_manager` 的節點列表，解決了自動啟動時部分組件卡在未配置狀態的問題。
*   **插件規範化**：全面採用 ROS 2 Jazzy 標準的 `::` 插件路徑格式（如 `nav2_navfn_planner::NavfnPlanner`），並移除冗餘列表以提升載入穩定性。

### 4.2 嵌入式平台穩定性
*   **TF 容錯增強**：將 `transform_tolerance` 調升至 **1.0s**。這能有效緩衝 Raspberry Pi 5 在高負載下處理座標轉換 (TF) 的微小延遲，避免導航系統因逾時而崩潰。
*   **動態參數微調**：
    *   **速度上限**：將 `max_vel_x` 提升至 **0.4 m/s**，使移動更流暢。
    *   **判定精準度**：將 `required_movement_radius` 縮小至 **0.05 m**，優化小範圍內的定位判定。

---

## 5. 安全機制：碰撞監測 (Collision Monitor)

系統整合了 `nav2_collision_monitor`，為 Ollie 提供硬體級的即時安全保護。

### 5.1 防護原理
*   **虛擬保護圈**：系統以 `base_link` 為中心建立 `PolygonStop` 區域。
*   **攔截機制**：一旦感測器偵測到障礙物進入保護圈，系統會立即攔截導航指令 (`cmd_vel_nav`) 並改發停止訊號至底盤。

### 5.2 ⚠️ 重要：LiDAR 誤判處理
實機運行時，LiDAR 可能因機身結構或地面反射產生「虛假近距離障礙物」，導致機器人因誤觸保護圈而鎖死。
*   **解決方案**：必須配合 `ldlidar` 的數據濾波功能。
*   **詳細指引**：請務必參閱 [**ldlidar 章節：範圍過濾器配置**](../ldlidar/readme.md)，確保導航系統接收的是清理後的乾淨數據。

---

## 6. 故障排除與健康檢查

若導航無法正常運行，請依序執行以下檢查：

### 6.1 話題流動檢查
```bash
# 地圖連通性
ros2 topic echo /map --count 1
# 代價地圖更新頻率 (預期 > 1Hz)
ros2 topic hz /local_costmap/costmap
# 原始感測器數據
ros2 topic hz /scan
```

### 6.2 動作伺服器狀態
```bash
ros2 action list
# 預期應包含: /navigate_to_pose, /follow_path, /compute_path_to_pose 等
```

### 6.3 座標轉換鏈 (TF Tree)
確保 `map` -> `odom` -> `base_link` 完整連接：
```bash
ros2 run tf2_ros tf2_echo map base_link
```

### 6.4 節點生命週期狀態
```bash
ros2 lifecycle get /bt_navigator
# 正常應回傳: active [3]
```

---

## 7. 目錄結構

- `maps/`: 存放地圖設定檔 (`.yaml`) 與圖檔 (`.pgm`)。
- `nav2_params.yaml`: 核心導航參數（規劃器、代價地圖、安全參數）。
- `ollie_nav_launch.py`: 整合式啟動腳本。
- `start_nav.sh`: 便捷啟動入口。
