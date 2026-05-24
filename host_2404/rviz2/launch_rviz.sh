#! /bin/bash
# 1. 載入 ROS 2 環境與設定
source /opt/ros/humble/setup.zsh
export ROS_DOMAIN_ID=30
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export LIBGL_ALWAYS_SOFTWARE=1

# 2. 自動除錯：重置通訊兵
echo "正在重置 ROS2 Daemon (${RWM_IMPLEMENTATION})..."
ros2 daemon stop
ros2 daemon start

# 3. 啟動 Rviz2
echo "正在啟動 Rviz2...(ROS_DOMAIN_ID=${ROS_DOMAIN_ID})"
rviz2