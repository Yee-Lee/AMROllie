# Ollie Web Controller 設定指南

本指南說明如何在 Raspberry Pi 3B (Ubuntu 22.04 / ROS 2 Humble) 上建立 `rosbridge_suite`，並透過網頁端的手機虛擬搖桿，實現遠端遙控 Ollie AMR。

## 系統架構簡介
* **前端介面 (手機/PC)**：透過瀏覽器開啟 HTML 網頁，使用虛擬搖桿產生控制指令。
* **通訊橋樑 (WebSocket)**：透過 `roslib.js` 將前端指令打包成 JSON，發送至 RPi。
* **ROS 2 後端 (RPi3B)**：`rosbridge_server` 接收 JSON 並轉換為 ROS 2 標準的 `geometry_msgs/msg/Twist` 訊息，發布至 `/cmd_vel` Topic。

---

## 🛠️ 第一階段：環境安裝與準備

### 1. 安裝 rosbridge_suite
在 RPi3B 的終端機中執行以下指令，安裝 WebSocket 通訊套件：
```bash
sudo apt update
sudo apt install ros-humble-rosbridge-suite
```

### 2. 建立網頁伺服器資料夾
建立一個專門放置網頁檔案的資料夾，並將我們的 `index.html` 放入其中：
```bash
mkdir -p ~/ollie_ws/web_controller
cd ~/ollie_ws/web_controller
# 將控制面板的 HTML 程式碼存成 index.html 放進這個資料夾
```

### 3. (選用) 離線環境設定
如果 Ollie 未來需要在「沒有網際網路」的區網環境下運作，請先下載 `roslib.min.js`：
```bash
wget https://cdn.jsdelivr.net/npm/roslib@1/build/roslib.min.js
```
*接著打開 `index.html`，將 `<script src="https://cdn.jsdelivr.net/..."></script>` 改為 `<script src="./roslib.min.js"></script>`*。

---

## 🚀 第二階段：啟動系統

為了讓系統順利運作，我們需要在 RPi3B 上開啟 **三個不同的終端機 (Terminal)**。
> ⚠️ **注意**：每次開啟新的終端機，都請確保已經載入 ROS 2 環境變數：
> `source /opt/ros/humble/setup.bash`

### [終端機 1] 啟動 WebSocket 橋樑
這個節點負責監聽網頁傳來的 JSON 封包，並將其轉換為 ROS 2 Topic。
```bash
ros2 launch rosbridge_server rosbridge_websocket_launch.xml
```
*預設會在 RPi 的 `9090` port 開啟 WebSocket 服務。*

### [終端機 2] 啟動 Web Server (託管網頁)
切換到剛才建立的網頁資料夾，使用 Python 內建的 HTTP 伺服器將網頁發布到區網中。
```bash
cd ~/ollie_ws/web_controller
python3 -m http.server 8000
```
*此時網頁已經架設在 RPi 的 `8000` port 上。*

---

## 🎮 第三階段：連線與測試

### 1. 取得 RPi3B 的區網 IP
如果你不知道 RPi3B 的 IP 位址，可以在 RPi 上輸入 `ip a` 或 `ifconfig` 查詢（例如：`192.168.1.100`）。

### 2. 開啟手機或 PC 瀏覽器
確認你的控制裝置（手機或筆電）與 RPi3B 連接在**同一個 Wi-Fi 網路**下。
在瀏覽器網址列輸入：
```text
http://<RPi3B的IP位址>:8000
```
網頁載入後，你應該會看到暗黑風格的 OLLIE DASHBOARD。

### 3. 驗證資料流 (Echo Topic)
在 RPi3B 上開啟 **[終端機 3]**，輸入以下指令來監看速度控制訊號：
```bash
ros2 topic echo /cmd_vel
```

**操作測試流程：**
1. 觀察網頁左上角的狀態，應顯示綠燈 🟢 代表 WebSocket 連線成功。
2. 點擊右上角的 `WAITING` 按鈕，將系統權限切換為 `ON`。
3. 嘗試推動左側或右側的虛擬搖桿。
4. 查看 **[終端機 3]** 的畫面，你應該會即時看到如下的數據噴發：
   ```yaml
   linear:
     x: 0.27
     y: 0.0
     z: 0.0
   angular:
     x: 0.0
     y: 0.0
     z: -1.5
   ---
   ```
   *數值會受限於我們設定的物理安全極限 (V_max = 0.27, W_max = 2.7)。*
5. 放開搖桿，終端機應立刻收到全為 `0.0` 的煞車指令。

---

## 💡 疑難排解 (Troubleshooting)

* **網頁顯示 🔴 連線錯誤：**
  * 確認終端機 1 的 `rosbridge_server` 是否有報錯。
  * 確認手機與 RPi 是否真的在同一個網段。
  * 檢查 RPi 的防火牆設定，確保 port `9090` 和 `8000` 有開放。
* **資料有收到，但 Ollie 的馬達沒動：**
  * 確認 `micro-ros-agent` 是否有同時在另一個終端機啟動。
  * 確認 ESP32 的 C++ 程式中，是否有正確訂閱 `/cmd_vel` 並實作逆運動學 (Inverse Kinematics) 轉換邏輯。
