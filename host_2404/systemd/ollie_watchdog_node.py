#!/usr/bin/env python3
"""
AMROllie Watchdog Node v3.0
--------------------------
此節點負責監控 ESP32 (Micro-ROS) 的連線狀態。
技術註記：
由於 ROS 2 Jazzy 在 ESP32 與主機間存在 Type Hash 不匹配問題 (INVALID Hash)，
傳統的訂閱回調 (Subscription Callback) 無法觸發。
因此，本節點採用「拓樸監控模式」(Topology Monitoring)，透過偵測 DDS 發布者是否存在來判斷狀態。
"""

import os
import time
import subprocess
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry

class OllieWatchdogNode(Node):
    def __init__(self):
        # 使用包含 PID 的節點名稱以避免衝突
        node_name = f'ollie_watchdog_{os.getpid()}'
        super().__init__(node_name)
        
        # --- 宣告 ROS 2 參數 (可透過 launch 或命令行覆寫) ---
        self.declare_parameter('restart_threshold', 15.0)  # 判定失蹤並觸發重啟的秒數
        self.declare_parameter('check_interval', 2.0)     # 掃描拓樸圖的頻率
        self.declare_parameter('target_service', 'ollie_microros.service')
        self.declare_parameter('topic_name', '/odom')
        
        # 取得參數值
        self.restart_threshold = self.get_parameter('restart_threshold').value
        self.check_interval = self.get_parameter('check_interval').value
        self.target_service = self.get_parameter('target_service').value
        self.topic_name = self.get_parameter('topic_name').value
        
        # 初始化內部狀態
        self.last_alive_time = time.time()
        self.is_offline = True
        self.restart_count = 0
        
        # 建立定時監控任務
        self.timer = self.create_timer(self.check_interval, self.monitor_cycle)
        
        # 建立一個嘗試性的訂閱 (雖因 Hash 問題通常無效，但保留作為極端情況下的信號)
        self.sub = self.create_subscription(
            Odometry, self.topic_name, self.odom_callback, 10
        )
        
        # 啟動資訊
        domain_id = os.environ.get('ROS_DOMAIN_ID', '30')
        self.get_logger().info("==========================================")
        self.get_logger().info(f"🛡️ AMROllie Watchdog v3.0 已啟動")
        self.get_logger().info(f"🌐 ROS_DOMAIN_ID: {domain_id}")
        self.get_logger().info(f"🔍 監控話題: {self.topic_name}")
        self.get_logger().info(f"⏰ 重啟閾值: {self.restart_threshold}s")
        self.get_logger().info(f"🛠️ 目標服務: {self.target_service}")
        self.get_logger().info("==========================================")

    def odom_callback(self, _msg):
        """若底層通訊突然匹配，此回調能提供最即時的活躍訊號"""
        self.update_alive_status("CALLBACK")

    def monitor_cycle(self):
        """核心監控循環：檢查發布者是否存在"""
        try:
            publishers_info = self.get_publishers_info_by_topic(self.topic_name)
            pub_count = len(publishers_info)
            
            if pub_count > 0:
                self.update_alive_status("TOPOLOGY")
            else:
                self.handle_absence()
                
        except Exception as e:
            self.get_logger().error(f"監控循環發生異常: {str(e)}")

    def update_alive_status(self, source):
        """更新活躍時間並處理狀態轉換"""
        now = time.time()
        self.last_alive_time = now
        
        if self.is_offline:
            self.get_logger().info(f"✅ [SUCCESS] 偵測到發布者恢復連線 (來源: {source})")
            self.is_offline = False

    def handle_absence(self):
        """處理發布者失蹤的情況"""
        elapsed = time.time() - self.last_alive_time
        
        # 每 6 秒打印一次警告，避免洗板
        self.get_logger().warn(
            f"⚠️ [WARNING] 找不到 {self.topic_name} 發布者！(已失蹤 {elapsed:.1f}s)", 
            throttle_duration_sec=6.0
        )
        
        if elapsed > self.restart_threshold:
            self.get_logger().error(f"🚨 [CRITICAL] 斷訊時間達 {elapsed:.1f}s，執行重啟程序...")
            self.trigger_restart()

    def trigger_restart(self):
        """執行系統服務重啟"""
        try:
            self.restart_count += 1
            self.get_logger().info(f"🔄 [RESTART #{self.restart_count}] 正在重啟 {self.target_service}...")
            
            # 使用 Popen 異步執行，避免阻塞 Watchdog 自身
            subprocess.Popen(["systemctl", "restart", self.target_service])
            
            # 重設計時器並標記為離線
            self.last_alive_time = time.time()
            self.is_offline = True
            
        except Exception as e:
            self.get_logger().error(f"重啟服務失敗: {str(e)}")

def main(args=None):
    rclpy.init(args=args)
    node = OllieWatchdogNode()
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()

if __name__ == '__main__':
    main()
