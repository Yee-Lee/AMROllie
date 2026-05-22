# ollie_description (機器人本體與核心座標)

本套件是 Ollie AMR 的視覺化與通訊核心，負責載入實體模型 (URDF) 並發布靜態/動態座標轉換 (TF)。它是所有導航 (Nav2)、建圖 (SLAM) 與模擬任務的基礎。

---

## 1. 什麼是 URDF？如何建立？

URDF (Unified Robot Description Format) 是 ROS 中用來描述機器人實體結構的標準 XML 格式。
在我們的專案中，URDF 檔案位於：`urdf/ollie.urdf`。

**建立與修改 URDF 的重點：**
1. **`base_link` (基準連桿)**：這是機器人的核心座標系（通常位於底盤中心）。所有的零件都是相對於這個中心點展開的。
2. **Link (連桿)**：代表機器的剛體部件，例如輪子 (`left_wheel`, `right_wheel`)、光達 (`laser_link`) 等。
3. **Joint (關節)**：定義 Link 之間的連接方式。例如輪子使用 `continuous` 關節來允許無限旋轉，而光達使用 `fixed` 關節固定在底盤上。
4. **維護建議**：若您未來要修改機器人外觀、改變光達高度，或新增超音波感測器，請直接編輯 `ollie.urdf`。修改時請確保 `<joint>` 的 XYZ 偏移量 (Origin) 與實際車體的實體距離相符（單位為公尺）。

---

## 2. 編譯與啟動 (真實環境)

當您修改了 URDF 或是剛 Clone 完專案後，需要編譯 `ollie_description` 套件讓系統抓到最新的模型。

**1. 編譯套件**
請在終端機中，從專案根目錄進入 `ros2_ws` 並執行編譯。強烈建議加上 `--symlink-install`，這樣未來修改 URDF 或 Python 檔後不需重新編譯即可生效：
```bash
cd ros2_ws
colcon build --packages-select ollie_description --symlink-install
source install/setup.bash
```

**2. 啟動機器人狀態發布**
啟動 Launch 檔，這會讀取 URDF 並啟動 `robot_state_publisher`，將機器人的靜態結構廣播到 ROS 2 系統中。
```bash
ros2 launch ollie_description ollie_visual.launch.py
```

---

## 3. 視覺化與脫機模擬 (Simulation)

> 💡 **需要進行 RViz 視覺化測試或軟體模擬？**
> 
> 關於如何安裝並使用 RViz2 查看 3D 模型，以及如何在不連接實體 ESP32 的情況下，使用 `fake_ollie` 進行純軟體的脫機模擬，我們已經將這些內容整理在獨立的教學文件中。
> 
> 👉 請參閱：[Ollie 視覺化與脫機模擬指南](../../../../docs/simulation.md)
