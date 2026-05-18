# 🤖 AMROllie 上位機系統部署與防護指南

本指南旨在協助開發者將 AMROllie 的上位機 (RPi 5B) 環境轉化為一個**高可靠性、具備自我修復能力**的機器人系統。

## 🎯 我們的目標與方法

### 為什麼使用 Systemd？
在機器人開發中，我們不希望每次開機都要手動開啟多個終端機。透過 `systemd`，我們實現了：
1.  **開機自啟 (Autostart)**：電源開啟後，核心通訊與感測器自動啟動。
2.  **崩潰復原 (Crash Recovery)**：若程式意外關閉，系統會立即自動重新拉起服務。
3.  **狀態統一管理**：透過標準指令監控所有背景程序的健康狀況。

### 部署方法：自動化範本生成 (Template Generation)
為了保持 GitHub 專案的乾淨，避免推入個人的絕對路徑，我們採用了**「範本化」**設計。
`systemd/system/` 目錄下存放的是 `.template` 檔案。部署時，透過 `install_services.sh` 腳本，系統會自動抓取當前的使用者名稱與絕對路徑，生成真實的 `.service` 檔案並註冊到 Linux 系統中。*(註：生成的實體檔案已被 `.gitignore` 忽略，不會汙染 Git 歷史)*。

---

## 🛠️ 第一階段：一鍵安裝服務與防護網

這次的部署非常簡單，所有繁瑣的路徑替換與權限設定皆由腳本自動處理。

### 1. 執行安裝腳本
請在終端機中導航到 `systemd` 目錄，然後執行安裝腳本：
```bash
cd ~/workspace/AMROllie/host5b/systemd
./install_services.sh
```
*腳本執行期間會請求 `sudo` 權限以建立系統軟連結與重新載入 systemd。*

### 2. 啟動核心服務
安裝完成後，請手動啟用並啟動核心的三大服務（Micro-ROS、模型發布、光達）：
```bash
sudo systemctl enable --now ollie_microros.service
sudo systemctl enable --now ollie_description.service
sudo systemctl enable --now ollie_lidar.service
```

### 3. 啟動主動看門狗 (Active Watchdog)
即使服務正在運行，通訊也可能因 Serial 緩存溢位而「假死」。看門狗負責主動偵測 `/odom` 數據，並在異常時強制重啟通訊。
啟動看門狗服務：
```bash
sudo systemctl enable --now ollie_watchdog.service
```

---

## 🔍 第二階段：檢查運行狀態與日誌

部署完成後，請依序執行以下檢查確保系統運作正常。

### 1. 綜合狀態快檢
執行專案提供的狀態腳本，一目了然所有服務與防護網的健康狀況：
```bash
./ollie_status.sh
```
*   🟢 **active**：代表服務正常運行中。
*   🛡️ **active**：代表看門狗防護網已啟動。
*   ❓ **unknown (activating)**：代表服務正在啟動中（若卡住請檢查硬體連線）。

### 2. 查驗服務日誌 (Debug)
若服務顯示 `failed` 或 `unknown`，請查看即時系統日誌尋找錯誤原因：
```bash
# 查看 Micro-ROS Agent 的通訊細節
sudo journalctl -u ollie_microros.service -f

# 查看看門狗的定期巡邏紀錄
sudo journalctl -u ollie_watchdog.service -f
```

#### 💡 如何驗證看門狗是否正常接收到 Odom？
您可以透過觀察 `ollie_watchdog.service` 的即時日誌來確認：
*   **正常接收時**：啟動後若收到第一筆資料，會出現 `✅ 系統通訊正常！開始接收 /odom 數據。`。
*   **通訊中斷時**：會顯示 `⚠️ 已經 X.X 秒未收到 /odom 資料`，並觸發重啟。
*   **手動驗證**：在另一個終端機執行 `ros2 topic echo /odom`。若此處有資料但看門狗仍報錯，請檢查 `ROS_DOMAIN_ID` 是否皆設定為 `30`。

### 3. 查看看門狗異常重啟紀錄
看門狗只會在「抓到異常並執行重啟」時將事件紀錄到專屬日誌中，這有助於事後分析系統穩定性：
```bash
cat /var/log/ollie_watchdog.log
```

---
*💡 提示：在關閉機器人電源或拔除 ESP32 進行離線調試前，建議暫時關閉看門狗以避免無意義的重啟日誌：`sudo systemctl stop ollie_watchdog.service`*

---

## 📖 附錄：底層部署原理與手動安裝

雖然我們提供了 `install_services.sh` 進行一鍵安裝，但了解底層機制有助於日後除錯或進行深度客製化。

如果腳本無法運行，或您希望手動部署，請遵循以下步驟：

### 1. 手動轉換範本 (Template -> Service)
在 `systemd/system/` 目錄中，所有的檔案皆為 `.template`。您需要手動將其複製並重新命名，去除 `.template` 後綴。
```bash
cd ~/workspace/AMROllie/host5b/systemd/system
cp ollie_microros.service.template ollie_microros.service
# 依此類推複製所有需要的檔案...
```

### 2. 編輯路徑與權限
打開您剛建立好的 `.service` 檔案，手動進行以下修改：
*   **使用者名稱**：找到 `User=<your_username>`，將其替換為您目前的 Linux 帳號（例如 `User=ollie`）。
*   **絕對路徑**：找到 `ExecStart=` 內的 `/home/<your_username>/workspace/AMROllie/host5b/`，將其替換為專案在您機器上的真實絕對路徑。

*(對於 `ollie_watchdog.service` 也需進行相同替換。)*

### 3. 手動建立軟連結 (Symbolic Link)
為了讓 `systemd` 能夠識別這些服務，您必須將修改好的 `.service` 檔案軟連結到 `/etc/systemd/system/` 目錄下：
```bash
# 建立軟連結
sudo ln -s ~/workspace/AMROllie/host5b/systemd/system/ollie_microros.service /etc/systemd/system/
sudo ln -s ~/workspace/AMROllie/host5b/systemd/system/ollie_description.service /etc/systemd/system/
sudo ln -s ~/workspace/AMROllie/host5b/systemd/system/ollie_lidar.service /etc/systemd/system/
# 如果要部署看門狗：
sudo ln -s ~/workspace/AMROllie/host5b/systemd/system/ollie_watchdog.service /etc/systemd/system/
```

### 4. 重新載入 Systemd
每次建立軟連結或修改 `.service` 檔案內容後，都**必須**執行以下指令通知系統更新：
```bash
sudo systemctl daemon-reload
```
完成後，您即可使用 `sudo systemctl start <service_name>` 來啟動服務。
