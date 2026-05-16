# 🤖 AMROllie 上位機系統服務與架構指南

本指南記錄了 AMROllie 專案在上位機 (RPI3B, Ubuntu 22.04 + ROS 2 Humble) 的核心背景服務架構。透過 `systemd` 與自動化腳本的整合，我們賦予了機器人「開機自動啟動」、「崩潰自動重啟」以及「通訊假死主動修復」的能力。

## 🏗️ 系統服務架構總覽

Ollie 的上位機大腦被拆分為多個獨立的背景服務與防護機制，彼此解耦以確保最高穩定性：

*   **核心通訊層 (`ollie_microros.service`)**
    *   負責啟動 `micro_ros_agent`，作為 ROS 2 與下位機 (ESP32) 溝通的橋樑。
    *   依賴硬體設備：`/dev/ollie_core`。
*   **狀態與視覺層 (`ollie_description.service`)**
    *   發布機器人的 URDF 模型、TF 座標轉換。
    *   啟動 Foxglove Bridge (Port 8765) 供遠端除錯連線。
*   **感測器層 (`ollie_lidar.service`)**
    *   啟動 LD19 光達驅動節點。
*   **主動免疫防護網 (Watchdog Cronjob)**
    *   負責監控系統與通訊健康狀態，發生異常時主動介入救援。

---

## 🛡️ 主動看門狗機制 (Active Watchdog)

即使服務設定了 `Restart=always`，有時底層通訊（例如 Serial buffer）卡死但進程仍在執行，會導致所謂的「假死」狀態。為了解決這個問題，我們實作了**雙重檢查機制**的看門狗。

**防護邏輯 (`ollie_watchdog.sh`)：**
1.  **快篩 (Service Check)**：檢查 `ollie_microros.service` 是否存活，若停止則立即重啟。
2.  **深蹲 (Functional Check)**：若服務存活，嘗試讀取 `/odom` topic 3 秒鐘。若超時或無數據，判定為通訊假死並強制重啟。

### 部署看門狗 (Systemd Timer 做法)
本專案採用 Systemd Timer 取代傳統 Cronjob，以獲得更好的整合性與監控能力：

1. **建立軟連結**：
   ```bash
   sudo ln -s ~/Workspace/AMROllie/host/systemd/system/ollie_watchdog.service /etc/systemd/system/
   sudo ln -s ~/Workspace/AMROllie/host/systemd/system/ollie_watchdog.timer /etc/systemd/system/
   sudo systemctl daemon-reload
   ```
2. **啟動並設定開機自啟**：
   ```bash
   sudo systemctl enable --now ollie_watchdog.timer
   ```
3. **檢查狀態**：可使用 `ollie_status.sh` 查看，或手動執行 `systemctl status ollie_watchdog.timer`。
4. **日誌追蹤**：異常重啟紀錄會寫入 `/var/log/ollie_watchdog.log`，也可透過 `journalctl -u ollie_watchdog.service` 查看執行歷史。

---

## 🛠️ Systemd 部署步驟

為了方便版控與維護，我們將 `.service` 設定檔集中在專案目錄 `system/` 下，並使用**軟連結 (Symbolic Link)** 將其註冊到系統中。

### 1. 建立服務軟連結
```bash
sudo ln -s ~/Workspace/AMROllie/host/systemd/system/ollie_microros.service /etc/systemd/system/
sudo ln -s ~/Workspace/AMROllie/host/systemd/system/ollie_description.service /etc/systemd/system/
sudo ln -s ~/Workspace/AMROllie/host/systemd/system/ollie_lidar.service /etc/systemd/system/

# 註冊完成後，重新載入 systemd 守護行程
sudo systemctl daemon-reload
```

*(附註：各服務的詳細設定檔內容，請直接參考 `system/` 目錄下的 `.service` 檔案。)*

---

## 🚀 系統管理指令 (Cheatsheet)

### 服務操作
將 `<service_name>` 替換為上述的服務名稱（例如 `ollie_microros.service`）：

* **設定開機自動啟動**：`sudo systemctl enable <service_name>`
* **取消開機自動啟動**：`sudo systemctl disable <service_name>`
* **手動啟動服務**：`sudo systemctl start <service_name>`
* **手動停止服務**：`sudo systemctl stop <service_name>`
* **重新啟動服務**：`sudo systemctl restart <service_name>`
* **查看當前狀態**：`sudo systemctl status <service_name>` (按 `q` 退出)

### 🕵️ 日誌與除錯 (Debug)
因為背景服務不會輸出畫面到終端機，需依賴 `journalctl` 查看運行狀況：

* **即時監聽日誌** (類似 `tail -f`，按 `Ctrl+C` 退出)：
  ```bash
  sudo journalctl -u <service_name> -f
  ```
* **查看該服務的所有歷史紀錄**：
  ```bash
  sudo journalctl -u <service_name> --no-pager
  ```
