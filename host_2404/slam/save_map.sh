#!/bin/bash

# 切換到腳本所在目錄
cd "$(dirname "$0")" || exit

# 定義地圖儲存目錄
MAPS_DIR="maps"

# 檢查並建立 maps 目錄
if [ ! -d "$MAPS_DIR" ]; then
    echo "📁 建立地圖儲存目錄: $MAPS_DIR"
    mkdir -p "$MAPS_DIR"
fi

# 取得當前時間做為檔名 (格式: YYYYMMDD_HHMMSS)
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
MAP_NAME="map_${TIMESTAMP}"

echo "💾 正在儲存地圖至: $MAPS_DIR/$MAP_NAME ..."

# 執行地圖儲存指令，使用絕對路徑確保儲存位置正確
ros2 run nav2_map_server map_saver_cli -f "$PWD/$MAPS_DIR/$MAP_NAME"