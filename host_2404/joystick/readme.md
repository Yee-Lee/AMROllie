# Ollie AMR - PS4 手把遙控開發與部署指南

本文件記錄了 Ollie 專案中，如何設定 PS4 手把並將其訊號轉換為 `/cmd_vel` 控制指令。內容涵蓋了在 UTM 虛擬機的初步測試，以及實際部署至 RPi 5B 實體機（Ubuntu 24.04 / ROS 2 Jazzy）的連線對照、降頻優化與避坑指南。

## 1. 運行環境架構對照

在開發與實際運行的過程中，我們會跨越兩種不同的硬體環境。兩者的軟體指令完全相同，最大差異在於實體連接與延遲表現：

| 項目 | 開發測試環境 (UTM 虛擬機) | 正式部署環境 (RPi 5B 實體機) |
| :--- | :--- | :--- |
| **主要定位** | 驗證 YAML 參數邏輯與 ROS 通訊架構 | 實際安裝於 Ollie 車體，進行實車遙控 |
| **硬體連接** | Mac USB 實體線 ➔ UTM USB 裝置直通 | USB 直連 或 **藍牙無線連線** |
| **作業環境** | 通常具備圖形介面 (Ubuntu Desktop) | 無頭模式 (Headless / Ubuntu Server 24.04) |
| **操作延遲** | **較高** (受限於虛擬機 USB Polling 轉譯) | **極低** (實體直連/藍牙原生 Linux 核心) |
| **推薦套件** | `joy_linux` (避開 SDL2 虛擬機焦點遺失) | `joy_linux` (避開無圖形介面導致的罷工) |

## 2. 硬體與系統層連線測試

### 2.1 USB 實體連接
- **UTM 環境**：將手把接上 Mac 後，點擊 UTM 頂部工具列的 USB 圖示，勾選 `Wireless Controller` 以直通進 Ubuntu。
- **RPi 5B 環境**：直接將手把透過 USB 線插入樹莓派即可。

### 2.2 藍牙無線配對與電源管理 (RPi 5B 實機部署)
⚠️ **注意：為節省 AMR 功耗，Ollie 的系統已設定為「開機預設徹底關閉藍牙硬體與系統服務」。**

**【手動開關藍牙】**
當需要使用遙控時，請手動執行工作區內的腳本：
* 開啟：執行 `enable_bluetooth.sh` (會解除 rfkill 並啟動 bluetooth.service)
* 關閉：執行 `disable_bluetooth.sh` (會徹底斷電並釋放背景資源)

**【初次藍牙配對步驟】**
1. 確保藍牙已開啟，並**拔除手把的 USB 線**。
2. 進入藍牙控制台：`bluetoothctl`
3. 啟動掃描：在提示字元下依序輸入 `agent on`, `default-agent`, `scan on`
4. **讓手把進入配對模式**：同時長按手把上的 **Share 鍵 + PS 鍵** 約 3~5 秒，直到前方 LED 燈條**快速連續閃爍白光**。
5. **連線並信任裝置**：找到名為 `Wireless Controller` 的 MAC 位址（如 `XX:XX:XX:XX:XX:XX`），執行：
   
```bash
   pair XX:XX:XX:XX:XX:XX
   trust XX:XX:XX:XX:XX:XX
   connect XX:XX:XX:XX:XX:XX
   ```
   *連線成功後，燈條會轉為恆亮。輸入 `exit` 離開。因為已設定 `trust`，未來只要開啟系統藍牙服務，按下 **PS 鍵** 即可自動連線。*

### 2.3 系統底層驗證 (Linux)
確認 Ubuntu 核心是否已成功掛載裝置（無論 USB 或藍牙）：
```bash
sudo apt update && sudo apt install joystick -y
sudo chmod a+rw /dev/input/js0
jstest /dev/input/js0
```
*註：若 js0 沒反應，請執行 `ls -l /dev/input/js*` 確認系統分配的裝置編號。Linux 通常會為 PS4 手把分配兩個裝置（一個是搖桿，一個是觸控板/陀螺儀）。*

## 3. ROS 2 遙控節點配置 (避坑重點區)

### 3.1 安裝核心套件
強烈建議使用直接讀取 Linux 底層的 `joy_linux`。
*💡 **相依性地雷：** Ubuntu 24.04 安裝 Jazzy joy 套件時，極易遇到 `libdbus-1-dev` 等底層套件版本衝突 (Held broken packages)。請使用 `aptitude` 進行降級安裝：*
```bash
sudo apt install aptitude -y
sudo aptitude install ros-jazzy-joy-linux ros-jazzy-teleop-twist-joy
# 遇到方案選項時，拒絕第一套(不變動)，接受第二套(Downgrade降級)方案。
```

### 3.2 配置文件 `ps4_config.yaml` 參數重點
設定檔主要負責定義 PS4 手把在 Ubuntu 下的映射邏輯（L1 解鎖、雙搖桿速度映射）與**發布頻率限制**。
*💡 **名稱地雷：** 絕對不能使用 `/**:` 萬用字元！這會導致參數無法注入。在 ROS 2 Jazzy 中，必須精確指定 `/teleop_twist_joy_node`。*
*💡 **頻率同步：** 請確保設定檔內的 `autorepeat_rate` 設為 `10.0` (10 Hz)，與硬體層的發布頻率對齊，保護下位機 ESP32 不被訊號塞爆。*

## 4. 啟動控制流程

### 4.1 啟動腳本與硬體降頻 (`start_joystick.sh`)
PS4 手把內建的高靈敏度陀螺儀會以高達 250 Hz 的頻率瘋狂發送微小震動，若不加上硬體層的合併延遲，將會直接塞爆 ESP32 的通訊緩衝區導致嚴重掉包與頓挫。

執行腳本時，務必確保 `joy_linux_node` 已帶入以下關鍵防護參數：
* **`coalesce_interval:=0.1`**：硬體合併為 10Hz，完美對齊 YAML 的軟體轉發設定。
* **`deadzone:=0.2`**：過濾陀螺儀帶來的微小高頻雜訊。

執行方式：只要手把連上線，執行啟動腳本即可。結束時按下 `Ctrl+C` 會自動安全地關閉所有背景節點。

## 5. 常見問題排除 (Troubleshooting)

1. **下位機 ESP32 嚴重掉包，Ollie 移動卡頓 (陀螺儀詛咒)**
   - **徵狀**：使用 `ros2 topic hz /cmd_vel` 測量時，發現頻率飆升至 100~250 Hz，或出現 `min: 0.000s` 的極端連發。
   - **解決**：請確認啟動腳本中的 `coalesce_interval:=0.1`（單位為秒，勿加 `_ms`）與 YAML 中的 `/teleop_twist_joy_node` 名稱是否正確套用。
2. **推動搖桿時 cmd_vel 沒反應，或按鍵邏輯錯亂**
   - **原因**：YAML 參數檔注入失敗，系統載入了預設的 XBOX 邏輯。
   - **解決**：檢查 YAML 第一行是否為正確的節點名稱 `/teleop_twist_joy_node`（嚴禁使用 `/**:`）。
3. **jstest 讀得到數值，但 ros2 topic echo /joy 沒反應**
   - **原因**：誤用到依賴 SDL2 的預設 `joy_node`，導致在無桌面環境中遺失視窗焦點而罷工。
   - **解決**：確認腳本中使用的是 `joy_linux_node`。
4. **安裝 ROS joy 套件時出現 "Held broken packages" 錯誤**
   - **解決**：參考 3.1 節，放棄使用 `apt`，改用 `aptitude` 進行自動降級解析。

---
*Created for Project Ollie (AMR).*
```
