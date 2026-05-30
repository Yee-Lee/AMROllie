# ROS 2 AMR 座標轉換 (TF) 核心觀念筆記 (AMROllie 專案版)

## 1. 觀念釐清：主題 (Topics) vs 座標系 (Frames)

* **主題 (Topics)**：負責傳遞「數據內容」。例如 `/odom` 傳遞的是位置與速度的數值，`/scan` 傳遞的是雷達測量的距離數值。
* **座標系 (Frames)**：代表空間中的「參考點」。TF 系統負責計算這些點之間的幾何變換（Translation + Rotation）。

---

## 2. AMROllie 的 TF 階層關係圖

在我們的專案中，TF 樹呈現以下單向階層關係：

```text
map (地圖座標系)
 └── odom (里程計座標系)
      └── base_link (車體基準中心)
           ├── base_laser (雷達安裝點)
           ├── left_wheel (左輪旋轉中心)
           ├── right_wheel (右輪旋轉中心)
           ├── left_ultrasonic_link (左超音波)
           └── right_ultrasonic_link (右超音波)
```

### 各層級的發布來源與意義：

1.  **`map` -> `odom`**
    *   **意義**：修正里程計的累積誤差，將機器人定位在真實地圖中。
    *   **來源**：由 **SLAM (Slam Toolbox)** 或 **導航 (Nav2)** 演算法發布。
2.  **`odom` -> `base_link`**
    *   **意義**：描述機器人相對於「開機原點」的連續移動。
    *   **來源**：由上位機的 `real_ollie_core.py` 接收 ESP32 的數據後即時發布。
3.  **`base_link` -> `各組件`**
    *   **意義**：描述感測器或輪胎相對於車體中心的固定安裝位置。
    *   **來源**：由 `robot_state_publisher` 讀取 `ollie.urdf` 描述檔後自動發布。

---

## 3. `/tf` 與 `/tf_static` 的差異

為了節省通訊頻寬，ROS 2 將座標變換拆分為兩個通道：

| 特性 | `/tf` (動態座標) | `/tf_static` (靜態座標) |
| :--- | :--- | :--- |
| **內容** | 隨時間不斷變動的關係 | 物理結構上鎖死不動的關係 |
| **AMROllie 例子** | `odom` → `base_link` (車在走) | `base_link` → `base_laser` (雷達鎖在車上) |
| **特性** | 高頻率、連續更新 (如 20Hz) | 只在啟動或變更時發布一次，永久保留 |

---

## 4. 認識 `frame_id` 與 `child_frame_id`

在 ROS 2 的訊息標頭 (Header) 中，我們透過 ID 來定義空間關係：

*   **單一座標宣告 (例如：雷達掃描數據 `/scan`)**：
    *   `header.frame_id: "base_laser"`
    *   **解讀**：這份距離數據是「以雷達感測器為中心」測量出來的。
*   **雙座標關係宣告 (例如：里程計數據 `/odom`)**：
    *   `header.frame_id: "odom"` (父層參考系)
    *   `child_frame_id: "base_link"` (子層參考系)
    *   **解讀**：這份數據描述的是 `base_link` 相對於 `odom` 的位移與旋轉。

---

## 5. 常見問題：為什麼 RViz 看不到模型？

1.  **TF 斷裂**：檢查是否有節點沒啟動（例如 `real_ollie_core` 沒跑，`odom` 到 `base_link` 的連接就會斷開）。
---

## 6. 如何檢查 TF 關係是否正常？

除了 `ros2 run tf2_tools view_frames` (產生 PDF 圖表) 外，還有幾種更直接、快速的終端機檢查方式：

### A. 實時數值檢查 (`tf2_echo`)
如果你想看特定的兩個座標系之間的精確數值（例如 `odom` 到 `base_link` 的位移），這是最常用的工具。
```bash
ros2 run tf2_ros tf2_echo <source_frame> <target_frame>

# 範例：查看機器人現在相對於里程計原點的位置
ros2 run tf2_ros tf2_echo odom base_link
```

### B. 整體狀態監測 (`tf2_monitor`)
這可以用來檢查所有座標系的發布頻率、延遲以及父子關係是否正確連通。
```bash
ros2 run tf2_ros tf2_monitor

# 你會看到類似這樣的清單：
# Frame: base_link, published by <node_name>, Average Delay: 0.001, Max Delay: 0.01
```

### C. RViz2 直觀可視化 (推薦)
這是最簡單的方式。啟動 RViz2 後：
1.  左側選單點擊 **Add** -> 選擇 **TF**。
2.  你會看到空間中出現許多座標軸（紅綠藍箭頭）。
3.  **檢查重點**：
    *   箭頭是否都在動？（動態 TF 應該要隨時更新）。
    *   是否有報錯（Status: Error）？如果有，通常是 TF 樹斷裂。
    *   座標軸的方向是否符合物理常識（例如 Z 軸是否朝上）。

### D. 快速查看原生數據 (`ros2 topic echo`)
直接看 `/tf` 或 `/tf_static` 的最後一筆資料，確認節點有沒有在說話。
```bash
# 查看最新的動態座標變換
ros2 topic echo /tf --limit 1

# 查看靜態座標變換（例如雷達安裝位移）
ros2 topic echo /tf_static --limit 1
```

### E. 檢查 TF 樹是否完整 (CLI 版)
如果你不想開 PDF，可以直接在終端機印出樹狀文字：
```bash
ros2 run tf2_ros tf2_monitor <frame_id> # 雖然是 monitor，但它會列出父層關係
```
*(註：在最新的 ROS 2 版本中，`view_frames` 依然是檢查「全局斷裂」最權威的方法，但開發時搭配 `tf2_echo` 效率最高。)*
