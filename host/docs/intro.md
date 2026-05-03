# Ollie AMR 上位機 (Raspberry Pi 3B) 系統架構與介紹

本文件旨在說明 Ollie 專案中，Raspberry Pi 3B 作為「上位機 (Host)」的系統定位、硬體配置、通訊機制以及核心軟體架構。

## 1. 系統定位與設計哲學

在 Ollie 的雙層架構中，秉持著「上帝的歸上帝，凱撒的歸凱撒」的設計哲學：
- **上位機 (Raspberry Pi 3B)**：負責耗費資源的「高階運算」與「決策」。包含但不限於：ROS 2 系統運行、micro-ROS 代理、SLAM 建圖、Nav2 導航、感測器資料融合、Web UI 以及搖桿訊號解析。
- **下位機 (ESP32)**：專注於「高即時性」的底層硬體控制。包含馬達 PID 控制、超音波防撞、IMU 姿態解算與真實里程計 (Odometry) 計算。

## 2. 硬體架構與外接設備

RPi3B 作為車體的核心樞紐，其周邊連接了以下主要硬體設備：
1. **下位機控制板 (ESP32)**：透過 **USB 轉 TTL 模組**進行通訊（將 RPi3B 原生的硬體 GPIO UART 保留給藍牙通訊使用）。
2. **LD19 光達 (LiDAR)**：透過 USB 轉接板連接，負責提供 2D 雷射掃描數據，用於 SLAM 與避障。
3. **PS4 藍牙/USB 手把**：透過 USB 直連，用於實車手動遙控 (Teleop)。
4. **電源模組**：(未來補充：提供 RPi3B 穩定 5V 供電的機制)。

> **💡 USB 裝置綁定**：由於 ESP32 與 LiDAR 皆依賴 USB 序列埠，為了避免重啟或插拔導致 `/dev/ttyUSB*` 順序跳動錯亂，系統透過 `udev` 規則將 ESP32 固定映射為 `/dev/ollie_core`，而 LiDAR 固定映射為 `/dev/ollie_lidar`。

## 3. 軟體環境與基礎設施

- **作業系統**：Ubuntu 22.04 (Server / 無頭模式 Headless)
- **ROS 版本**：ROS 2 Humble
- **容器化環境**：Docker (用於運行 `micro-ROS Agent`)
- **進程管理**：Linux `systemd` (確保核心服務開機自啟與崩潰重啟)

## 4. 上下位機通訊機制

RPi3B 與 ESP32 之間捨棄了傳統的自定義字串解析，全面採用 **micro-ROS** 架構，達成原生的 Topic/Service 傳遞。

- **實體層**：RPi3B 端透過 USB 轉 TTL 模組與 ESP32 進行 UART 序列通訊 (TX/RX 交叉接線並共地)，鮑率設定為 `460800`。
- **裝置綁定**：使用 `udev` 規則將通訊埠固定為 `/dev/ollie_core`。
- **代理層 (Agent)**：RPi3B 透過 Docker 運行 `microros/micro-ros-agent`，並指定 `ROS_DOMAIN_ID=30` 避免區網干擾。
- **資料流向**：
  - **RPi3B -> ESP32**：發送速度指令 (`/cmd_vel`)、服務請求 (`/reset_odom`)。
  - **ESP32 -> RPi3B**：接收里程計 (`/odom` 與 TF)、超音波測距 (`/sonar/left`, `/sonar/right`)。

## 5. 核心軟體模組與 Systemd 服務

為了保證機器人的穩定運作，RPi3B 上的核心模組被拆分為獨立的背景服務 (`/etc/systemd/system/`)：

1. **通訊橋樑 (`ollie_microros.service`)**
   - 啟動 micro-ROS Agent，建立 ROS 2 與 ESP32 的連線。
2. **機器人狀態發布 (`ollie_description.service`)**
   - 載入 URDF 模型，發布機器人的靜態與動態 TF 座標轉換。
3. **光達驅動 (`ollie_lidar.service`)**
   - 啟動 LD19 光達節點，發布 `/scan` 話題供建圖使用。
4. **搖桿遙控 (手動啟動或整合至 Launch)**
   - 使用 `joy_linux` 與 `teleop_twist_joy` 將 PS4 訊號轉換為 `/cmd_vel`。
5. **[未來規劃] 導航與建圖 (SLAM / Nav2)**
   - 負責建立環境地圖並執行自動導航。

## 6. 目錄結構與模組詳情

`host/` 目錄下包含了上位機的各項子系統配置與詳細說明，欲了解各模組的具體使用方式與測試步驟，請進入以下子目錄查看對應的 `readme.md`：

- **`micro_ros/`**：包含啟動 `micro-ROS Agent` 的 Docker 腳本與 Udev 綁定說明。
- **`joystick/`**：PS4 手把遙控開發與部署指南，包含 ROS 2 參數 (`teleop_twist_joy`) 設定與映射邏輯。
- **`ros2_ws/`**：上位機的 ROS 2 工作區，包含雷達驅動 (`ldlidar`)、機器人模型描述 (`ollie_description`) 等。
- **`systemd/`**：存放將各模組封裝為 Linux 背景服務的 `.service` 配置檔與部署指令 (開機自啟與崩潰重啟機制)。
- **`uart_test_py/`**：Python 開發的 UART 裸測腳本，用於除錯與驗證 RPi3B 與 ESP32 的底層硬體通訊。
- **`lidar/`**：存放光達的基礎連接說明（ROS 2 的完整整合指南則位於 `ros2_ws/src/ldlidar/`）。
- **`docs/`**：存放上位機系統架構相關的說明文件（即本目錄）。
- **`webserver/`**：*(已廢棄 Obsolete)* 早期未整合手把前，用於測試的網頁版遙控介面。