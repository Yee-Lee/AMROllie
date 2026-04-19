# LD19 LiDAR 使用與整合指南

本指南說明如何將 LD19 光達硬體連接至系統、進行底層通訊測試，並啟動 ROS 2 節點以供 AMR (如 Ollie) 導航使用。

---

## 1. 環境準備與硬體連接

在啟動任何軟體之前，必須先確保實體接線與系統權限設定正確。

1. **硬體連接**：將 LD19 雷達透過隨附的 USB 轉 TTL 轉接板，插入樹莓派/電腦的 USB 孔。
2. **確認裝置名稱**：通常系統會將其識別為 `/dev/ttyUSB0`。你可以透過以下指令檢查：
   ```bash
   ls /dev/ttyUSB*
   ```
3. **賦予讀寫權限**：為了讓程式能讀取雷達數據，必須賦予該序列埠權限（每次重開機後若無設定 udev 規則，需重新執行）：
   ```bash
   sudo chmod 777 /dev/ttyUSB0
   ```

---

## 2. 硬體通訊測試 (UART 裸測)

在進入複雜的 ROS 2 系統前，建議先透過簡單的 Python 腳本直接讀取 USB 序列埠 (UART)，確認雷達馬達有在轉、且有原始數據傳入。這**完全不需要**啟動 ROS 2。

### 建立與執行 `test_lidar.py`

確保系統已安裝 `pyserial` 套件（`pip install pyserial`），然後建立 `test_lidar.py`：

**執行測試：**
```bash
python3 test_lidar.py
```
*(如果畫面上印出一長串英數夾雜的 Hex 數值，代表雷達硬體與 USB 轉接板完全正常！)*

---

## 3. 啟動 LD19 ROS 2 節點

確認硬體通訊無誤後，即可啟動 ROS 2 節點，讓雷達數據轉換為標準的 `/scan` 話題供系統使用。

**1. 載入工作區環境**
```bash
cd ~/workspace/AMROllie/host/ros2_ws
source install/setup.bash
```

**2. 啟動雷達 Launch 檔**
```bash
ros2 launch ldlidar_stl_ros2 ld19.launch.py
```
啟動成功後，雷達會持續發布 `sensor_msgs/msg/LaserScan` 訊息至 `/scan` 話題，你可以在另一個終端機使用 `ros2 topic echo /scan` 或是開啟 Foxglove Studio 進行 3D 視覺化確認。

---

## 4. 補充說明：如何編譯 LD19 ROS 2 套件

若你是初次建置環境，或需要從原始碼重新編譯 LD19 的驅動程式，請依循以下步驟：

**1. 下載原始碼**
將官方原始碼 clone 到工作區的 `src` 目錄下：
```bash
cd ~/workspace/AMROllie/host/ros2_ws/src
git clone [https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git](https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git)
```

**2. 單獨編譯套件**
回到工作區根目錄，使用 `colcon build` 並指定只編譯雷達套件以節省時間：
```bash
cd ~/workspace/AMROllie/host/ros2_ws
colcon build --packages-select ldlidar_stl_ros2
```

**3. 更新環境**
編譯完成後，務必重新載入環境變數讓系統找到新編譯好的執行檔：
```bash
source install/setup.bash
```