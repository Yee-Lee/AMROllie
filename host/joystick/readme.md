# Ollie AMR - PS4 手把遙控開發與部署指南

本文件記錄了 Ollie 專案中，如何設定 PS4 手把並將其訊號轉換為 `/cmd_vel` 控制指令。內容涵蓋了在 UTM 虛擬機的初步測試，以及實際部署至 RPI3B 實體機的 USB 與藍牙連線對照指南。

## 1. 運行環境架構對照

在開發與實際運行的過程中，我們會跨越兩種不同的硬體環境。兩者的軟體指令完全相同，最大差異在於實體連接與延遲表現：

| 項目 | 開發測試環境 (UTM 虛擬機) | 正式部署環境 (RPI3B 實體機) |
| :--- | :--- | :--- |
| **主要定位** | 驗證 YAML 參數邏輯與 ROS 通訊架構 | 實際安裝於 Ollie 車體，進行實車遙控 |
| **硬體連接** | Mac USB 實體線 ➔ UTM USB 裝置直通 | USB 直連 或 **藍牙無線連線** |
| **作業環境** | 通常具備圖形介面 (Ubuntu Desktop) | 無頭模式 (Headless / Ubuntu Server) |
| **操作延遲** | **較高** (受限於虛擬機 USB Polling 轉譯) | **極低** (實體直連/藍牙原生 Linux 核心) |
| **推薦套件** | `joy_linux` (避開 SDL2 虛擬機焦點遺失) | `joy_linux` (避開無圖形介面導致的罷工) |

## 2. 硬體與系統層連線測試

### 2.1 USB 實體連接
- **UTM 環境**：將手把接上 Mac 後，點擊 UTM 頂部工具列的 USB 圖示，勾選 `Wireless Controller` 以直通進 Ubuntu。
- **RPI3B 環境**：直接將手把透過 USB 線插入樹莓派即可。

### 2.2 藍牙無線配對 (RPI3B 實機部署推薦)
在實車部署時，為了達到真正的無線遙控，請依循以下步驟進行藍牙配對：
1. **拔除手把的 USB 線**。
2. 進入藍牙控制台：`bluetoothctl`
3. 啟動掃描：在提示字元下依序輸入 `agent on`, `default-agent`, `scan on`
4. **讓手把進入配對模式**：同時長按手把上的 **Share 鍵 + PS 鍵** 約 3~5 秒，直到前方 LED 燈條**快速連續閃爍白光**。
5. **連線並信任裝置**：在終端機找到名為 `Wireless Controller` 的 MAC 位址（格式如 `XX:XX:XX:XX:XX:XX`），依序執行以下指令：
   ```bash
   pair XX:XX:XX:XX:XX:XX
   trust XX:XX:XX:XX:XX:XX
   connect XX:XX:XX:XX:XX:XX
   ```
   *連線成功後，手把燈條會轉為恆亮（通常為藍色）。輸入 `exit` 離開控制台。*
6. **自動連線機制**：因已設定 `trust`，未來只要 RPI3B 開機，隨時按下 **PS 鍵** 即可自動喚醒並連線，無需重新配對。

### 2.3 系統底層驗證 (Linux)
在開始 ROS 測試前，需確認 Ubuntu 核心是否已成功掛載硬體裝置（無論是 USB 或藍牙）。

```bash
# 安裝搖桿測試工具
sudo apt update && sudo apt install joystick -y

# 賦予裝置最高讀寫權限 (若 jstest 報錯時執行)
sudo chmod a+rw /dev/input/js0

# 進行即時數值測試 (確認按鍵與搖桿正常反應後，按 Ctrl+C 退出)
jstest /dev/input/js0
```
*註：若 js0 沒反應，請執行 `ls -l /dev/input/js*` 確認系統分配的裝置編號 (如 js1, js2)。*

## 3. ROS 2 遙控節點配置

### 3.1 安裝核心套件
為了確保在 RPI3B 的無頭模式 (Headless) 下能穩定運作，強烈建議捨棄預設的 `joy` 套件，改用直接讀取 Linux 底層的 `joy_linux`。
```bash
sudo apt install ros-humble-joy-linux ros-humble-teleop-twist-joy
```

