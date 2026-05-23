#!/bin/bash

# 切換到腳本所在目錄
cd "$(dirname "$0")" || exit

# 預設地圖路徑 (可根據需求修改)
DEFAULT_MAP="$PWD/maps/map.yaml"

if [ ! -f "$DEFAULT_MAP" ]; then
    echo "⚠️ 找不到預設地圖: $DEFAULT_MAP"
    echo "請確保 maps 目錄下有 map.yaml，或手動修改此腳本。"
    # 不退出，讓使用者知道可能需要手動指定
fi

echo "🚀 正在啟動 Ollie 導航系統..."
ros2 launch ollie_nav_launch.py map:="$DEFAULT_MAP"
