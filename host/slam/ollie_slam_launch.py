import os
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # 強制指定你剛剛修改過的 yaml 檔案路徑
    params_file = '/home/yee/workspace/AMROllie/host/slam/mapper_params.yaml'

    return LaunchDescription([
        Node(
            parameters=[
                params_file,
                {'use_sim_time': False} # 確保使用系統真實時間
            ],
            package='slam_toolbox',
            executable='async_slam_toolbox_node',
            name='slam_toolbox',
            output='screen'
        )
    ])
