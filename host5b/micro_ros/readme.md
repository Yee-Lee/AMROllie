# Ollie micro-ROS Agent 執行指南

本目錄包含在上位機（Raspberry Pi 5B）上運行 `micro-ROS Agent` 的相關腳本與說明。透過此 Agent，可以將下位機（ESP32）透過 UART 發送的資料橋接至 ROS 2（Jazzy）系統中。

> **⚠️ 連線除錯首要原則 (SOP)**：
> 如果您在執行 Agent 時遇到連線失敗、無資料回傳的問題，請**不要急著改 ROS 2 設定**，請遵循以下步驟：
> 1. **釋放通訊埠**：先停止 micro-ROS 服務 (`sudo systemctl stop ollie_microros`) 或關閉正在執行的 Agent。
> 2. **退回硬體裸測**：進入本目錄下的 `raw_uart_test/`，執行底層通訊測試腳本。
> 3. **確認結果**：唯有在裸測腳本中確認能正常收到 ESP32 的字串，且沒有亂碼，才代表「硬體接線 (TX/RX)」與「共地 (GND)」完全正常。此時才能回頭排查 Agent 或 Domain ID 等軟體設定。

> **💡 效能優化建議**：針對機器人即時性要求，強烈建議採用 **「原生編譯 (Native Build)」**。原生執行可免除容器化網路開銷，開機延遲最低。

---

## 🌟 核心使用策略：雙軌並行

1. **「原生編譯 (Native Build)」作為日常主力 (推薦)**：
   - **優勢**：零容器化開銷，節省資源，開機延遲最低。
   - **情境**：實際上線運行、執行高耗能的 SLAM 與導航任務。
2. **「Docker 容器」作為退路機制 (Fallback)**：
   - **優勢**：環境絕對乾淨、隔離，免除編譯環境變數等疑難雜症。
   - **情境**：原生環境損壞時救火、交叉驗證通訊功能，或快速體驗測試。

---

## 1. 硬體與連接埠設定 (Udev 規則)

為了避免每次重開機或插拔 USB 後，通訊埠名稱 (如 `/dev/ttyUSB0`) 發生跳動，我們必須設定 `udev` 規則，將連接 ESP32 的通訊埠永遠固定命名為 `/dev/ollie_core`。

### 建立 Udev 規則：

1. **查詢裝置硬體 ID**（假設裝置目前在 `/dev/ttyUSB0`）：
   ```bash
   udevadm info -a -n /dev/ttyUSB0 | grep '{idVendor}'
   udevadm info -a -n /dev/ttyUSB0 | grep '{idProduct}'
   ```
   *(註：通常會跑出多組，請尋找類似 `1a86` / `7523` 這種晶片特徵碼，忽略 `1d6b` 等 Root Hub 資訊。)*

2. **編輯規則檔案**：
   ```bash
   sudo vim /etc/udev/rules.d/99-ollie-core.rules
   ```
   **【⚠️ 填寫注意：請依照您的連接方式，擇一填寫一行即可】**
   
   - **方案 A (推薦)：使用 USB 轉 TTL 模組** (請將 ID 替換為您剛查到的數值)：
     ```text
     SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", SYMLINK+="ollie_core", MODE="0666"
     ```
   - **方案 B：使用 RPi 實體 GPIO UART 腳位** (例如 TXD/RXD 直接對接)：
     ```text
     KERNEL=="ttyS0", SYMLINK+="ollie_core", MODE="0666"
     ```

3. **套用規則**：
   ```bash
   sudo udevadm control --reload-rules && sudo udevadm trigger
   ls -l /dev/ollie_core  # 確認連結是否已建立
   ```
   > 💡 **小撇步**：如果執行 `ls` 發現找不到檔案，請直接將 USB **拔掉重新插上**，通常系統就會立刻抓到並建立名稱了。

---

## 2. 方案 A：原生編譯與執行 (主力推薦)

請確認主機已安裝 **ROS 2 Jazzy**。

### 2.1 建立與編譯工作區

```bash
# 1. 載入 ROS 2 環境 (建議加入 ~/.bashrc)
source /opt/ros/jazzy/setup.bash

# 2. 進入專案工作區並下載 micro-ROS Setup 工具
cd ~/workspace/AMROllie/host5b/ros2_ws/src
git clone -b jazzy https://github.com/micro-ROS/micro_ros_setup.git
cd ..

# 3. 安裝編譯工具與相依套件，並編譯 Setup 工具
rosdep install --from-paths src --ignore-src -y
colcon build --packages-select micro_ros_setup
source install/local_setup.bash

# 4. 建立並編譯 Agent 專案
ros2 run micro_ros_setup create_agent_ws.sh
ros2 run micro_ros_setup build_agent.sh
source install/local_setup.bash
```


### 2.2 手動啟動原生 Agent (測試連線用)

編譯完成後，您可以透過以下指令手動啟動 Agent。這通常用於除錯或初次連線測試：

```bash
export ROS_DOMAIN_ID=30
# 注意：務必指定與 ESP32 相同的鮑率 -b 115200
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -b 115200 -v4
```

> **💡 連線成功指標**：執行指令後，請按下 ESP32 開發板上的 **EN (Reset)** 按鈕重啟微控制器。若終端機出現綠色的 `Session established` 或建立連線的日誌，代表 ROS 2 已成功與硬體對接！

### 2.3 背景自動啟動 (正式部署)

確認手動連線沒問題後，我們通常不會每次開機都手動輸入指令。
請參考專案中的 `systemd/` 目錄安裝背景服務，之後只要透過以下指令即可在背景啟動與管理：

```bash
# 啟動 micro-ROS 服務
sudo systemctl start ollie_microros.service

# 查看即時連線狀態日誌
sudo journalctl -u ollie_microros.service -f
```

---

## 3. 方案 B：Docker 環境準備與執行 (退路機制)

### 3.1 準備工作

```bash
# 1. 安裝 Docker (若尚未安裝)
curl -fsSL https://get.docker.com -o get-docker.sh && sudo sh get-docker.sh
sudo usermod -aG docker $USER  # 需重新登入套用

# 2. 拉取映像檔
docker pull microros/micro-ros-agent:jazzy
```

### 3.2 執行與控制

- **啟動 Agent**：`./run_docker_micro_ros.sh`
- **手動開啟 Docker 服務**（若已依下方效能優化策略禁用）：`./start_docker_service.sh`

> **⚠️ 提醒**：若要在主機端查看話題，請務必執行 `export ROS_DOMAIN_ID=30`。

---

## 4. 效能優化策略：彻底禁用 Docker 常駐

在 Raspberry Pi 5B 上，若您不使用 Docker，建議平常完全關閉 Docker 服務以釋放資源：

```bash
# 停止並禁用開機自啟
sudo systemctl stop docker.service docker.socket containerd.service
sudo systemctl disable docker.service docker.socket containerd.service
```

需要使用時，執行 `./start_docker_service.sh` 即可手動喚醒。
