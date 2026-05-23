#!/bin/bash

# 切換到腳本所在目錄，確保能找到 launch 檔
cd "$(dirname "$0")" || exit

echo "🚀 正在啟動 Ollie SLAM 建圖系統..."
ros2 launch ollie_slam_launch.py