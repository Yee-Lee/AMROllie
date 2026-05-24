# CycloneDDS 安裝與驗證指南

在 ROS 2 中，預設的通訊後端 (RMW) 有時在跨設備（如機器人到筆電）的 Wi-Fi 環境下不夠穩定。我們推薦使用 **Eclipse Cyclone DDS**。

---

## 1. 安裝 CycloneDDS

在機器人 (Raspberry Pi) 與遠端筆電上皆執行：

```bash
sudo apt update
sudo apt install ros-jazzy-rmw-cyclonedds-cpp
```

---

## 2. 啟用設定

要切換到 CycloneDDS，您需要設定環境變數 `RMW_IMPLEMENTATION`。

**暫時切換（當前終端機）：**
```bash
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
```

**永久切換（寫入 .bashrc）：**
```bash
echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc
source ~/.bashrc
```

---

## 3. 驗證是否生效

### 方法一：查看執行時日誌
啟動任何 ROS 2 節點時，如果環境變數設定正確，ROS 2 會載入對應的動態連結程式。

### 方法二：使用 `ros2 doctor`
執行以下指令：
```bash
ros2 doctor --report | grep middleware
```
**預期輸出：**
```text
middleware_name    : rmw_cyclonedds_cpp
```

### 方法三：重置 Daemon 測試
有時切換 RMW 後會因為舊的 Daemon 導致通訊失敗，建議重置：
```bash
ros2 daemon stop
ros2 daemon start
```

---

## 4. 故障排除
如果在跨設備通訊中仍然找不到節點：
1. 確保雙方的 `ROS_DOMAIN_ID` 相同。
2. 檢查防火牆是否允許 UDP 通訊（CycloneDDS 主要使用 UDP）。
3. 嘗試在 `CYCLONEDDS_URI` 設定檔中指定網路介面。
