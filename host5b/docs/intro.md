# Ollie AMR 上位機 (Raspberry Pi 5B) 系統架構與介紹

本文件旨在說明 Ollie 專案中，Raspberry Pi 5B 作為「上位機 (Host)」的系統定位、硬體配置、通訊機制以及核心軟體架構。

## 1. 系統定位與設計哲學

在 Ollie 的雙層架構中，採用 RPi 5B 提供更強大的運算效能，支撐複雜的 SLAM 與導航：
- **上位機 (Raspberry Pi 5B)**：負責耗費資源的「高階運算」與「決策」。包含但不限於：ROS 2 Jazzy 系統運行、micro-ROS 代理、SLAM 建圖、Nav2 導航、感測器資料融合以及搖桿訊號解析。
- **下位機 (ESP32)**：專注於「高即時性」的底層硬體控制。包含馬達 PID 控制、超音波防撞、IMU 姿態解算與真實里程計 (Odometry) 計算。

## 2. 硬體架構與外接設備

RPi 5B 作為車體的核心樞紐，其周邊連接了以下主要硬體設備：
1. **下位機控制板 (ESP32)**：透過 USB 轉 TTL 模組進行通訊（固定映射為 `/dev/ollie_core`）。
2. **LD19 光達 (LiDAR)**：透過 USB 轉接板連接，提供 2D 雷射掃描數據（固定映射為 `/dev/ollie-lidar`）。
3. **PS4 藍牙手把**：用於實車手動遙控 (Teleop)。
4. **主動式散熱器 (Active Cooler)**：由於 RPi 5B 效能較高，必須配備散熱器以維持長時間高負載運作。

## 3. 軟體環境與基礎設施

- **作業系統**：Ubuntu 24.04 LTS
- **ROS 版本**：ROS 2 Jazzy Jalisco
- **通訊方式**：原生編譯執行 (Native Build) 以獲得最佳效能。
- **進程管理**：Linux `systemd` (確保核心服務開機自啟與崩潰重啟)

## 4. 上下位機通訊機制

RPi 5B 與 ESP32 之間採用 **micro-ROS** 架構，達成原生的 Topic/Service 傳遞。

- **實體層**：UART 序列通訊，鮑率設定為 `115200`。
- **代理層 (Agent)**：RPi 5B 執行原生 `micro-ros-agent`，並指定 `ROS_DOMAIN_ID=30`。
- **資料流向**：
  - **RPi 5B -> ESP32**：發送速度指令 (`/cmd_vel`)、服務請求 (`/reset_odom`)。
  - **ESP32 -> RPi 5B**：接收里程計 (`/odom` 與 TF)、超音波測距。

## 5. 遠端視覺化與網路機制 (RViz2)

ROS 2 採用去中心化的 DDS 廣播協議，這意味著 RPi 5B **不需要**運行任何網頁伺服器或橋接程式 (Bridge)。只要符合以下條件，您的開發機就能自動接收機器人資料：

1. **同網段**：Mac/PC 必須與 RPi 5B 處於同一個區域網路。
2. **相同 Domain ID**：這是最關鍵的一步。在開發機開啟 RViz2 之前，**必須**在其終端機設定環境變數：
   ```bash
   export ROS_DOMAIN_ID=30
   rviz2
   ```
   *(💡 強烈建議將 `export ROS_DOMAIN_ID=30` 加入您開發機的 `~/.bashrc` 或 `~/.zshrc` 中，以免每次手動輸入。)*

## 6. 核心軟體模組與 Systemd 服務

核心模組被拆分為獨立的背景服務 (`/etc/systemd/system/`)：

1. **`ollie_microros.service`**：啟動 micro-ROS Agent 橋接。
2. **`ollie_description.service`**：載入 URDF 並發布 TF 座標。
3. **`ollie_lidar.service`**：啟動 LD19 光達節點。
4. **`ollie_watchdog.service`**：監控系統健康狀態。

## 6. 目錄結構與模組詳情

- **`micro_ros/`**：Agent 執行指南。
- **`joystick/`**：PS4 手把遙控設定與映射。
- **`ros2_ws/`**：ROS 2 工作區，包含雷達驅動與機器人模型。
- **`systemd/`**：Linux 背景服務配置與自動化部署指令。
- **`docs/`**：系統架構與優化說明文件（即本目錄）。

> **💡 平台遷移註記**：若您是從 RPi 3B (Humble) 升級而來，請參閱 `docs/rpi5_migration.md` 了解詳細的環境轉換與相容性檢查步驟。