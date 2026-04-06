#include "task.h"
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/range.h>
#include <std_srvs/srv/trigger.h>

// --- micro-ROS 相關實體 ---
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

rcl_publisher_t pub_odom;
rcl_publisher_t pub_sonar_left;
rcl_publisher_t pub_sonar_right;
rcl_subscription_t sub_cmd_vel;
rcl_service_t srv_reset_odom;
rcl_timer_t ros_timer;

geometry_msgs__msg__Twist msg_cmd_vel;
nav_msgs__msg__Odometry msg_odom;
sensor_msgs__msg__Range msg_sonar_left;
sensor_msgs__msg__Range msg_sonar_right;
std_srvs__srv__Trigger_Request req_reset_odom;
std_srvs__srv__Trigger_Response res_reset_odom;

// --- 4. micro-ROS Callback 函式 ---

// 接收上位機速度指令
void cmd_vel_callback(const void * msgin) {
    const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;
    portENTER_CRITICAL(&mux);
    cmd_target_v = msg->linear.x;
    cmd_target_w = msg->angular.z;
    portEXIT_CRITICAL(&mux);
}

// 接收上位機歸零里程計請求
void reset_odom_callback(const void * req, void * res) {
    portENTER_CRITICAL(&mux);
    flag_reset_odom = true;
    portEXIT_CRITICAL(&mux);
    
    std_srvs__srv__Trigger_Response * response = (std_srvs__srv__Trigger_Response *)res;
    response->success = true;
}

// 定期發布 Odom 與 Sonar (10Hz)
void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
    if (timer != NULL) {
        portENTER_CRITICAL(&mux);
        // 安全讀取最新硬體狀態
        float current_x = odom_x, current_y = odom_y, current_theta = odom_theta;
        float current_v = actual_v, current_w = actual_w;
        float s_left = sonar_left_dist, s_right = sonar_right_dist;
        portEXIT_CRITICAL(&mux);

        // 1. 填入 Odom 位置與姿態 (將 Yaw 轉換為四元數)
        msg_odom.pose.pose.position.x = current_x;
        msg_odom.pose.pose.position.y = current_y;
        msg_odom.pose.pose.position.z = 0.0;
        msg_odom.pose.pose.orientation.x = 0.0;
        msg_odom.pose.pose.orientation.y = 0.0;
        msg_odom.pose.pose.orientation.z = sin(current_theta / 2.0f);
        msg_odom.pose.pose.orientation.w = cos(current_theta / 2.0f);

        // 2. 填入 Odom 速度
        msg_odom.twist.twist.linear.x = current_v;
        msg_odom.twist.twist.angular.z = current_w;

        // 發布 Odom
        rcl_publish(&pub_odom, &msg_odom, NULL);

        // 發布超音波資料
        msg_sonar_left.range = s_left;
        rcl_publish(&pub_sonar_left, &msg_sonar_left, NULL);
        msg_sonar_right.range = s_right;
        rcl_publish(&pub_sonar_right, &msg_sonar_right, NULL);
    }
}

// taskROS (Core 1 - 中優先級)
void taskROS(void *pvParameters) {
    // 設定 micro-ROS 傳輸層 (使用預設的 Serial 進行 UART 通訊)
    set_microros_serial_transports(Serial);
    delay(2000); // 等待連線穩定

    allocator = rcl_get_default_allocator();

    // 建立 support 與 node
    rclc_support_init(&support, 0, NULL, &allocator);
    rclc_node_init_default(&node, "base_controller_node", "", &support);

    // --- 初始化 Publisher: /odom ---
    rclc_publisher_init_default(
        &pub_odom, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "odom");

    // 設定 Odom 訊息的靜態 Frame ID
    msg_odom.header.frame_id.data = (char*)"odom";
    msg_odom.header.frame_id.size = strlen("odom");
    msg_odom.header.frame_id.capacity = msg_odom.header.frame_id.size + 1;

    msg_odom.child_frame_id.data = (char*)"base_link";
    msg_odom.child_frame_id.size = strlen("base_link");
    msg_odom.child_frame_id.capacity = msg_odom.child_frame_id.size + 1;

    // --- 初始化 Publisher: /sonar/left 與 /sonar/right ---
    rclc_publisher_init_default(
        &pub_sonar_left, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/left");
        
    rclc_publisher_init_default(
        &pub_sonar_right, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/right");

    // 設定超音波訊息的固定參數 (不隨時間改變的屬性)
    msg_sonar_left.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_left.min_range = 0.02f; // 2cm
    msg_sonar_left.max_range = 4.00f; // 400cm
    
    msg_sonar_right.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_right.min_range = 0.02f;
    msg_sonar_right.max_range = 4.00f;

    // --- 初始化 Subscriber: /cmd_vel ---
    rclc_subscription_init_default(
        &sub_cmd_vel, &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel");

    // --- 初始化 Service: /reset_odom ---
    rclc_service_init_default(
        &srv_reset_odom, &node,
        ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, Trigger), "reset_odom");

    // --- 初始化 Timer (10Hz = 100ms) ---
    rclc_timer_init_default(&ros_timer, &support, RCL_MS_TO_NS(100), timer_callback);

    // --- 初始化 Executor ---
    // 參數 3 代表 Handles 數量: 包含 1 個 Timer, 1 個 Subscriber, 1 個 Service
    rclc_executor_init(&executor, &support.context, 3, &allocator);
    rclc_executor_add_timer(&executor, &ros_timer);
    rclc_executor_add_subscription(&executor, &sub_cmd_vel, &msg_cmd_vel, &cmd_vel_callback, ON_NEW_DATA);
    rclc_executor_add_service(&executor, &srv_reset_odom, &req_reset_odom, &res_reset_odom, reset_odom_callback);
    
    while(1) {
        // 執行 micro-ROS 事件迴圈
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
        vTaskDelay(pdMS_TO_TICKS(1)); // 避免觸發 Task Watchdog
    }
}