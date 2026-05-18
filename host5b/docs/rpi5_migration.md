# AMROllie Raspberry Pi 5B 遷移指南 (Ubuntu 24.04 + ROS 2 Jazzy)

本文件說明如何將 AMROllie 的上位機系統從 RPi 3B (Humble) 遷移至 RPi 5B (Jazzy)。

## 1. 系統環境說明
- **硬體**：Raspberry Pi 5B (建議 4GB/8GB RAM)
- **OS**：Ubuntu 24.04 LTS (Noble Numbat)
- **ROS 2**：Jazzy Jalisco
- **通訊**：原生執行 micro-ROS Agent

## 2. ROS 2 Jazzy 安裝步驟 (原生)

在 Ubuntu 24.04 上執行以下指令：

```bash
# 1. 設定 Locale
sudo apt update && sudo apt install locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

# 2. 設定來源
sudo apt install software-properties-common
sudo add-apt-repository universe
sudo apt update && sudo apt install curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/index.key -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# 3. 安裝 ROS 2 Jazzy 基礎包與編譯工具 (明確指定 rmw-fastrtps-cpp 避免底層通訊庫遺失)
sudo apt update
sudo apt upgrade -y
sudo apt install -y build-essential ros-jazzy-ros-base ros-jazzy-rmw-fastrtps-cpp python3-colcon-common-extensions python3-rosdep python3-vcstool

# 4. 初始化 rosdep (解決依賴問題必備)
sudo rosdep init
rosdep update

# 5. 載入環境 (建議加入 ~/.bashrc)
source /opt/ros/jazzy/setup.bash
```

## 3. 模組編譯與設定引導

在完成上述 ROS 2 Jazzy 基礎環境的安裝後，請依序前往以下各子目錄，依照其專屬的 README 說明完成套件的下載、編譯與設定。

> **💡 開發原則**：為了保持文件一致性，各模組的詳細編譯與使用指令皆維護在該模組目錄下。本指南僅提供執行順序。

### 步驟 1：建立微控制橋樑 (micro-ROS Agent)
請前往 `micro_ros/readme.md` 閱讀。
- 您需要在該目錄的指引下，於專案工作區原生編譯 Agent。
- 完成後，請務必先進行硬體通訊測試（詳見其除錯 SOP），確保能與 ESP32 正常連線。

### 步驟 2：編譯光達驅動 (LD19 LiDAR)
請前往 `ros2_ws/src/ldlidar/readme.md` 閱讀。
- 該文件將引導您下載官方 Jazzy 相容版本的驅動原始碼，並進行 `colcon build`。
- 包含如何透過 USB 轉 TTL 進行底層硬體裸測的說明。

### 步驟 3：編譯視覺化與通訊核心 (ollie_description)
請前往 `ros2_ws/src/ollie_description/readme.md` 閱讀。
- 此模組負責載入 URDF 與發布 TF。由於使用的是標準 ROS 2 API，不需修改代碼，只需按照說明重新編譯即可。
- 文件中也包含了如何透過 RViz2 進行遠端 3D 監看的詳細設定。

### 步驟 4：設定手把遙控 (Joystick)
請前往 `joystick/readme.md` 閱讀。
- 該文件說明了如何在 RPi 5B 上安裝 Jazzy 版本的手把套件 (`joy_linux` 與 `teleop_twist_joy`)。
- 包含藍牙配對與一鍵啟動腳本的使用方式。

## 4. ESP32 相容性說明

- **目前狀態**：ESP32 端運行的是 micro-ROS Humble。
- **相容性**：Jazzy 版本的 Agent 通常可以與 Humble 版本的 Client 通訊，因為 XRCE-DDS 協議具有向後相容性。
- **測試**：
  ```bash
  ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -b 115200 -v4
  ```
  如果能看到 `/odom` 或 `/sonar` 話題，則無需修改 ESP32 代碼。

## 5. RPi 5B 系統優化 (工業化激進版本)

為了達到**工業化等級的穩定性與可預期性**，我們必須徹底消除系統中任何可能在背景偷偷佔用 CPU 或 I/O 的任務（如自動更新、套件快取、背景索引）。雖然 RPi 5B 效能足夠，但這些不可控的因素會導致機器人在運行導航或 SLAM 時出現突發性的延遲。

> **💡 詳細操作**：請前往並參閱 `docs/optimize_system.md`，依照裡面的 5 個階段執行系統優化指令。設定完成後，您可以利用 `systemd/system_optimize/check_optimizations.sh` 腳本來驗證優化結果。
