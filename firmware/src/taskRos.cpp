#include "task.h"
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/range.h>
#include <std_srvs/srv/trigger.h>
#include <FastLED.h>

// --- FastLED 設定 ---
CRGB leds[NUM_LEDS];

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

// 定期發布 Odom 與 Sonar (20Hz)
void timer_callback(rcl_timer_t * timer, int64_t last_call_time) {
    if (timer != NULL) {
        portENTER_CRITICAL(&mux);
        // 安全讀取最新硬體狀態
        float current_x = odom_x, current_y = odom_y, current_theta = odom_theta;
        float current_v = actual_v, current_w = actual_w;
        float s_left = sonar_left_dist, s_right = sonar_right_dist;
        portEXIT_CRITICAL(&mux);

        // 取得與 RPI3B (Agent) 同步的系統時間 (奈秒)
        int64_t time_ns = rmw_uros_epoch_nanos();
        int32_t sec = (int32_t)(time_ns / 1000000000);
        uint32_t nanosec = (uint32_t)(time_ns % 1000000000);

        // 每秒 (相當於 20 次 callback) 透過序列埠印出一次時間戳記狀態，確認是否同步
        static int log_counter = 0;
        if (log_counter++ >= 20) {
            Serial.printf("[ROS] Sync: %s | Epoch Sec: %d\n", rmw_uros_epoch_synchronized() ? "YES" : "NO", sec);
            log_counter = 0;
        }

        // 替所有的 Header 蓋上時間戳記
        msg_odom.header.stamp.sec = sec;
        msg_odom.header.stamp.nanosec = nanosec;
        msg_sonar_left.header.stamp.sec = sec;
        msg_sonar_left.header.stamp.nanosec = nanosec;
        msg_sonar_right.header.stamp.sec = sec;
        msg_sonar_right.header.stamp.nanosec = nanosec;

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
        (void)rcl_publish(&pub_odom, &msg_odom, NULL);

        // 發布超音波資料
        msg_sonar_left.range = s_left / 100.0f; // 單位轉換：公分 (cm) 轉為公尺 (m)
        (void)rcl_publish(&pub_sonar_left, &msg_sonar_left, NULL);
        msg_sonar_right.range = s_right / 100.0f; // 單位轉換：公分 (cm) 轉為公尺 (m)
        (void)rcl_publish(&pub_sonar_right, &msg_sonar_right, NULL);
    }
}

