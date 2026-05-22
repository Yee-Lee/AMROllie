# Ollie 視覺化與脫機模擬指南 (RViz2 & Fake Ollie)

本指南將帶您了解如何透過 RViz2 進行 3D 視覺化測試，以及如何在沒有連接實體硬體 (ESP32) 的情況下，使用 `fake_ollie` 進行純軟體的脫機模擬。

> **⚠️ 注意**：在進行以下步驟前，請確保您已經完成 `ros2_ws/src/ollie_description` 套件的編譯。

---

## 1. 安裝 RViz2 與視覺化測試

RViz2 是 ROS 2 的官方 3D 視覺化工具。您可以在樹莓派本地或同網段的開發機 (Mac/PC) 上開啟。

**1. 安裝 RViz2** 
*(如果您使用的是 Ubuntu Desktop 的 full 版本通常已內建，否則請手動安裝)*：
```bash
sudo apt update
sudo apt install -y ros-jazzy-rviz2
```

**2. 啟動與設定**
確保您已經執行了實體車或模擬的 Launch 檔，接著開啟一個**新的終端機**：
```bash
export ROS_DOMAIN_ID=30
rviz2
```

**RViz2 設定步驟：**
1. 在左下角點擊 **Add** -> 選擇 **RobotModel** 即可看到 Ollie 的 3D 外觀。
2. 點擊 **Add** -> 選擇 **TF** 可以看見各個關節的坐標軸。
3. 在左上角的 **Fixed Frame** 欄位，手動輸入 `odom` 或 `base_link`。

---

## 2. 脫機模擬測試 (Fake Ollie)

當您手邊沒有實體車 (未連接 ESP32)，但想測試導航演算法、或純粹想觀察車體在 RViz2 中的運動時，可以使用 `fake_ollie` 模擬器。

`fake_ollie` 是一個純 Python 腳本 (`fake_ollie_core.py`)，它會**假裝自己是硬體**，接收您發送的 `/cmd_vel` (速度指令)，並利用數學運算模擬車體移動，自動發布對應的 `/odom` (里程計) 與 TF 轉換。

**測試步驟：**

**(1) 啟動視覺化與模型 (Terminal 1)**
```bash
cd ros2_ws
source install/setup.bash
export ROS_DOMAIN_ID=30
ros2 launch ollie_description ollie_visual.launch.py
```

**(2) 啟動 Fake Ollie 模擬核心 (Terminal 2)**
不要啟動 `micro_ros_agent`，改為啟動我們的假硬體節點：
```bash
cd ros2_ws
source install/setup.bash
export ROS_DOMAIN_ID=30
ros2 run ollie_description fake_ollie_core.py
```

**(3) 發送移動指令 (Terminal 3)**
這時您可以在 RViz2 中看到車體。開啟鍵盤遙控節點，按下方向鍵來移動它：
```bash
export ROS_DOMAIN_ID=30
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```
此時，您會看到 RViz2 中的 Ollie 車體根據您的鍵盤指令，在虛擬空間中順暢移動，這代表模擬環境已成功建立！
