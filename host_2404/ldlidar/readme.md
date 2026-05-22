# LD19 LiDAR 使用與整合指南

本指南說明如何將 LD19 光達硬體連接至系統、設定 `udev` 規則、進行底層通訊測試，以及下載並編譯 ROS 2 節點以供 AMR (如 Ollie) 導航使用。

---

## 1. 環境準備與硬體連接 (udev 設定)

在啟動任何軟體之前，必須先確保實體接線與系統權限設定正確。系統預設會將 USB 轉 TTL 設備識別為 `/dev/ttyUSB0` 等不穩定的名稱。為了確保每次開機都能正確識別雷達，我們需要建立 `udev` 規則將其綁定為 `/dev/ollie_lidar`。

1. **硬體連接**：將 LD19 雷達透過隨附的 USB 轉 TTL 轉接板，插入樹莓派/電腦的 USB 孔。
2. **確認硬體 ID**：輸入 `lsusb` 指令，你應該會看到類似 `ID 10c4:ea60 Cygnal Integrated Products, Inc. CP2102...` 的設備。`10c4` 為 Vendor ID，`ea60` 為 Product ID。
3. **建立 udev 規則檔案**：執行以下指令來建立綁定規則（假設你的晶片為 CP210x）：
   ```bash
   echo 'KERNEL=="ttyUSB*", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE:="0666", SYMLINK+="ollie_lidar"' | sudo tee /etc/udev/rules.d/10-ollie_lidar.rules
   ```
4. **套用規則**：
   ```bash
   sudo udevadm control --reload-rules
   sudo udevadm trigger
   ```
5. **硬體重置**：請**將光達的 USB 拔掉再重新插上**。
6. **確認裝置名稱**：檢查綁定是否成功：
   ```bash
   ls -l /dev/ollie_lidar
   ```
   *(應該會看到它正確連結到 `ttyUSB0` 或類似名稱)*

---

## 2. 硬體通訊測試 (UART 裸測)

在進入複雜的 ROS 2 系統前，建議先透過簡單的 Python 腳本直接讀取 USB 序列埠 (UART)，確認雷達馬達有在轉、且有原始數據傳入。這**完全不需要**啟動 ROS 2。

### 執行測試腳本

確保系統已安裝 `pyserial` 套件（`pip install pyserial`）。本目錄下已備妥 `test_lidar.py` 供測試：

**執行測試：**
```bash
cd ldlidar
python3 test_lidar.py
```
*(如果畫面上印出一長串收到 0x54 標頭的訊息，代表雷達硬體與通訊完全正常！)*

---

## 3. 下載與編譯 LiDAR ROS 2 驅動

在啟動或修改節點前，必須先從官方下載原始碼並進行編譯。

**1. 下載原始碼**
將官方原始碼 clone 到 ROS 2 工作區的 `src` 目錄下：
```bash
cd ros2_ws/src
git clone https://github.com/ldrobotSensorTeam/ldlidar_stl_ros2.git
```

**2. 修正官方程式碼編譯錯誤**
由於官方驅動在較新的編譯器環境下會報錯 (`pthread_mutex_init` was not declared)，我們必須在編譯前手動補上標頭檔。
請執行以下指令，透過 `sed` 自動將 `<pthread.h>` 補入程式碼中：
```bash
sed -i 's/#include <string.h>/#include <string.h>\n#include <pthread.h>/' ldlidar_stl_ros2/ldlidar_driver/src/logger/log_module.cpp
```

**3. 單獨編譯套件**
回到工作區根目錄，使用 `colcon build` 並指定只編譯雷達套件，**強烈建議加上 `--symlink-install`**，這樣後續修改 Launch 檔就不需要重新編譯：
```bash
cd ../
colcon build --packages-select ldlidar_stl_ros2 --symlink-install
```

**4. 更新環境**
編譯完成後，務必重新載入環境變數讓系統找到新編譯好的執行檔：
```bash
source install/setup.bash
```

---

## 4. 修正 Launch 檔通訊埠設定

剛下載好的官方 Launch 檔案，預設通訊埠通常為 `/dev/ttyUSB0`。我們需要將其改為我們設定好的 `/dev/ollie_lidar`。

*(由於我們剛才編譯時使用了 `--symlink-install`，修改此檔案後會立即生效，無須重新編譯。)*

**解決辦法**：
請使用編輯器打開以下檔案：
```bash
vim src/ldlidar_stl_ros2/launch/ld19.launch.py
```
尋找包含 `port_name` 的設定行（例如 `{'port_name': '/dev/ttyUSB0'}`），將其修改為 `{'port_name': '/dev/ollie_lidar'}` 並存檔。

---

## 5. 啟動 LD19 ROS 2 節點

確認硬體通訊無誤且編譯修改完成後，即可啟動 ROS 2 節點，讓雷達數據轉換為標準的 `/scan` 話題供系統使用。

**1. 載入工作區環境**
```bash
cd ../
source install/setup.bash
```

**2. 啟動雷達 Launch 檔**
```bash
ros2 launch ldlidar_stl_ros2 ld19.launch.py
```
啟動成功後，雷達會持續發布 `sensor_msgs/msg/LaserScan` 訊息至 `/scan` 話題，你可以在另一個終端機使用 `ros2 topic echo /scan` 或是開啟 RViz2 進行 3D 視覺化確認。
