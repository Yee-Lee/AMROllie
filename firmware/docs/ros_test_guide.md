# ESP32 micro-ROS 韌體測試指南

本指南說明如何透過 ROS 2 標準工具，測試 ESP32 底層控制器 (Base Controller) 的各項微 ROS (micro-ROS) 功能。請準備一台安裝有 ROS 2 (Humble) 的 Linux 電腦或 Raspberry Pi，並透過 USB 連接 ESP32。

## 階段 1：編譯與燒錄

1. 使用 PlatformIO 開啟專案。
2. 選擇 `env:main` 環境進行編譯 (Build) 與燒錄 (Upload)。
3. 確認燒錄成功後，將 ESP32 透過 USB 連接至上位機。

## 階段 2：建立連線與啟動 micro-ROS Agent

ESP32 (Client) 必須透過 micro-ROS Agent 才能將資料橋接進入 ROS 2 的世界。根據您的測試環境，分為以下兩種連線模式：

### 模式 A：使用 PC 透過 USB 測試 (目前程式碼預設)
此模式適合開發階段，直接使用燒錄用的 USB 線連接 PC 進行測試。目前 `taskRos.cpp` 中的傳輸層已預設為 `set_microros_serial_transports(Serial);`。

1. 在 PC (Linux) 上給予 USB 序列埠權限 (請根據實際掛載修改，如 `/dev/ttyUSB0`)：
   ```bash
   sudo chmod 777 /dev/ttyUSB0
   ```
2. 透過 Docker 啟動 Agent：
   ```bash
   docker run -it --rm -v /dev:/dev --privileged microros/micro-ros-agent:humble serial --dev /dev/ttyUSB0 -v6
   ```

### 模式 B：使用 Raspberry Pi 實際連線測試 (硬體 UART)
當 ESP32 實際安裝至車體並與 Raspberry Pi 對接時，雙方不再透過 USB，而是透過 GPIO 進行 UART 通訊。

1. **修改程式碼**：將 `src/taskRos.cpp` 中的傳輸層設定修改為 `set_microros_serial_transports(Serial2);`，然後重新編譯並燒錄至 ESP32。
2. **硬體接線**：將 ESP32 的 Pin 16 (RX2) 接至 RPi 的 TX 腳位，Pin 17 (TX2) 接至 RPi 的 RX 腳位，並且**務必將兩者的 GND (地線) 相連**。
3. **啟動 Agent**：在 RPi 上啟動 Agent (此處以 RPi 預設的硬體序列埠 `/dev/serial0` 為例，依據您的 RPi 系統設定實際掛載點可能為 `/dev/ttyAMA0` 或 `/dev/serial0`)：
   ```bash
   sudo chmod 777 /dev/serial0
   docker run -it --rm -v /dev:/dev --privileged microros/micro-ros-agent:humble serial --dev /dev/serial0 -v6
   ```

> **提示**：Agent 啟動後，按一下 ESP32 板子上的 `EN` 或 `RST` 按鈕重新啟動。若連線成功，您會在終端機看到 Agent 建立 session 的綠色文字訊息。

## 階段 3：確認 ROS 2 節點與主題

開啟**另一個**已經 source 過 ROS 2 Humble 環境的終端機。

1. **檢查節點是否上線：**
   ```bash
   ros2 node list
   ```
   您應該要看到 `/base_controller_node`。

2. **檢查主題 (Topics) 是否建立：**
   ```bash
   ros2 topic list
   ```
   您應該會看到 `/odom`、`/sonar/left`、`/sonar/right`，以及 `/cmd_vel`。

## 階段 4：測試上行通訊 (感測器與里程計回傳)

驗證 ESP32 是否有穩定地將硬體資料拋上來 (10Hz)。

1. **測試超音波：**
   ```bash
   ros2 topic echo /sonar/left
   ```
   用手在左邊超音波感測器前揮動，觀察終端機印出的 `range` 數值是否會跟著改變。

2. **測試里程計與 IMU：**
   ```bash
   ros2 topic echo /odom
   ```
   嘗試用手去轉動馬達輪子，或是拿著車體原地旋轉，觀察 `pose.pose.position.x` 以及 `pose.pose.orientation.z` (四元數) 是否有相應的變化。

## 階段 5：測試下行通訊 (馬達控制與服務)

驗證 ESP32 是否能正確接收上位機的指令並驅動馬達。

1. **發送速度指令 (測試 `/cmd_vel`)：**
   **請將車子架空以免暴衝。** 在終端機輸入以下指令讓車子以 0.1 m/s 前進：
   ```bash
   ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" -1
   ```
   觀察兩個輪子是否開始向前旋轉。
   要讓馬達停止，則發送全為 0 的指令：
   ```bash
   ros2 topic pub /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.0, y: 0.0, z: 0.0}, angular: {x: 0.0, y: 0.0, z: 0.0}}" -1
   ```

2. **測試重置里程計服務 (測試 `/reset_odom`)：**
   當車子經過移動後，`/odom` 的數值已經不是 0，呼叫這個服務將它歸零：
   ```bash
   ros2 service call /reset_odom std_srvs/srv/Trigger
   ```
   接著再執行 `ros2 topic echo /odom` 檢查看看數值是否都回到 `0.0` 了。

---

### 常見除錯提示
* 如果 Agent 連不上，通常是 Baudrate 問題或線材品質不佳。目前的傳輸是透過預設 Serial (Baudrate: `115200`)。
* 如果馬達轉動有異常抖動或不順，可以回頭微調 `src/baseController/config.h` 裡面的 `MOTOR_PID_KP/KI/KD` 參數。

### 🛠 PC 環境測試注意事項

1. **Docker 鏡像名稱必須「全小寫」**
   Docker Hub 的規範非常嚴格，指令中的 Repository 名稱不能包含大寫字母。
   * 錯誤範例：`microros/micro-ROS-agent` (會導致 `invalid reference format` 錯誤)
   * 正確範例：`microros/micro-ros-agent`

2. **虛擬機 (VM) 或 WSL 的 USB 掛載**
   USB 裝置在物理上具有排他性，一次只能給一個系統使用。若您是在 Windows/macOS 透過 Linux 虛擬機 (如 VirtualBox, VMWare) 進行測試：
   * **確保接管**：必須在虛擬機軟體的設定中，手動勾選或掛載您的 ESP32 USB 裝置，Linux 系統才能在 `/dev/ttyUSB0` 看到它。若執行指令時顯示 `No such file or directory`，請先檢查虛擬機是否已成功接管 USB。

3. **燒錄與 Agent 運行的衝突提醒**
   USB 序列埠是搶手資源，您無法同時進行「PlatformIO 燒錄」與「啟動 Agent 測試」。
   * 確保之前的 Docker Agent 已經完全關閉（在終端機按 `Ctrl+C`），否則 PlatformIO 會因為序列埠被佔用而發生 Access Denied 或燒錄失敗。若使用虛擬機，燒錄時需將 USB 主控權還給主機，測試前再掛載回虛擬機。