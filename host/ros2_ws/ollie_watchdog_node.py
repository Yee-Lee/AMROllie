#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
import subprocess
import time

class OllieWatchdogNode(Node):
    def __init__(self):
        super().__init__('ollie_watchdog_node')
        
        # --- Watchdog 參數設定 ---
        self.timeout_threshold = 5.0  # 幾秒沒收到資料判定為超時
        self.check_interval = 1.0     # 每秒檢查一次
        self.target_service = "ollie_microros.service"
        
        # 初始化狀態
        self.last_msg_time = time.time()
        self.is_restarting = False
        
        # 訂閱 /odom
        self.create_subscription(Odometry, '/odom', self.odom_callback, 10)
        
        # 建立定時檢查器
        self.timer = self.create_timer(self.check_interval, self.check_timeout)
        self.get_logger().info(f"Ollie 守門員已啟動！(超時閾值: {self.timeout_threshold} 秒)")

    def odom_callback(self, msg):
        # 只要收到資料，就更新最後收到訊息的時間
        self.last_msg_time = time.time()
        self.is_restarting = False

    def check_timeout(self):
        if self.is_restarting:
            return  # 正在重啟中，先不檢查
            
        elapsed_time = time.time() - self.last_msg_time
        
        if elapsed_time > self.timeout_threshold:
            self.get_logger().error(f"⚠️ 已經 {elapsed_time:.1f} 秒未收到 /odom 資料，準備重啟 {self.target_service} ...")
            self.is_restarting = True
            self.restart_microros()

    def restart_microros(self):
        try:
            # 執行 systemctl restart (執行此腳本的 User 需有免密碼 sudo 權限)
            subprocess.run(["sudo", "systemctl", "restart", self.target_service], check=True)
            self.get_logger().info("✅ 服務重啟成功！給予 10 秒寬限期等待系統恢復...")
            time.sleep(10.0)  # 給 Agent 一點時間重連
            self.last_msg_time = time.time()  # 重置計時器
            self.is_restarting = False
        except subprocess.CalledProcessError as e:
            self.get_logger().error(f"❌ 重啟服務失敗: {e}")

def main(args=None):
    rclpy.init(args=args)
    node = OllieWatchdogNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()