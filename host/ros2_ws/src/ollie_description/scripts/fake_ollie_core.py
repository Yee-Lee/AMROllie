#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from tf2_ros import TransformBroadcaster
from geometry_msgs.msg import TransformStamped
import math

class FakeOllieCore(Node):
    def __init__(self):
        super().__init__('fake_ollie_core')
        
        # 建立發布器
        self.joint_pub = self.create_publisher(JointState, 'joint_states', 10)
        self.tf_broadcaster = TransformBroadcaster(self)
        
        # Ollie 的狀態變數 (X, Y 座標、面向角度、輪胎轉角)
        self.x = 0.0
        self.y = 0.0
        self.theta = 0.0
        self.wheel_angle = 0.0
        
        # 設定定時器，每秒執行 30 次 (30Hz)
        self.timer = self.create_timer(1.0 / 30.0, self.timer_callback)
        self.get_logger().info("Ollie 假訊號發電機已啟動！正在畫圓圈...")

    def timer_callback(self):
        now = self.get_clock().now().to_msg()
        dt = 1.0 / 30.0
        
        # 1. 模擬物理移動 (讓車子以 10cm/s 前進，並緩慢轉彎畫大圓)
        linear_vel = 0.1   # 線速度：每秒 0.1 公尺
        angular_vel = 0.2  # 角速度：每秒轉 0.2 弧度
        
        self.theta += angular_vel * dt
        self.x += linear_vel * math.cos(self.theta) * dt
        self.y += linear_vel * math.sin(self.theta) * dt
        
        # 模擬輪胎旋轉 (角速度 = 線速度 / 輪胎半徑 0.033m)
        wheel_angular_vel = linear_vel / 0.033
        self.wheel_angle += wheel_angular_vel * dt

        # 2. 發布 JointState (告訴系統輪胎轉到哪了)
        joint_msg = JointState()
        joint_msg.header.stamp = now
        joint_msg.name = ['left_wheel_joint', 'right_wheel_joint']
        joint_msg.position = [self.wheel_angle, self.wheel_angle]
        self.joint_pub.publish(joint_msg)

        # 3. 發布 TF 座標 (告訴系統車子在世界的哪裡)
        t = TransformStamped()
        t.header.stamp = now
        t.header.frame_id = 'odom'        # 世界原點
        t.child_frame_id = 'base_link'    # 車身中心
        
        # 設定 X, Y 移動
        t.transform.translation.x = self.x
        t.transform.translation.y = self.y
        t.transform.translation.z = 0.0
        
        # 將面向角度 (Theta) 轉換為四元數 (Quaternion) 旋轉
        t.transform.rotation.x = 0.0
        t.transform.rotation.y = 0.0
        t.transform.rotation.z = math.sin(self.theta / 2.0)
        t.transform.rotation.w = math.cos(self.theta / 2.0)
        
        self.tf_broadcaster.sendTransform(t)

def main(args=None):
    rclpy.init(args=args)
    node = FakeOllieCore()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
