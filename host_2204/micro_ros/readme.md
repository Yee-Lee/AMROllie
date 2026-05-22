# Ollie micro-ROS Agent 執行指南

本目錄包含在上位機（例如 Raspberry Pi 或 PC）上運行 `micro-ROS Agent` 的相關腳本與說明。透過此 Agent，可以將下位機（ESP32）透過 UART 發送的資料橋接至 ROS 2（Humble）系統中。

> **💡 效能優化建議**：針對資源受限的平台（如 Raspberry Pi 3B, 1GB RAM），強烈建議採用 **「原生編譯為主，Docker 為輔」** 的雙軌策略。原生執行可節省約 100~150MB 記憶體並大幅縮短開機時間。

---

## 🌟 核心使用策略：雙軌並行

1. **「原生編譯 (Native Build)」作為日常主力 (推薦)**：
   - **優勢**：零容器化開銷，節省資源，開機延遲最低。
   - **情境**：實際上線運行、執行高耗能的 SLAM 與導航任務。
2. **「Docker 容器」作為退路機制 (Fallback)**：
   - **優勢**：環境絕對乾淨、隔離，免除編譯環境變數等疑難雜症。
   - **情境**：原生環境損壞時救火、交叉驗證通訊功能，或快速體驗測試。

---

## 1. 硬體與連接埠設定 (通用 Udev 規則)

無論使用哪種方案，都建議設定 `udev` 規則將序列埠固定映射為 `/dev/ollie_core`。

### 建立 Udev 規則：

1. **查詢裝置資訊**（假設裝置目前在 `/dev/ttyUSB0`）：
   ```bash
   udevadm info -a -n /dev/ttyUSB0 | grep '{idVendor}'
   udevadm info -a -n /dev/ttyUSB0 | grep '{idProduct}'
   ```
2. **編輯規則檔案**：
   ```bash
   sudo nano /etc/udev/rules.d/99-ollie-core.rules
   ```
   填入內容（請替換 ID）：
   - **USB 轉 TTL**：`SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="ollie_core", MODE="0666"`
   - **Pi 實體 GPIO UART**：`KERNEL=="ttyS0", SYMLINK+="ollie_core", MODE="0666"`
3. **套用規則**：
   ```bash
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ls -l /dev/ollie_core  # 確認連結已建立
   ```

---

## 2. 方案 A：原生編譯與執行 (主力推薦)

請確認主機已安裝 **ROS 2 Humble**。

### 2.1 建立與編譯工作區

```bash
# 1. 載入 ROS 2 環境 (建議加入 ~/.bashrc)
source /opt/ros/humble/setup.bash

# 2. 建立工作區並下載 micro-ROS Setup 工具
mkdir -p host/ros2_ws/src
cd host/ros2_ws/src
git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git
cd ..

# 3. 初始化 rosdep、安裝編譯工具與相依套件，並編譯 Setup 工具
sudo apt update
sudo apt install -y python3-rosdep python3-vcstool python3-colcon-common-extensions
sudo rosdep init
rosdep update
rosdep install --from-paths src --ignore-src -y
colcon build
source install/local_setup.bash

# 4. 建立並編譯 Agent 專案 (在 Pi 3B 上可能需要較長時間)
ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
source install/local_setup.bash
```


### 2.2 執行原生 Agent

```bash
export ROS_DOMAIN_ID=30
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -v6
```

---

## 3. 方案 B：Docker 環境準備與執行 (退路機制)

### 3.1 準備工作

```bash
# 1. 安裝 Docker (若尚未安裝)
curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh
sudo usermod -aG docker $USER  # 需重新登入套用

# 2. 拉取映像檔
docker pull microros/micro-ros-agent:humble
```

### 3.2 執行與控制

- **啟動 Agent**：`./run_docker_micro_ros.sh`
- **手動開啟 Docker 服務**（若已依下方效能優化策略禁用）：`./start_docker_service.sh`

> **⚠️ 提醒**：若要在主機端查看話題，請務必執行 `export ROS_DOMAIN_ID=30`。

---

## 4. 效能優化策略：彻底禁用 Docker 常駐

在 Raspberry Pi 3B 上，建議平常完全關閉 Docker 服務以釋放記憶體：

```bash
# 停止並禁用開機自啟
sudo systemctl stop docker.service docker.socket containerd.service
sudo systemctl disable docker.service docker.socket containerd.service
```

需要使用時，執行 `./start_docker_service.sh` 即可手動喚醒。

---

## 5. 自動化執行 (Systemd)

若希望原生 Agent 在開機時自動執行，可參考專案中的 `host/systemd/` 目錄，將 `source` 環境變數與 `ros2 run` 指令封裝後，建立 systemd 服務。
