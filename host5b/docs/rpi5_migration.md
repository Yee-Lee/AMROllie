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

# 3. 安裝 ROS 2 Jazzy 基礎包
sudo apt update
sudo apt upgrade
sudo apt install ros-jazzy-ros-base python3-colcon-common-extensions

# 4. 載入環境 (建議加入 ~/.bashrc)
source /opt/ros/jazzy/setup.bash
```

## 3. micro-ROS Agent (Jazzy 原生編譯)

由於我們偏好原生執行，且環境已從 Humble 變為 Jazzy，需要重新建立 Agent 工作區：

```bash
mkdir -p ~/ollie_ws/src
cd ~/ollie_ws/src
git clone -b jazzy https://github.com/micro-ROS/micro_ros_setup.git
cd ..
rosdep update
rosdep install --from-paths src --ignore-src -y
colcon build
source install/local_setup.bash
ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
```

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

> **⚠️ 警告**：這些操作會停用 Ubuntu 的自動維護機制，您的系統將完全由您手動掌控。

### 5.1 徹底移除自動更新與非必要套件管理器
Ubuntu 預設的無人值守更新與 Snapd 會在背景頻繁下載與讀寫，對邊緣運算極為不利。

```bash
# 1. 停用並移除 Snapd (Ubuntu 的沙盒套件系統，極度吃資源)
sudo systemctl stop snapd
sudo systemctl disable snapd
sudo apt-get purge -y snapd
sudo rm -rf /snap /var/snap /var/lib/snapd /var/cache/snapd

# 2. 停用自動更新與非必要伺服器套件
sudo apt-get purge -y unattended-upgrades modemmanager multipath-tools
sudo apt-get autoremove -y
```

### 5.2 關閉耗時的背景排程與開機等待
以下服務會在開機時卡住，或是定時在背景大量讀寫硬碟。

```bash
# 1. 關閉網路連線等待 (讓系統先開機，網路在背景慢慢連)
sudo systemctl disable systemd-networkd-wait-online.service
sudo systemctl mask systemd-networkd-wait-online.service

# 2. 移除 Cloud-init (雲端初始化工具，開機會卡很久)
sudo touch /etc/cloud/cloud-init.disabled
sudo apt-get purge -y cloud-init
sudo rm -rf /etc/cloud /var/lib/cloud

# 3. 關閉耗費大量 I/O 的背景維護任務 (磁區整理、說明檔索引重建)
sudo systemctl disable fstrim.timer man-db.timer dpkg-db-backup.timer
sudo systemctl mask fstrim.service man-db.service dpkg-db-backup.service

# 4. 關閉次要的系統檢測與派送服務
sudo systemctl disable networkd-dispatcher.service e2scrub_reap.service rpi-eeprom-update.service
sudo systemctl mask networkd-dispatcher.service e2scrub_reap.service rpi-eeprom-update.service
```

### 5.3 關閉雙重日誌寫入 (RSyslog)
Systemd 已經有高效能的 `journald` 在管理二進制日誌。Ubuntu 預設又跑了一個 `rsyslog` 把日誌寫成文字檔，這對 SD 卡是雙重打擊。

```bash
sudo systemctl disable rsyslog.service
sudo systemctl mask rsyslog.service
```

### 5.4 優化 ROS 2 網路通訊 (關閉 IPv6)
關閉 IPv6 能有效避免 ROS 2 DDS (Data Distribution Service) 尋找節點時的延遲與封包遺失問題。

```bash
sudo sh -c 'echo "net.ipv6.conf.all.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sh -c 'echo "net.ipv6.conf.default.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sh -c 'echo "net.ipv6.conf.lo.disable_ipv6 = 1" >> /etc/sysctl.conf'
sudo sysctl -p
```

### 5.5 保護硬碟壽命 (啟用 ZRAM 取代傳統 Swap)
即使 RAM 很大，系統偶爾還是會分配 Swap。將 Swap 掛載在記憶體內 (ZRAM) 可以大幅減少對實體硬碟的頻繁讀寫。

```bash
sudo apt-get install -y zram-tools
# 編輯 /etc/default/zramswap
sudo sed -i 's/^#ALGO=.*/ALGO=lz4/' /etc/default/zramswap
sudo sed -i 's/^#PERCENT=.*/PERCENT=50/' /etc/default/zramswap
sudo systemctl restart zramswap
```

### 5.6 關於 CPU 效能模式 (Performance Governor)
* **強烈建議**：如果您有安裝**官方的主動式散熱器 (Active Cooler)**，請執行以下指令鎖定 CPU 為最高頻率，確保算力穩定。
  ```bash
  sudo apt install cpufrequtils
  echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils
  sudo systemctl restart cpufrequtils
  ```
* **警告**：如果您沒有安裝風扇（裸機運行），請絕對不要執行上述指令，否則 RPi 5B 會過熱而發生更嚴重的降頻。

### 5.7 硬體權限 (必備)
延續 RPi 3B 的 Udev 規則。確保 `/etc/udev/rules.d/99-ollie-core.rules` 已建立，內容參考 `micro_ros/readme.md`。

## 6. 手把遙控 (Jazzy)

在 RPi 5B 上安裝 Jazzy 版本的手把套件：

```bash
sudo apt install ros-jazzy-joy-linux ros-jazzy-teleop-twist-joy
```

啟動方式與 RPi 3B 相同，可使用 `joystick/start_joystick.sh`。

## 7. 工作區遷移與編譯

現有的 `ros2_ws` 需要在 Jazzy 環境下重新編譯：

```bash
cd ~/workspace/AMROllie/host5b/ros2_ws
rm -rf build/ install/ log/
colcon build --symlink-install
```
*注意：部分套件 (如 ldlidar) 可能需要安裝 Jazzy 版本的依賴項。*
