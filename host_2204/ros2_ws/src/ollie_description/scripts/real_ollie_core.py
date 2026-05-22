#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
from nav_msgs.msg import Odometry
from tf2_ros import TransformBroadcaster
from geometry_msgs.msg import TransformStamped

class RealOllieCore(Node):
    def __init__(self):
        super().__init__('real_ollie_core')
        
        # 1. 發布輪胎轉角給 Foxglove 渲染
        self.joint_pub = self.create_publisher(JointState, 'joint_states', 10)
        # 2. 發布 TF 座標轉換
        self.tf_broadcaster = TransformBroadcaster(self)
        
        # 3. 訂閱 ESP32 傳來的真實里程計資料
        self.odom_sub = self.create_subscription(Odometry, 'odom', self.odom_callback, 10)
        
        # 儲存輪胎目前的視覺旋轉角度
        self.left_wheel_angle = 0.0
        self.right_wheel_angle = 0.0
        self.last_time = self.get_clock().now()

        # Ollie 的物理參數 (必須與 URDF 吻合)
        self.wheel_radius = 0.033
        self.wheel_base = 0.136  # Y軸偏移 6.8cm 的兩倍

        self.get_logger().info("真實 Ollie 核心已啟動，等待 ESP32 的 /odom 訊號中...")

    def odom_callback(self, msg):
        now = self.get_clock().now()
        dt = (now - self.last_time).nanoseconds / 1e9
        self.last_time = now
        
        if dt <= 0:
            return

        # ==========================================
        # 1. 轉發 TF (將 ESP32 算的 odom 轉成地圖座標系)
        # ==========================================
        t = TransformStamped()
        t.header.stamp = msg.header.stamp
        t.header.frame_id = 'odom'
        t.child_frame_id = 'base_link'
        
        # 直接拿 ESP32 算好的絕對位置與旋轉角度
        t.transform.translation.x = msg.pose.pose.position.x
        t.transform.translation.y = msg.pose.pose.position.y
        t.transform.translation.z = 0.0
        t.transform.rotation = msg.pose.pose.orientation
        
        self.tf_broadcaster.sendTransform(t)

        # ==========================================
        # 2. 推算輪胎視覺轉角 (給 3D 模型看)
        # ==========================================
        v = msg.twist.twist.linear.x
        w = msg.twist.twist.angular.z
        
        # 逆向運動學：從車體線速度/角速度，推回左右輪速
        v_left = v - w * (self.wheel_base / 2.0)
        v_right = v + w * (self.wheel_base / 2.0)
        
        # 累加旋轉角度 = (輪速 / 輪半徑) * 時間差
        self.left_wheel_angle += (v_left / self.wheel_radius) * dt
        self.right_wheel_angle += (v_right / self.wheel_radius) * dt

        joint_msg = JointState()
        joint_msg.header.stamp = msg.header.stamp
        joint_msg.name = ['left_wheel_joint', 'right_wheel_joint']
        joint_msg.position = [self.left_wheel_angle, self.right_wheel_angle]
        self.joint_pub.publish(joint_msg)

def main(args=None):
    rclpy.init(args=args)
    node = RealOllieCore()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
