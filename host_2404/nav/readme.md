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

### 3.2 在 RViz2 中進行導航
1. **設定初始姿勢 (Initial Pose)**：在 RViz2 工具列點選 `2D Pose Estimate`，並在地圖上標定機器人當前位置。
2. **發送目標點 (Navigation Goal)**：點選 `Nav2 Goal`，在地圖上選擇目的地。

---

## 4. 目錄結構說明

- `maps/`: 存放導航用的地圖檔案。
- `nav2_params.yaml`: 導航參數設定（包括局部/全域規劃器、代價地圖等）。
- `ollie_nav_launch.py`: 導航啟動腳本。
- `start_nav.sh`: 便捷啟動入口。
