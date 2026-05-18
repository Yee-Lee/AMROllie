#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Twist
import math

class OllieLogProcessor(Node):
    def __init__(self):
        super().__init__('ollie_log_processor')
        
        # --- 調校參數 ---
        self.log_divider = 1  # 頻率除數。1:全印, 2:印一半, 5:五分之一，以此類推
        self.counter = 0      # 內部計數器
        
        # 記憶最新的指令
        self.cmd_v = 0.0
        self.cmd_w = 0.0
        
        # 訂閱設定
        self.create_subscription(Twist, '/cmd_vel', self.cmd_callback, 10)
        self.create_subscription(Odometry, '/odom', self.odom_callback, 10)
        
        self.get_logger().info(f'Ollie 數據監控中... (頻率稀釋倍率: {self.log_divider})')

    def cmd_callback(self, msg):
        self.cmd_v = msg.linear.x
        self.cmd_w = msg.angular.z

    def odom_callback(self, msg):
        self.counter += 1
        # 頻率控制邏輯
        if self.counter % self.log_divider != 0:
            return

        # 1. 抓取實際數值
        v = msg.twist.twist.linear.x
        w_corr = msg.twist.twist.angular.z # 這裡對應你需要的角速度修正值
        
        # 2. 座標與角度轉換
        x = msg.pose.pose.position.x
        y = msg.pose.pose.position.y
        q = msg.pose.pose.orientation
        theta = math.atan2(2 * (q.w * q.z + q.x * q.y), 1 - 2 * (q.y * q.y + q.z * q.z))

        # 3. 打印符合你習慣的 [DEBUG] 格式
        print(f"[DEBUG] CMD(v:{self.cmd_v: 6.3f}, w:{self.cmd_w: 6.3f}) | "
              f"ODOM(v:{v: 6.3f}, w_corr:{w_corr: 6.3f}) | "
              f"Pose(x:{x: 6.3f}, y:{y: 6.3f}, th:{theta: 6.3f})")

def main(args=None):
    rclpy.init(args=args)
    node = OllieLogProcessor()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
