import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    # 1. 找到你的 URDF 檔案路徑
    urdf_file = os.path.join(
        get_package_share_directory('ollie_description'),
        'urdf',
        'ollie.urdf'
    )

    # 2. 讀取檔案內容
    with open(urdf_file, 'r') as infp:
        robot_desc = infp.read()

    return LaunchDescription([
        # 節點 A：發布模型狀態 (將 URDF 轉換為 TF 座標)
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{'robot_description': robot_desc}],
        ),
        # 節點 B：啟動 Foxglove 橋樑 (讓 Mac 可以連進來看畫面)
        Node(
            package='foxglove_bridge',
            executable='foxglove_bridge',
            name='foxglove_bridge',
            output='screen',
        ),
        # 3. 啟動 Fake Ollie Core (發布假資料)
        Node(
            package='ollie_description',
            executable='fake_ollie_core.py',
            name='fake_ollie_core',
            output='screen',
        )
    ])
