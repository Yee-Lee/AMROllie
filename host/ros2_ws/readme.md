 Ollie ROS 2 工作區與環境建置指南

本目錄 (`host/ros2_ws`) 為 Ollie 上位機（Raspberry Pi 3B 或開發用 PC）的 ROS 2 工作區 (Workspace)。裡面存放了 Ollie 專屬的 ROS 2 套件，包含機器人模型描述 (`ollie_description`) 與光達驅動 (`ldlidar`) 等。

---

## 1. 為什麼我們需要 ROS 2？

在 Ollie 的雙層架構中，下位機 (ESP32) 負責底層硬體的即時控制（如馬達 PID、讀取超音波等）。而上位機則需要處理更複雜的高階邏輯。引入 **ROS 2 (Robot Operating System 2)** 能為我們帶來以下巨大優勢：

1. **標準化的通訊架構**
   過去我們可能需要自己寫 Python 解析 UART 的字串（例如 `V:0.5,W:1.2`），不僅容易出錯且難以擴充。有了 ROS 2 與 `micro-ROS`，上下位機可以直接透過標準的 `Topic` (話題) 與 `Service` (服務) 進行溝通。
2. **豐富的開源生態系**
   我們不需要重新發明輪子。想用 PS4 搖桿遙控？直接載入 `joy` 與 `teleop_twist_joy` 套件；想畫地圖或自動導航？直接對接 `SLAM Toolbox` 與 `Nav2`；想用網頁控制？透過 `rosbridge_suite` 就能無縫接軌。
3. **分散式架構與視覺化除錯**
   ROS 2 天生支援區域網路內的多機通訊 (透過 DDS)。你可以在沒有螢幕的樹莓派上執行核心程式，然後在自己舒適的 Mac/PC 上開啟 Foxglove Studio 或 RViz，透過網路即時監看 Ollie 的 3D 姿態、光達點雲與感測器數據。

---

## 2. 如何安裝 ROS 2 Humble

Ollie 專案採用 **ROS 2 Humble Hawksbill** 版本，且作業系統環境需為 **Ubuntu 22.04**。

> **💡 提示**：若你是在 Raspberry Pi 3B (實體車) 上安裝，為了節省有限的記憶體 (1GB) 與 SD 卡空間，強烈建議安裝無圖形介面的 `ros-base` 版本；若是在個人電腦或虛擬機上開發，則可安裝 `desktop` 版本。

### 步驟 1：設定語系 (Locale)
確保系統支援 UTF-8：
```bash
sudo apt update && sudo apt install locales -y
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8
```

### 步驟 2：啟用 Ubuntu Universe 儲存庫並加入 ROS 2 GPG Key
```bash
sudo apt install software-properties-common -y
sudo add-apt-repository universe

sudo apt update && sudo apt install curl -y
sudo curl -sSL <https://raw.githubusercontent.com/ros/rosdistro/master/ros.key> -o /usr/share/keyrings/ros-archive-keyring.gpg
```

### 步驟 3：將 ROS 2 加入系統的軟體源 (Sources List)
```bash
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
```

### 步驟 4：安裝 ROS 2 與編譯工具
更新軟體清單並開始安裝（樹莓派請選 Base 版）：

```bash
sudo apt update
sudo apt upgrade -y

# 【選項 A】實體車 RPi 3B (無桌面環境推薦)
sudo apt install ros-humble-ros-base -y

# 【選項 B】開發用 PC/Mac 虛擬機 (包含 RViz 等圖形化工具)
# sudo apt install ros-humble-desktop -y

# 安裝 ROS 2 開發與編譯必備工具
sudo apt install python3-colcon-common-extensions python3-rosdep -y
```

### 步驟 5：設定環境變數
為了讓每次打開終端機都能直接使用 `ros2` 指令，請將其加入 `~/.bashrc`：
```bash
echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

---

## 3. 編譯 Ollie 工作區

當你完成 ROS 2 安裝後，就可以來編譯本目錄 (`host/ros2_ws`) 中的原始碼了。

1. 進入工作區目錄：
   ```bash
   cd ~/Workspace/AMROllie/host/ros2_ws
   ```
2. 安裝套件相依性 (rosdep)：
   ```bash
   sudo rosdep init
   rosdep update
   rosdep install -i --from-path src --rosdistro humble -y
   ```
3. 進行編譯：
   ```bash
   colcon build --symlink-install
   ```
4. 編譯成功後，載入該工作區的環境變數即可開始使用：
   ```bash
   source install/setup.bash
   ```