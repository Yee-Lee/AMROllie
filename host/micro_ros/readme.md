# Ollie micro-ROS Agent 執行指南

本目錄包含在上位機 (例如 Raspberry Pi 或 PC) 上運行 `micro-ROS Agent` 的相關腳本。透過此 Agent，可以將下位機 (ESP32) 透過 UART 發送的資料橋接至 ROS 2 (Humble) 系統中。

> **💡 效能優化建議**：針對記憶體僅有 1GB 的 Raspberry Pi 3B，強烈建議採用 **「原生編譯為主，Docker 為輔」** 的雙軌策略。請參閱本文最後的 效能優化策略 說明。

## 🌟 核心使用策略：雙軌並行

由於 Raspberry Pi 3B 資源極度有限 (1GB RAM)，常駐運行 Docker 引擎會消耗寶貴的記憶體並拖慢開機速度。為了解放硬體效能，我們制定了以下使用策略：

1. **「原生編譯 (Native Build)」作為日常主力 (推薦)**：
   - **優勢**：零容器化開銷，節省約 100~150MB 記憶體，開機延遲最低。
   - **情境**：實際上線運行、執行高耗能的 SLAM 與導航任務。
2. **「Docker 容器」作為退路機制 (Fallback)**：
   - **優勢**：環境絕對乾淨、隔離，免除編譯環境變數等疑難雜症。
   - **情境**：原生環境損壞時救火、交叉驗證通訊功能，或快速體驗測試。

---

## 1. 硬體與連接埠設定 (通用 Udev 規則)

在 `run_docker.sh` 腳本中，我們預期與 ESP32 通訊的序列埠會掛載於 `/dev/ollie_core`。為了避免每次連接裝置或重啟後，`/dev/ttyUSB*` 或 `/dev/serial*` 變更導致啟動失敗，請務必設定 `udev` 規則來建立符號連結。

### 建立 Udev 規則範例：

1. **若是使用 USB 轉 TTL 模組：**
   先查詢設備的 Vendor ID 和 Product ID (假設您的裝置分配為 `/dev/ttyUSB0`)：
   ```bash
   udevadm info -a -n /dev/ttyUSB0 | grep '{idVendor}'
   udevadm info -a -n /dev/ttyUSB0 | grep '{idProduct}'
   ```
   編輯 udev 規則檔案：
   ```bash
   sudo nano /etc/udev/rules.d/99-ollie-core.rules
   ```
   填入以下內容 (請將 `idVendor` 與 `idProduct` 替換為實際查到的值)：
   ```udev
   SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="ollie_core", MODE="0666"
   ```

2. **若是使用 Raspberry Pi 的實體 GPIO UART (例如 /dev/serial0)：**
   直接建立對於硬體序列埠的規則：
   ```udev
   KERNEL=="ttyS0", SYMLINK+="ollie_core", MODE="0666"
   ```

3. **套用規則：**
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```
   執行 `ls /dev/ollie_core` 檢查是否有成功顯示該設備。

---

## 2. 方案 A：原生編譯與執行 (主力推薦)

在原生主機 (Ubuntu 22.04) 上編譯 micro-ROS Agent，請確保已預先安裝好 `ROS 2 Humble`。

### 2.1 下載並編譯 Agent

開啟終端機，執行以下指令建立工作空間並進行編譯 (在 Raspberry Pi 上編譯可能需要數分鐘，請耐心等待)：

```bash
# 1. 載入 ROS 2 環境
source /opt/ros/humble/setup.bash

# 2. 建立工作空間並下載 micro-ROS 設定工具
mkdir -p ~/micro_ros_ws/src
cd ~/micro_ros_ws/src
git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git
cd ~/micro_ros_ws

# 3. 安裝相依套件並編譯設定工具
rosdep update && rosdep install --from-paths src --ignore-src -y
colcon build
source install/local_setup.bash

# 4. 建立 Agent 工作區並編譯
ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
source install/local_setup.bash
```

### 2.2 執行原生 Agent

編譯完成後，指定您的 ROS 2 網域並啟動 Agent (透過 `/dev/ollie_core` 進行序列埠通訊)：

```bash
export ROS_DOMAIN_ID=30
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -v4
```

---

## 3. 方案 B：Docker 環境準備與執行 (退路機制)

若原生環境發生問題，您可以透過我們準備好的 Docker 環境快速恢復。使用前請先安裝 Docker。

### 3.1 安裝 Docker 與抓取映像檔

```bash
# 1. 下載並執行官方安裝腳本
curl -fsSL https://get.docker.com -o get-docker.sh
sudo sh get-docker.sh

# 2. (選用) 將當前使用者加入 docker 群組，免 sudo 執行 docker
sudo usermod -aG docker $USER
# ⚠️ 執行後，請登出再登入套用權限變更。

# 3. 拉取對應的 micro-ROS 映像檔
docker pull microros/micro-ros-agent:humble
```

### 3.2 執行 Docker Agent 與環境變數設定

確認 Docker 服務正在運行後，您可以直接透過腳本啟動容器：
```bash
./run_docker.sh
```

> **⚠️ 重要提醒：`ROS_DOMAIN_ID`**
> `run_docker.sh` 啟動的 Container 已指定使用 `ROS_DOMAIN_ID=30` 來避免區網干擾。
> 若要在主機端 (Ubuntu/RPi) 正確與 Ollie 通訊、查看話題 (`ros2 topic list`) 或發送控制指令，**請務必在主機的終端機執行：**
> ```bash
> export ROS_DOMAIN_ID=30
> ```
> *(強烈建議將 `export ROS_DOMAIN_ID=30` 加到您的 `~/.bashrc` 中，避免每次開新終端機時遺漏。)*

---

## 4. 效能優化策略：原生編譯為主，Docker 為輔 (退路機制)

Raspberry Pi 3B 資源極度有限 (1GB RAM)。Docker 引擎 (`dockerd` 與 `containerd`) 在背景常駐會吃掉 80MB ~ 150MB 的記憶體，且會嚴重拖慢開機速度 (在網路等待與 I/O 競爭下可達 1 分鐘以上)。

為了解放效能，建議採用**「預設關閉 Docker，平常使用原生編譯 (Native Build) 版本的 Agent」**的策略。保留 Docker 映像檔僅作為不預期狀況下的「退路機制 (Fallback)」。

### 4.1 平常狀態：徹底關閉並禁用 Docker

執行以下指令停止 Docker 相關服務，並取消開機自動啟動：
```bash
sudo systemctl stop docker.service docker.socket containerd.service
sudo systemctl disable docker.service docker.socket containerd.service
```
*設定完成後，Docker 佔用 CPU/RAM 皆為 0，開機延遲為 0 秒。*

### 4.2 需要救火/除錯時：手動喚醒 Docker

當原生環境出現問題，想要切回穩定的 Docker 容器環境進行交叉測試時，只需手動啟動服務：
```bash
# 1. 手動啟動 Docker 引擎 (僅當次開機有效)
sudo systemctl start docker.service

# 2. 執行 Docker 版的 Agent
./run_docker.sh
```
*重開機後，Docker 服務又會自動回到未啟動的休眠狀態，不佔用任何資源。*