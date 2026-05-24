# RViz2 遠端平台監控指南

本指南說明如何建立一個遠端監控平台（例如您的開發筆電），透過 RViz2 即時觀察 AMROllie 的狀態。

---

## 1. 環境安裝與設定

在遠端電腦上，您需要安裝帶有圖形界面的 ROS 2 環境。

### 安裝 RViz2
如果您使用的是 Ubuntu，請安裝桌面版套件：
```bash
sudo apt update
sudo apt install ros-jazzy-desktop
# 或者只安裝 rviz2
sudo apt install ros-jazzy-rviz2
```

### 網路環境檢查
1. **連線**: 確保開發機與機器人處於同一區域網路（Wi-Fi 或乙太網路）。
2. **Domain ID**: 必須與機器人設定相同的 `ROS_DOMAIN_ID`。請檢查您的機器人環境變數：
   ```bash
   echo $ROS_DOMAIN_ID
   ```
   並在開發機設定對應的值：
   ```bash
   export ROS_DOMAIN_ID=<您的_DOMAIN_ID>
   ```
3. **DDS 設定**: 強烈建議使用 CycloneDDS 以獲得更穩定的跨設備通訊。詳細請參考 [CycloneDDS 安裝指南](./readme_dds.md)。

---

## 2. RViz2 操作與元件配置

### 啟動 RViz2
您可以使用目錄下的腳本快速啟動（已預設 DOMAIN_ID 與硬體加速設定）：
```bash
./launch_rviz.sh
```

### 基本 Display 配置
進入 RViz2 後，需進行以下基本設定才能看到物體：

1. **Global Options**:
   - **Fixed Frame**: 改為 `odom` (若已啟動里程計) 或 `base_link` (僅看車體本身)。

2. **添加必要元件 (Add)**:
   - **RobotModel**: 
     - 顯示 Ollie 的 3D 模型。
     - **Description Topic**: 通常為 `/robot_description`。
     - **Visual Enabled**: 勾選。
   - **TF**:
     - 顯示座標系轉換。可以觀察 `base_link` -> `laser_frame` -> `odom` 的關係。
     - 建議將 **Marker Scale** 調小（如 0.1），避免座標軸擋住車體。
   - **LaserScan**:
     - 顯示雷達掃描數據。
     - **Topic**: `/scan`。
     - **Size**: 建議調大一點（如 0.03m），顏色可選為紅色以便觀察。
   - **Odometry**:
     - 顯示里程計軌跡。
     - **Topic**: `/odom`。
     - **Keep**: 設定顯示最近的多少筆數據（例如 100）。

---

## 3. 故障排除 (Debug)

如果在 RViz2 中看不到任何東西，請依序檢查：

### 第一步：檢查是否有收到 Topic
使用我們準備的監控腳本查看數據流量與 HZ：
```bash
python3 mointor_topic.py
```
- 如果 `/odom` 或 `/tf` 顯示 `0.0 Hz (斷線/未發布)`，代表遠端電腦根本沒收到資料。
- 請檢查 `ROS_DOMAIN_ID` 與網路連線。

### 第二步：檢查 QoS 設定
RViz2 預設的 QoS 可能與機器人發布的不匹配，導致數據（如 LiDAR、模型或地圖）無法顯示。

1. **Reliability (可靠性)**:
   - 如果發布端是 `Best Effort`（常見於雷達數據），則 RViz2 的元件設定也必須改為 `Best Effort`。
2. **Durability (耐久性 - 重要！)**:
   - **RobotModel** 與 **Map** 話題通常使用 **`Transient Local`**。
   - 如果您看不到機器人模型或地圖，請在元件設定中將 **Durability Policy** 從 `Volatile` 改為 **`Transient Local`**。
   - **進階建議**：有時上位機發布數據過早，而 RViz2 在啟動後未能正確同步緩存。若切換設定後仍無效，請嘗試：
     - **先開啟 RViz2**，再啟動機器人的相關服務。
     - **重啟發布服務**：在 RViz2 運行期間，重啟機器人的模型發布服務（例如 `sudo systemctl restart ollie_description`），這會強制觸發一次新的數據同步。

---

## 4. 目錄工具說明
- `launch_rviz.sh`: 快速啟動 RViz2 並設定環境變數。
- `mointor_topic.py`: 即時監控 `/odom`, `/scan`, `/tf` 的頻率與數值摘要，是偵錯通訊的首選工具。