// 建立 ROS 實體
bool create_entities() {
    allocator = rcl_get_default_allocator();

    // --- 設定 ROS_DOMAIN_ID 為 30 ---
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    rcl_init_options_init(&init_options, allocator);
    rcl_init_options_set_domain_id(&init_options, 30);

    // 使用自訂的 options 建立 support，然後釋放 options 記憶體
    if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) != RCL_RET_OK) {
        return false;
    }
    rcl_init_options_fini(&init_options);

    if (rclc_node_init_default(&node, "base_controller_node", "", &support) != RCL_RET_OK) return false;

    // --- 初始化 Publisher: /odom ---
    if (rclc_publisher_init_best_effort(&pub_odom, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "odom") != RCL_RET_OK) return false;

    // 設定 Odom 訊息的靜態 Frame ID
    msg_odom.header.frame_id.data = (char*)"odom";
    msg_odom.header.frame_id.size = strlen("odom");
    msg_odom.header.frame_id.capacity = msg_odom.header.frame_id.size + 1;
    msg_odom.child_frame_id.data = (char*)"base_link";
    msg_odom.child_frame_id.size = strlen("base_link");
    msg_odom.child_frame_id.capacity = msg_odom.child_frame_id.size + 1;

    // --- 初始化 Publisher: /sonar/left 與 /sonar/right ---
    if (rclc_publisher_init_best_effort(&pub_sonar_left, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/left") != RCL_RET_OK) return false;
    if (rclc_publisher_init_best_effort(&pub_sonar_right, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/right") != RCL_RET_OK) return false;

    // 設定超音波訊息的固定參數
    msg_sonar_left.header.frame_id.data = (char*)"left_ultrasonic_link";
    msg_sonar_left.header.frame_id.size = strlen("left_ultrasonic_link");
    msg_sonar_left.header.frame_id.capacity = msg_sonar_left.header.frame_id.size + 1;
    msg_sonar_left.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_left.field_of_view = 0.26f;
    msg_sonar_left.min_range = 0.02f;
    msg_sonar_left.max_range = 4.00f;

    msg_sonar_right.header.frame_id.data = (char*)"right_ultrasonic_link";
    msg_sonar_right.header.frame_id.size = strlen("right_ultrasonic_link");
    msg_sonar_right.header.frame_id.capacity = msg_sonar_right.header.frame_id.size + 1;
    msg_sonar_right.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_right.field_of_view = 0.26f;
    msg_sonar_right.min_range = 0.02f;
    msg_sonar_right.max_range = 4.00f;

    // --- 初始化 Subscriber: /cmd_vel ---
    if (rclc_subscription_init_best_effort(&sub_cmd_vel, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel") != RCL_RET_OK) return false;

    // --- 初始化 Service: /reset_odom ---
    if (rclc_service_init_default(&srv_reset_odom, &node, ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, Trigger), "reset_odom") != RCL_RET_OK) return false;

    // --- 初始化 Timer (20Hz = 50ms) ---
    if (rclc_timer_init_default(&ros_timer, &support, RCL_MS_TO_NS(50), timer_callback) != RCL_RET_OK) return false;

    // --- 初始化 Executor ---
    if (rclc_executor_init(&executor, &support.context, 3, &allocator) != RCL_RET_OK) return false;
    if (rclc_executor_add_timer(&executor, &ros_timer) != RCL_RET_OK) return false;
    if (rclc_executor_add_subscription(&executor, &sub_cmd_vel, &msg_cmd_vel, &cmd_vel_callback, ON_NEW_DATA) != RCL_RET_OK) return false;
    if (rclc_executor_add_service(&executor, &srv_reset_odom, &req_reset_odom, &res_reset_odom, reset_odom_callback) != RCL_RET_OK) return false;

    return true;
}

// 非阻塞 LED 更新邏輯
void updateLED(AgentState state) {
    portENTER_CRITICAL(&mux);
    // 讀取由 taskBaseController 同步的全域緊急煞停狀態
    bool is_braking = status_emergency_brake;
    portEXIT_CRITICAL(&mux);

    if (state == WAITING_AGENT) {
        // 藍色呼吸燈 (未連線狀態)
        float breath = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
        leds[0] = CRGB::Blue;
        FastLED.setBrightness(breath);
    } else if (state == AGENT_CONNECTED) {
        if (is_braking) {
            // 紅色急閃 5Hz (連線中，但底層觸發了防撞/煞停機制)
            if (millis() % 200 < 100) {
                leds[0] = CRGB::Red;
                FastLED.setBrightness(255);
            } else {
                leds[0] = CRGB::Black;
                FastLED.setBrightness(0);
            }
        } else {
            // 綠色常亮 (正常運作狀態)
            leds[0] = CRGB::Green;
            FastLED.setBrightness(100); // 調整至適當亮度避免過亮
        }
    }
    FastLED.show();
}

// taskROS (Core 1 - 中優先級)
void taskROS(void *pvParameters) {
    // 初始化 FastLED
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.clear();
    FastLED.show();

    // 設定 micro-ROS 傳輸層 (改用 Serial2 與 RPi 通訊)
    set_microros_serial_transports(Serial2);
    delay(2000); // 等待連線穩定

    AgentState state = WAITING_AGENT;
    unsigned long last_sync_try = 0;

    while(1) {
        switch (state) {
            case WAITING_AGENT:
                // 狀態機：等待並檢查 Agent 連線 (Timeout = 100ms, 非阻塞)
                if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) {
                    Serial.println("[ROS] micro-ROS Agent found! Creating entities...");
                    if (create_entities()) {
                        Serial.println("[ROS] Entities created successfully.");
                        state = AGENT_CONNECTED;

                        // 嘗試初步時間同步
                        for (int i = 0; i < 5; i++) {
                            rmw_uros_sync_session(100);
                            if (rmw_uros_epoch_synchronized()) break;
                            vTaskDelay(pdMS_TO_TICKS(10));
                        }
                    } else {
                        Serial.println("[ROS] Failed to create entities. Retrying in next loop...");
                        // 注意：如果實體建立失敗，將停留在 WAITING_AGENT 狀態，並在下一次迴圈重試。
                        // (後續階段將在此處補充 destroy_entities 以確保記憶體不外洩)
                    }
                }
                break;

            case AGENT_CONNECTED:
                // 狀態機：Agent 已連線，處理通訊與時間同步
                if (!rmw_uros_epoch_synchronized()) {
                    unsigned long now = millis();
                    if (now - last_sync_try > 1000) {
                        rmw_uros_sync_session(10);
                        last_sync_try = now;
                    }
                }

                // 執行 micro-ROS 事件迴圈
                rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

                // (後續階段將在此處補充 Agent 斷線檢查邏輯)
                break;
        }

        // 每次迴圈結束前更新狀態指示燈，確保視覺回饋不延遲
        updateLED(state);

        vTaskDelay(pdMS_TO_TICKS(10)); // 讓出 CPU，避免觸發 Task Watchdog
    }
}