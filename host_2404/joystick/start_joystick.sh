#!/bin/bash

echo "🎮 啟動 Ollie 的 PS4 手把遙控節點..."

# 取得目前腳本所在的絕對路徑，確保 yaml 檔能被正確找到
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
CONFIG_FILE="$SCRIPT_DIR/ps4_config.yaml"

# 【修正】使用正確的相容參數名稱：coalesce_interval (單位：秒)
ros2 run joy_linux joy_linux_node --ros-args -p dev:=/dev/input/js0 -p coalesce_interval:=0.1 -p deadzone:=0.2 &
JOY_PID=$!

# 啟動 teleop_node 在背景執行
ros2 run teleop_twist_joy teleop_node --ros-args --params-file "$CONFIG_FILE" &
TELEOP_PID=$!

echo "✅ 節點已啟動！ (PID: $JOY_PID, $TELEOP_PID)"
echo "🛑 按下 Ctrl+C 即可安全關閉遙控功能。"

# 使用 trap 捕捉 Ctrl+C (SIGINT) 訊號，確保退出時順手砍掉背景的 ROS 2 節點
trap "echo -e '\n關閉遙控節點...'; kill $JOY_PID $TELEOP_PID; exit" SIGINT

# 讓腳本保持執行狀態，等待訊號
wait $JOY_PID $TELEOP_PID
