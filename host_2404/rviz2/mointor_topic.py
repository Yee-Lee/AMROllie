#!/usr/bin/env python3
import os
import sys
import time
from collections import deque
import rclpy
from rclpy.node import Node

# 嘗試載入需要的消息型態（若未安裝相關套件會跳過內容顯示，但仍可算 HZ）
try:
    from nav_msgs.msg import Odometry
    from sensor_msgs.msg import LaserScan
    from tf2_msgs.msg import TFMessage
except ImportError:
    pass

class TopicMonitor(Node):
    def __init__(self):
        super().__init__('rviz_topic_monitor')

        # 儲存時間戳記用來計算 HZ
        self.timestamps = {
            '/odom': deque(maxlen=20),
            '/scan': deque(maxlen=20),
            '/tf': deque(maxlen=20)
        }

        # 儲存最新一筆內容摘要
        self.latest_data = {
            '/odom': "等待資料...",
            '/scan': "等待資料...",
            '/tf': "等待資料..."
        }

        # 訂閱 Topics (這裡使用 generic 訂閱或嘗試特定型態)
        # 注意：ROS2 必須指定正確的型態，如果型態不符會噴錯
        try:
            self.create_subscription(Odometry, '/odom', lambda msg: self.cb('/odom', msg), 10)
            self.create_subscription(LaserScan, '/scan', lambda msg: self.cb('/scan', msg), 10)
            self.create_subscription(TFMessage, '/tf', lambda msg: self.cb('/tf', msg), 10)
        except Exception as e:
            self.get_logger().error(f"訂閱失敗，請確保安裝了 nav_msgs, sensor_msgs, tf2_msgs: {e}")

        # 建立定時器，每秒在終端機刷一次畫面
        self.create_timer(1.0, self.update_display)
        print("Topic 監控啟動，正在收集數據...")

    def cb(self, topic_name, msg):
        current_time = time.time()
        self.timestamps[topic_name].append(current_time)

        # 根據不同 topic 擷取精簡的重要內容，避免洗版
        if topic_name == '/odom':
            pos = msg.pose.pose.position
            ori = msg.pose.pose.orientation
            self.latest_data[topic_name] = f"Pos: ({pos.x:.2f}, {pos.y:.2f}, {pos.z:.2f}) | Ori_w: {ori.w:.2f}"

        elif topic_name == '/scan':
            # 顯示雷達點數與前後左右的幾點距離作參考
            num_ranges = len(msg.ranges)
            mid = num_ranges // 2
            self.latest_data[topic_name] = f"Points: {num_ranges} | Front Dist: {msg.ranges[0]:.2f}m | Back Dist: {msg.ranges[mid]:.2f}m"

        elif topic_name == '/tf':
            if msg.transforms:
                t = msg.transforms[0]
                trans = t.transform.translation
                self.latest_data[topic_name] = f"{t.header.frame_id} -> {t.child_frame_id} | XYZ: ({trans.x:.2f}, {trans.y:.2f}, {trans.z:.2f})"

    def calculate_hz(self, topic_name):
        times = self.timestamps[topic_name]
        if len(times) < 2:
            return 0.0
        # 用最早和最晚的時間差計算平均 HZ
        duration = times[-1] - times[0]
        if duration == 0:
            return 0.0
        return (len(times) - 1) / duration

    def update_display(self):
        # 清除終端機畫面 (Linux/macOS)
        os.system('clear')

        print("=" * 80)
        print(f" ROS2 Topic 即時狀態監控 (每秒更新)   現在時間: {time.strftime('%H:%M:%S')}")
        print("=" * 80)
        print(f"{'Topic 名稱':<15} | {'即時 HZ':<10} | {'最新內容摘要'}")
        print("-" * 80)

        for topic in ['/odom', '/scan', '/tf']:
            hz = self.calculate_hz(topic)
            hz_str = f"{hz:.1f} Hz" if hz > 0 else "0.0 Hz (斷線/未發布)"
            data_str = self.latest_data[topic]
            print(f"{topic:<15} | {hz_str:<10} | {data_str}")

        print("-" * 80)
        print("提示: Ctrl+C 可結束監控。")

def main(args=None):
    rclpy.init(args=args)
    node = TopicMonitor()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()