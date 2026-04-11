# ollie_description

Ollie AMR 的視覺化與通訊核心套件。負責接收下位機 (ESP32) 的硬體狀態，轉換為 ROS 2 座標系統，並透過 Foxglove Bridge 提供遠端 3D 即時監控。

## 目錄結構

- `urdf/ollie.urdf` : Ollie 實體的 3D 模型藍圖 (純英文，避免解析崩潰)。
- `scripts/real_ollie_core.py` : 里程計座標 (TF) 轉換與關節狀態 (Joint States) 推算節點。
- `launch/ollie_visual.launch.py` : 一鍵啟動模型發布、核心轉換與 Foxglove 通訊橋樑。

## 編譯指南

在工作空間的根目錄 (例如 `~/workspace/AMROllie/host/ros2_ws`) 執行：

```bash
colcon build --packages-select ollie_description
