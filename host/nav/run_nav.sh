#!/bin/bash

# 先刪除之前的 FastDDS 緩存，避免參數衝突
sudo rm -rf /dev/shm/fastrtps*

ros2 launch nav2_bringup bringup_launch.py \
    use_sim_time:=False \
    autostart:=True \
    map:=/home/yee/workspace/AMROllie/host/slam/maps/my_home_map.yaml \
    params_file:=/home/yee/workspace/AMROllie/host/nav/ollie_nav2_params.yaml