### 3.2 建立配置文件 `ps4_config.yaml`
針對 PS4 手把在 Ubuntu 下的映射邏輯進行設定：
- **L1 鍵**: 安全解鎖按鈕 (Deadman Switch) ➔ **Button 4**
- **左搖桿 (上下)**: 線速度 v ➔ **Axis 2**
- **右搖桿 (左右)**: 角速度 ω ➔ **Axis 3**
```yaml
/**:
  ros__parameters:
    # 安全機制：必須按住此按鍵 (L1) 才能發送 cmd_vel 指令
    require_enable_button: true
    enable_button: 4             # PS4 L1 鍵索引

    # 線速度設定 (Linear)
    axis_linear:
      x: 2                       # 左搖桿上下 (Axis 2)
    scale_linear:
      x: 0.5                     # 最大線速度 0.5 m/s

    # 角速度設定 (Angular)
    axis_angular:
      yaw: 3                     # 右搖桿左右 (Axis 3)
    scale_angular:
      yaw: 1.0                   # 最大角速度 1.0 rad/s
```
*💡 關鍵技巧：頂層使用 `/**:` 萬用字元，可強制覆蓋設定，避免因執行檔與內部節點名稱不符導致參數被忽略。*

## 4. 啟動控制流程

### 4.1 使用一鍵啟動腳本 (推薦)
為了避免每次都需要開啟多個終端機輸入冗長的指令，並妥善處理 ROS 2 節點的關閉流程以避免 C++ Exception 報錯，建議建立一個 Bash 腳本 (`start_joystick.sh`)。

建立檔案並賦予執行權限 (`chmod +x start_joystick.sh`)：
```bash
#!/bin/bash

echo "🎮 啟動 Ollie 的 PS4 手把遙控節點..."

# 取得目前腳本所在的絕對路徑，確保 yaml 檔能被正確找到
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
CONFIG_FILE="$SCRIPT_DIR/ps4_config.yaml"

# 啟動 joy_linux_node 在背景執行
ros2 run joy_linux joy_linux_node --ros-args -p dev:=/dev/input/js0 &
JOY_PID=$!

# 啟動 teleop_node 在背景執行
ros2 run teleop_twist_joy teleop_node --ros-args --params-file "$CONFIG_FILE" &
TELEOP_PID=$!

echo "✅ 節點已啟動！ (PID: $JOY_PID, $TELEOP_PID)"
echo "🛑 按下 Ctrl+C 即可安全關閉遙控功能。"

# 捕捉 Ctrl+C (SIGINT)，改用 kill -INT 傳送標準終止訊號，讓 ROS 2 有時間跑完清理流程
trap "echo -e '\n正在安全關閉遙控節點...'; kill -INT $JOY_PID $TELEOP_PID 2>/dev/null; wait $JOY_PID $TELEOP_PID 2>/dev/null; exit" SIGINT

# 讓腳本保持執行狀態，等待訊號
wait $JOY_PID $TELEOP_PID
```
**執行方式**：只要手把連上線，執行 `./start_joystick.sh` 即可。結束時按下 `Ctrl+C` 會自動乾淨地關閉所有背景節點。

### 4.2 手動逐一啟動 (除錯用)
若需確認單一節點狀態，可分開三個終端機執行：
1. **讀取硬體**：`ros2 run joy_linux joy_linux_node --ros-args -p dev:=/dev/input/js0`
2. **轉換指令**：`ros2 run teleop_twist_joy teleop_node --ros-args --params-file ps4_config.yaml`
3. **監聽輸出**：`ros2 topic echo /cmd_vel`

**測試動作**：死命按住 **L1 (Button 4)** 不放，同時推動左搖桿(上下)與右搖桿(左右)，觀察 `/cmd_vel` 是否印出對應的 `linear.x` 與 `angular.z` 數值。

## 5. 常見問題排除 (Troubleshooting)
1. **jstest 讀得到數值，但 ros2 topic echo /joy 沒反應**
   - **原因**：誤用到依賴 SDL2 的 `joy_node`，導致在虛擬機或無桌面環境中遺失視窗焦點而罷工。
   - **解決**：請確認執行的是 `joy_linux_node`。
2. **推動搖桿時 cmd_vel 沒反應，或按鍵邏輯錯亂**
   - **原因**：YAML 參數檔注入失敗，系統載入了預設的 XBOX 邏輯。
   - **解決**：確認 `ps4_config.yaml` 頂層為 `/**:`，並確認啟動時有正確加上 `--params-file` 參數。若使用啟動腳本，請確認 yaml 檔與腳本放置於同一個資料夾。
3. **在 UTM 測試時感覺搖桿延遲很長**
   - **原因**：跨系統的 USB Passthrough 輪詢延遲。
   - **解決**：此為虛擬機硬傷。轉移至 RPI3B 實體機並使用藍牙連線後，延遲問題將自動消失。

---
*Created for Project Ollie (AMR).*
