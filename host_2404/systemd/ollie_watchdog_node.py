#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, HistoryPolicy, DurabilityPolicy
from nav_msgs.msg import Odometry
import subprocess
import time
import os

class OllieWatchdogNode(Node):
    def __init__(self):
        super().__init__('ollie_watchdog_node')
        
        # --- Watchdog 參數設定 ---
        self.warning_threshold = 10.0 # 10秒沒收到資料發出警告
        self.restart_threshold = 15.0 # 15秒沒收到資料觸發重啟
        self.check_interval = 1.0     # 每秒檢查一次
        self.retry_interval = 20.0    # 離線狀態下，每隔幾秒再次嘗試重啟 (稍微放寬)
        self.target_service = "ollie_microros.service"
        
        # 初始化狀態
        self.last_msg_time = time.time()
        self.is_restarting = False
        self.is_offline = True  # 預設為離線，直到收到第一筆資料才改為正常
        self.has_warned = False # 是否已針對當前斷訊發出過警告
        
        # 建立與硬體匹配的 QoS Profile (Reliable + Volatile)
        qos_profile = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            history=HistoryPolicy.KEEP_LAST,
            depth=10,
            durability=DurabilityPolicy.VOLATILE
        )
        
        # 訂閱 /odom
        self.create_subscription(
            Odometry, 
            '/odom', 
            self.odom_callback, 
            qos_profile
        )
        
        # 建立定時檢查器
        self.timer = self.create_timer(self.check_interval, self.check_timeout)
        
        # 取得並顯示目前的 ROS_DOMAIN_ID
        domain_id = os.environ.get('ROS_DOMAIN_ID', '未設定 (預設 0)')
        self.get_logger().info(f"🛡️ Ollie 守門員已啟動！")
        self.get_logger().info(f"🌐 目前 ROS_DOMAIN_ID: {domain_id}")
        self.get_logger().info(f"⏰ 設定 - 警告: {self.warning_threshold}s, 重啟: {self.restart_threshold}s")
        self.get_logger().info(f"⏳ 等待 Odom 數據中...")

    def odom_callback(self, msg):
        # 如果原本是離線或剛重啟完，現在收到資料了，就印出「恢復通訊」的明確訊息
        if self.is_offline:
            self.get_logger().info("✅ 系統通訊正常！開始接收 /odom 數據。")
            self.is_offline = False

        # 只要收到資料，就更新最後收到訊息的時間
        self.last_msg_time = time.time()
        self.is_restarting = False
        self.has_warned = False # 重置警告狀態

    def check_timeout(self):
        if self.is_restarting:
            return  # 正在重啟中，先不檢查
            
        elapsed_time = time.time() - self.last_msg_time
        
        # 第一階段：警告 (10s)
        if elapsed_time > self.warning_threshold and not self.has_warned and not self.is_offline:
            self.get_logger().warn(f"⚠️ 注意：已經 {elapsed_time:.1f} 秒未收到 /odom 資料...")
            self.has_warned = True

        # 第二階段：重啟 (15s)
        if elapsed_time > self.restart_threshold:
            if not self.is_offline:
                # 原本連線正常，突然斷線：觸發重啟
                self.get_logger().error(f"🚨 嚴重：{elapsed_time:.1f} 秒未收到數據，準備重啟 {self.target_service} ...")
                self.is_offline = True
                self.is_restarting = True
                self.restart_microros()
            else:
                # 已經是離線狀態，檢查是否超過再次重啟的閾值
                if elapsed_time > self.retry_interval:
                    self.get_logger().error(f"⚠️ 離線狀態持續 {elapsed_time:.1f} 秒，再次嘗試重啟 {self.target_service} ...")
                    self.is_restarting = True
                    self.restart_microros()
                else:
                    self.get_logger().info(f"⏳ 持續等待 /odom 恢復中... (已斷線 {elapsed_time:.1f} 秒)", throttle_duration_sec=5.0)

    def restart_microros(self):
        try:
            self.get_logger().info("🔄 正在執行 systemctl restart...")
            
            # 紀錄到專屬日誌檔
            log_msg = f"[{time.ctime()}] Watchdog triggered restart of {self.target_service} due to Odom timeout.\n"
            try:
                with open("/var/log/ollie_watchdog.log", "a") as f:
                    f.write(log_msg)
            except Exception as log_e:
                self.get_logger().warn(f"無法寫入日誌檔 /var/log/ollie_watchdog.log: {log_e}")

            # 服務現在以 root 執行，不需要 sudo
            subprocess.run(["systemctl", "restart", self.target_service], check=True)
            self.get_logger().info("🔄 服務重啟命令發送成功！給予 10 秒寬限期等待 Micro-ROS Agent 啟動...")
            time.sleep(10.0)  # 給 Agent 一點時間啟動與重連
        except subprocess.CalledProcessError as e:
            self.get_logger().error(f"❌ 重啟服務失敗: {e}")
        finally:
            self.last_msg_time = time.time()  # 重置計時器
            self.is_restarting = False

def main(args=None):
    rclpy.init(args=args)
    node = OllieWatchdogNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
