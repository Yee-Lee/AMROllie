# 🤖 AMROllie Systemd 部署指南

本指南記錄了如何將 Ollie (RPI3B 上位機) 的核心 ROS 2 服務寫入 Linux 的 `systemd`，使其具備「開機自動啟動」與「崩潰自動重啟」的能力。

## 架構總覽

Ollie 的大腦被拆分為三個獨立的背景服務，彼此解耦，確保最高穩定性：
1. **[已完成]** `ollie_microros.service`：負責啟動 micro-ROS agent (推薦使用原生編譯版)，橋接下位機 ESP32。
2. **[已完成]** `ollie_description.service`：啟動 `ollie_visual.launch.py`，負責發布 URDF 模型、TF 座標轉換，以及開啟 Foxglove Bridge (Port 8765)。
3. **[已完成]** `ollie_lidar.service`：負責啟動 LD19 光達驅動。

---

## 🛠️ 部署步驟

為了方便版控與修改，建議將設定檔維護在專案目錄 (`~/workspace/AMROllie/host/systemd/system/`) 中，再透過**軟連結 (Symbolic Link)** 指向 `/etc/systemd/system/`。
這樣未來每次修改檔案內容後，就不需要重新複製，只要執行 `sudo systemctl daemon-reload` 即可。

**建立軟連結的指令：**
```bash
sudo ln -s ~/workspace/AMROllie/host/systemd/system/ollie_microros.service /etc/systemd/system/
sudo ln -s ~/workspace/AMROllie/host/systemd/system/ollie_description.service /etc/systemd/system/
sudo ln -s ~/workspace/AMROllie/host/systemd/system/ollie_lidar.service /etc/systemd/system/

# 建立完成後重新載入 systemd
sudo systemctl daemon-reload
```

### 步驟 1：部署 Micro-ROS 通訊橋樑 (已完成)

**檔案路徑**：`~/workspace/AMROllie/host/systemd/system/ollie_microros.service`

```ini
[Unit]
Description=Ollie Micro-ROS Agent (ESP32 Bridge)
After=network.target

[Service]
Type=simple
User=yee
Environment="ROS_DOMAIN_ID=30"
Restart=always
RestartSec=3
# 啟動原生編譯的 micro-ROS agent，使用 udev 綁定的 /dev/ollie_core
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && source /home/yee/micro_ros_ws/install/local_setup.bash && ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ollie_core -v4"

[Install]
WantedBy=multi-user.target
```

### 步驟 2：部署 Ollie Description 機器人狀態發布 (已完成)

**檔案路徑**：`~/workspace/AMROllie/host/systemd/system/ollie_description.service`

```ini
[Unit]
Description=Ollie Robot State Publisher (URDF/TF)
After=network.target

[Service]
Type=simple
User=yee
Environment="ROS_DOMAIN_ID=30"
# 透過 bash -c 串聯環境變數 source 與啟動指令 (包含 TF、Foxglove 橋接與實體連線)
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && source /home/yee/workspace/AMROllie/host/ros2_ws/install/setup.bash && ros2 launch ollie_description ollie_visual.launch.py"
Restart=on-failure
RestartSec=3

[Install]
WantedBy=multi-user.target
```

### 步驟 3：部署 LD19 光達驅動 (待完成)

**檔案路徑**：`~/workspace/AMROllie/host/systemd/system/ollie_lidar.service`

```ini
[Unit]
Description=Ollie LD19 LiDAR Driver
After=network.target systemd-udevd.service
Wants=systemd-udevd.service

[Service]
Type=simple
User=yee
Environment="ROS_DOMAIN_ID=30"
# 確保 udev 載入硬體權限後才啟動
ExecStart=/bin/bash -c "source /opt/ros/humble/setup.bash && source /home/yee/workspace/AMROllie/host/ros2_ws/install/setup.bash && ros2 ld19 start"
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

---

## 🚀 系統管理指令 (Cheatsheet)

每次新增或修改 `.service` 檔案後，**務必**執行以下指令通知系統：
```bash
sudo systemctl daemon-reload
```

### 服務操作
將 `<service_name>` 替換為上述的三個服務名稱（例如 `ollie_microros.service`）：

* **設定開機自動啟動**：
  ```bash
  sudo systemctl enable <service_name>
  ```
* **取消開機自動啟動**：
  ```bash
  sudo systemctl disable <service_name>
  ```
* **手動啟動服務**：
  ```bash
  sudo systemctl start <service_name>
  ```
* **手動停止服務**：
  ```bash
  sudo systemctl stop <service_name>
  ```
* **重新啟動服務**：
  ```bash
  sudo systemctl restart <service_name>
  ```
* **查看服務當前狀態** (按 `q` 退出)：
  ```bash
  sudo systemctl status <service_name>
  ```

### 🕵️ 日誌除錯 (Debug)
因為背景服務不會輸出畫面到終端機，需依賴 `journalctl` 查看運行狀況：

* **即時監聽服務日誌** (類似 `tail -f`，按 `Ctrl+C` 退出)：
  ```bash
  sudo journalctl -u <service_name> -f
  ```
* **查看特定服務從開機到現在的所有日誌**：
  ```bash
  sudo journalctl -u <service_name> --no-pager
  ```
