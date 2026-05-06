# 建立原生 micro-ROS Agent 執行環境

在資源受限的平台上（如 Raspberry Pi 3B），使用 Docker 執行 micro-ROS Agent 可能會佔用過多記憶體並增加開機時間。因此，強烈建議使用原生編譯（Native Build）的方式來執行。

本指南將說明如何在安裝了 ROS 2 Humble 的 Ubuntu 22.04 環境中，原生編譯與執行 micro-ROS Agent。

## 1. 系統需求與環境準備

請確認您的主機已經安裝了 **ROS 2 Humble** 桌面版或基礎版（`ros-humble-desktop` 或 `ros-humble-ros-base`）。
若尚未安裝 ROS 2，請參考 [ROS 2 官方安裝指南](https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debians.html)。

設定 ROS 2 環境變數（建議將此行加入 `~/.bashrc`）：
```bash
source /opt/ros/humble/setup.bash
```

## 2. 建立工作區與下載原始碼

請在您的使用者目錄下建立一個專屬的 ROS 2 工作區（例如 `~/microros_ws`）：

```bash
# 建立工作區目錄
mkdir -p ~/microros_ws/src
cd ~/microros_ws/src

# 複製 micro-ROS 建置系統的程式碼
git clone -b humble https://github.com/micro-ROS/micro_ros_setup.git
```

## 3. 安裝相依套件並更新 rosdep

使用 `rosdep` 安裝 `micro_ros_setup` 需要的系統相依套件：

```bash
cd ~/microros_ws

# 更新 rosdep 索引
sudo apt update && rosdep update

# 安裝相依套件
rosdep install --from-paths src --ignore-src -y
```

## 4. 編譯 micro-ROS Setup

使用 `colcon` 編譯剛剛下載的 `micro_ros_setup` 工具：

```bash
cd ~/microros_ws
colcon build

# 載入剛編譯好的環境
source install/local_setup.bash
```

## 5. 建立與編譯 micro-ROS Agent

透過 `micro_ros_setup` 工具，建立並編譯 Agent 工作區。

### 5.1 建立 Agent 專案
```bash
# 這將會下載 micro-ROS Agent 的原始碼
ros2 run micro_ros_setup create_agent_ws.sh
```

### 5.2 編譯 Agent
```bash
# 這個步驟可能需要數分鐘，視硬體效能而定
ros2 run micro_ros_setup build_agent.sh
```

完成後，請再次載入環境變數以套用編譯好的 Agent：
```bash
source install/local_setup.bash
```

## 6. 執行原生的 micro-ROS Agent

為了讓主機能與下位機（如 ESP32）順利通訊，請務必先確認[設定 udev 規則](./readme.md#1-硬體與連接埠設定-udev-規則)將序列埠映射為 `/dev/ollie_core`。

同時，為避免網路干擾，請確保設定了正確的 `ROS_DOMAIN_ID`：
```bash
export ROS_DOMAIN_ID=30
```

最後，執行 Agent 啟動序列埠通訊（預設鮑率為 115200 或請依據您 ESP32 的設定調整）：
```bash
# 語法：ros2 run micro_ros_agent micro_ros_agent serial --dev <裝置路徑>
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -v6
```
（`-v6` 參數開啟了詳細日誌輸出，方便確認連線狀態與話題傳輸，熟悉後可移除）

---

> **💡 提示：自動化執行**
> 若您希望在開機時自動執行這個原生的 Agent，您可以將上述 `source` 環境變數與 `ros2 run` 的指令封裝成 Shell 腳本，並搭配 `systemd` 服務（如 `host/systemd/` 目錄下的設定）來實現開機自啟。