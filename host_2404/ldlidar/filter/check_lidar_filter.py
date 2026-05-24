import rclpy
from rclpy.node import Node
from sensor_msgs.msg import LaserScan
import sys

print("=============================================")
print("【系統提示】腳本已成功啟動，正在嘗試初始化 ROS 2...")
print("=============================================")
sys.stdout.flush()

class OllieLidarVerifier(Node):
    def __init__(self):
        super().__init__('ollie_lidar_verifier')
        
        print("【系統提示】ROS 2 節點建立成功，開始訂閱 Topic...")
        sys.stdout.flush()

        self.sub_filtered = self.create_subscription(
            LaserScan, '/scan', self.filtered_callback, 10)
        self.sub_raw = self.create_subscription(
            LaserScan, '/scan_raw', self.raw_callback, 10)

        print("【系統提示】訂閱完成！正在等待雷達數據流入...")
        print(" (如果卡在這一行，代表這個終端機視窗『聽不到』/scan 的資料) ")
        print("=============================================")
        sys.stdout.flush()
        
        self.raw_illegal_count = 0
        self.filtered_illegal_count = 0
        self.loop_count = 0

    def raw_callback(self, msg):
        illegal = [r for r in msg.ranges if 0.0 < r < 0.16]
        self.raw_illegal_count = len(illegal)

    def filtered_callback(self, msg):
        illegal = [r for r in msg.ranges if 0.0 < r < 0.16]
        self.filtered_illegal_count = len(illegal)
        
        self.loop_count += 1
        if self.loop_count % 10 == 0:
            self.print_report()

    def print_report(self):
        print(f"\r[Raw < 16cm]: {self.raw_illegal_count} 點  |  [Filtered < 16cm]: {self.filtered_filtered_status()}", end="")
        sys.stdout.flush()

    def filtered_filtered_status(self):
        if self.filtered_illegal_count > 0:
            return f"⚠️ {self.filtered_illegal_count} 點殘留"
        else:
            return "✓ 完美切除"

def main(args=None):
    rclpy.init(args=args)
    node = OllieLidarVerifier()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        print("\n使用者中斷執行。")
    finally:
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
