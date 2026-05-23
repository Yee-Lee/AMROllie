import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    # 獲取當前檔案所在目錄
    current_dir = os.path.dirname(os.path.realpath(__file__))
    
    # 定義參數檔案路徑
    slam_params_file = os.path.join(current_dir, 'mapper_params.yaml')

    # 宣告參數 (可選，方便從命令列覆蓋)
    declare_params_file_cmd = DeclareLaunchArgument(
        'slam_params_file',
        default_value=slam_params_file,
        description='Full path to the ROS2 parameters file to use for the slam_toolbox node'
    )

    # 啟動 slam_toolbox 節點 (非同步模式)
    start_async_slam_toolbox_node = Node(
        parameters=[
            LaunchConfiguration('slam_params_file'),
            {'use_sim_time': False} # Ollie 實體運行不使用模擬時間
        ],
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen'
    )

    ld = LaunchDescription()

    # 加入動作
    ld.add_action(declare_params_file_cmd)
    ld.add_action(start_async_slam_toolbox_node)

    return ld
