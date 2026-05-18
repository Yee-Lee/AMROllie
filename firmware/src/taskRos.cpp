#include "task.h"
#include <micro_ros_platformio.h>
#include <micro_ros_utilities/string_utilities.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>

#include <geometry_msgs/msg/twist.h>
#include <nav_msgs/msg/odometry.h>
#include <sensor_msgs/msg/range.h>
#include <tf2_msgs/msg/tf_message.h>
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
rcl_publisher_t pub_tf;
rcl_subscription_t sub_cmd_vel;
rcl_service_t srv_reset_odom;
rcl_timer_t ros_timer;

geometry_msgs__msg__Twist msg_cmd_vel;
nav_msgs__msg__Odometry msg_odom;
sensor_msgs__msg__Range msg_sonar_left;
sensor_msgs__msg__Range msg_sonar_right;
tf2_msgs__msg__TFMessage msg_tf;
geometry_msgs__msg__TransformStamped tf_array[1]; // TF 陣列記憶體
std_srvs__srv__Trigger_Request req_reset_odom;
std_srvs__srv__Trigger_Response res_reset_odom;

// --- 4. micro-ROS Callback 函式 ---

// 接收上位機速度指令
void cmd_vel_callback(const void * msgin) {
    const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;

    portENTER_CRITICAL(&mux);
    cmd_target_v = msg->linear.x;
    cmd_target_w = msg->angular.z;
    last_cmd_vel_time = millis(); // 更新指令接收時間
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

        // 替所有的 Header 蓋上時間戳記
        msg_odom.header.stamp.sec = sec;
        msg_odom.header.stamp.nanosec = nanosec;
        msg_sonar_left.header.stamp.sec = sec;
        msg_sonar_left.header.stamp.nanosec = nanosec;
        msg_sonar_right.header.stamp.sec = sec;
        msg_sonar_right.header.stamp.nanosec = nanosec;
        msg_tf.transforms.data[0].header.stamp.sec = sec;
        msg_tf.transforms.data[0].header.stamp.nanosec = nanosec;

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

        // 3. 填入 TF 轉換 (odom -> base_link)
        msg_tf.transforms.data[0].transform.translation.x = current_x;
        msg_tf.transforms.data[0].transform.translation.y = current_y;
        msg_tf.transforms.data[0].transform.translation.z = 0.0;
        
        msg_tf.transforms.data[0].transform.rotation.x = 0.0;
        msg_tf.transforms.data[0].transform.rotation.y = 0.0;
        msg_tf.transforms.data[0].transform.rotation.z = sin(current_theta / 2.0f);
        msg_tf.transforms.data[0].transform.rotation.w = cos(current_theta / 2.0f);

        // 發布 Odom
        rcl_ret_t ret_odom = rcl_publish(&pub_odom, &msg_odom, NULL);
        if (ret_odom != RCL_RET_OK) {
            static int error_counter_odom = 0;
            if (error_counter_odom++ >= 50) { // 每 5 秒印一次 (10Hz * 50 = 5s)
                Serial.printf("[ROS] Publish Odom failed: %d\n", (int)ret_odom);
                error_counter_odom = 0;
            }
        }

        // 發布 TF
        rcl_ret_t ret_tf = rcl_publish(&pub_tf, &msg_tf, NULL);
        if (ret_tf != RCL_RET_OK) {
            static int error_counter_tf = 0;
            if (error_counter_tf++ >= 50) {
                Serial.printf("[ROS] Publish TF failed: %d\n", (int)ret_tf);
                error_counter_tf = 0;
            }
        }

        // 發布超音波資料
        msg_sonar_left.range = s_left / 100.0f; // 單位轉換：公分 (cm) 轉為公尺 (m)
        rcl_ret_t ret_sl = rcl_publish(&pub_sonar_left, &msg_sonar_left, NULL);
        if (ret_sl != RCL_RET_OK) {
            static int error_counter_sl = 0;
            if (error_counter_sl++ >= 50) {
                Serial.printf("[ROS] Publish Sonar L failed: %d\n", (int)ret_sl);
                error_counter_sl = 0;
            }
        }

        msg_sonar_right.range = s_right / 100.0f; // 單位轉換：公分 (cm) 轉為公尺 (m)
        rcl_ret_t ret_sr = rcl_publish(&pub_sonar_right, &msg_sonar_right, NULL);
        if (ret_sr != RCL_RET_OK) {
            static int error_counter_sr = 0;
            if (error_counter_sr++ >= 50) {
                Serial.printf("[ROS] Publish Sonar R failed: %d\n", (int)ret_sr);
                error_counter_sr = 0;
            }
        }
    }
}

// 建立 ROS 實體
bool create_entities() {
    allocator = rcl_get_default_allocator();

    // --- 設定 ROS_DOMAIN_ID 為 30 ---
    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    (void)rcl_init_options_init(&init_options, allocator);
    (void)rcl_init_options_set_domain_id(&init_options, 30);

    // 使用自訂的 options 建立 support，然後釋放 options 記憶體
    if (rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator) != RCL_RET_OK) {
        return false;
    }
    (void)rcl_init_options_fini(&init_options);

    if (rclc_node_init_default(&node, "base_controller_node", "", &support) != RCL_RET_OK) return false;

    // --- 初始化 Publisher: /odom (改為 Default/Reliable QoS 以符合 ROS 2 上位機預設標準) ---
    if (rclc_publisher_init_default(&pub_odom, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry), "odom") != RCL_RET_OK) return false;

    // 設定 Odom 訊息的靜態 Frame ID 與初始化協方差矩陣為 0
    msg_odom.header.frame_id = micro_ros_string_utilities_init("odom");
    msg_odom.child_frame_id = micro_ros_string_utilities_init("base_link");
    for (int i = 0; i < 36; i++) {
        msg_odom.pose.covariance[i] = 0.0;
        msg_odom.twist.covariance[i] = 0.0;
    }

    // --- 初始化 Publisher: /tf ---
    if (rclc_publisher_init_default(&pub_tf, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(tf2_msgs, msg, TFMessage), "/tf") != RCL_RET_OK) return false;
    
    // 設定 TF 陣列記憶體與靜態字串
    msg_tf.transforms.data = tf_array;
    msg_tf.transforms.size = 1;
    msg_tf.transforms.capacity = 1;
    msg_tf.transforms.data[0].header.frame_id = micro_ros_string_utilities_init("odom");
    msg_tf.transforms.data[0].child_frame_id = micro_ros_string_utilities_init("base_link");

    // --- 初始化 Publisher: /sonar/left 與 /sonar/right ---
    if (rclc_publisher_init_best_effort(&pub_sonar_left, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/left") != RCL_RET_OK) return false;
    if (rclc_publisher_init_best_effort(&pub_sonar_right, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Range), "sonar/right") != RCL_RET_OK) return false;

    // 設定超音波訊息的固定參數
    msg_sonar_left.header.frame_id = micro_ros_string_utilities_init("left_ultrasonic_link");
    msg_sonar_left.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_left.field_of_view = 0.26f;
    msg_sonar_left.min_range = 0.02f;
    msg_sonar_left.max_range = 4.00f;

    msg_sonar_right.header.frame_id = micro_ros_string_utilities_init("right_ultrasonic_link");
    msg_sonar_right.radiation_type = sensor_msgs__msg__Range__ULTRASOUND;
    msg_sonar_right.field_of_view = 0.26f;
    msg_sonar_right.min_range = 0.02f;
    msg_sonar_right.max_range = 4.00f;

    // --- 初始化 Subscriber: /cmd_vel ---
    if (rclc_subscription_init_best_effort(&sub_cmd_vel, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "cmd_vel") != RCL_RET_OK) return false;

    // --- 初始化 Service: /reset_odom ---
    if (rclc_service_init_default(&srv_reset_odom, &node, ROSIDL_GET_SRV_TYPE_SUPPORT(std_srvs, srv, Trigger), "reset_odom") != RCL_RET_OK) return false;

    // --- 初始化 Timer (10Hz = 100ms，降低頻率以節省 Serial 頻寬) ---
    if (rclc_timer_init_default(&ros_timer, &support, RCL_MS_TO_NS(100), timer_callback) != RCL_RET_OK) return false;

    // --- 初始化 Executor ---
    if (rclc_executor_init(&executor, &support.context, 3, &allocator) != RCL_RET_OK) return false;
    if (rclc_executor_add_timer(&executor, &ros_timer) != RCL_RET_OK) return false;
    if (rclc_executor_add_subscription(&executor, &sub_cmd_vel, &msg_cmd_vel, &cmd_vel_callback, ON_NEW_DATA) != RCL_RET_OK) return false;
    if (rclc_executor_add_service(&executor, &srv_reset_odom, &req_reset_odom, &res_reset_odom, reset_odom_callback) != RCL_RET_OK) return false;

    return true;
}

// 銷毀 ROS 實體，釋放記憶體
void destroy_entities() {
    rmw_context_t * rmw_context = rcl_context_get_rmw_context(&support.context);
    (void) rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);

    (void) rcl_publisher_fini(&pub_odom, &node);
    (void) rcl_publisher_fini(&pub_sonar_left, &node);
    (void) rcl_publisher_fini(&pub_sonar_right, &node);
    (void) rcl_publisher_fini(&pub_tf, &node);
    (void) rcl_subscription_fini(&sub_cmd_vel, &node);
    (void) rcl_service_fini(&srv_reset_odom, &node);
    (void) rcl_timer_fini(&ros_timer);
    (void) rclc_executor_fini(&executor);
    (void) rcl_node_fini(&node);
    (void) rclc_support_fini(&support);

    // 釋放動態分配的字串記憶體
    micro_ros_string_utilities_destroy(&msg_odom.header.frame_id);
    micro_ros_string_utilities_destroy(&msg_odom.child_frame_id);
    micro_ros_string_utilities_destroy(&msg_sonar_left.header.frame_id);
    micro_ros_string_utilities_destroy(&msg_sonar_right.header.frame_id);
    micro_ros_string_utilities_destroy(&msg_tf.transforms.data[0].header.frame_id);
    micro_ros_string_utilities_destroy(&msg_tf.transforms.data[0].child_frame_id);
}

// 非阻塞 LED 更新邏輯
void updateLED(AgentState agent_state) {
    portENTER_CRITICAL(&mux);
    // 讀取由 taskBaseController 同步的全域感測器等級
    SensorStatus sensor_status = current_sensor_status;
    portEXIT_CRITICAL(&mux);

    // --- 優先級一：連線狀況 (系統健康度) ---
    if (agent_state == WAITING_AGENT) {
        // 藍色呼吸燈 (未連線狀態)
        float breath = (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0;
        leds[0] = CRGB::Blue;
        FastLED.setBrightness(breath);
    } 
    else if (agent_state == AGENT_LOSING) {
        // 黃色閃爍 2Hz (懷疑斷線，Debounce 中)
        if (millis() % 500 < 250) {
            leds[0] = CRGB::Yellow;
            FastLED.setBrightness(150);
        } else {
            leds[0] = CRGB::Black;
            FastLED.setBrightness(0);
        }
    } 
    else if (agent_state == AGENT_DISCONNECTED) {
        // 斷線清理瞬間，顯示紅色常亮
        leds[0] = CRGB::Red;
        FastLED.setBrightness(100);
    }
    // --- 優先級二：感測器狀態 (僅在連線正常時顯示) ---
    else if (agent_state == AGENT_CONNECTED) {
        if (sensor_status == SENSOR_BRAKE) {
            // 紅色急閃 5Hz (危險煞停區)
            if (millis() % 200 < 100) {
                leds[0] = CRGB::Red;
                FastLED.setBrightness(255);
            } else {
                leds[0] = CRGB::Black;
                FastLED.setBrightness(0);
            }
        } 
        else if (sensor_status == SENSOR_WARNING) {
            // 橘色常亮 (警告減速區)
            leds[0] = CRGB::Orange;
            FastLED.setBrightness(150);
        } 
        else {
            // 綠色常亮 (安全區域)
            leds[0] = CRGB::Green;
            FastLED.setBrightness(100);
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

    unsigned long last_sync_try = 0;
    
    // Ping 與逾時追蹤變數
    unsigned long last_ping_time = millis();
    unsigned long last_wait_log_time = millis();
    unsigned long waiting_start_time = millis();
    int ping_missed_count = 0;

    while(1) {
        // 安全讀取全域連線狀態
        portENTER_CRITICAL(&mux);
        AgentState local_state_copy = current_agent_state;
        portEXIT_CRITICAL(&mux);

        switch (local_state_copy) {
            case WAITING_AGENT:
                // 狀態機：等待並檢查 Agent 連線
                
                // 1. 每 5 秒列印一次 Waiting 提示與重啟倒數
                if (millis() - last_wait_log_time > 5000) {
                    last_wait_log_time = millis();
                    unsigned long waited_seconds = (millis() - waiting_start_time) / 1000;
                    Serial.printf("[ROS] Waiting for micro-ROS Agent for %lu seconds... (30s reset)\n", waited_seconds);
                }

                // 2. Timeout 保護機制：如果等待超過 30 秒，自動重啟 ESP32
                if (millis() - waiting_start_time > 30000) {
                    Serial.println("[FATAL] Agent wait timeout (30 seconds). Rebooting ESP32...");
                    delay(100); // 給 UART 緩衝區一點時間送出字串
                    ESP.restart();
                }

                // 3. 低頻重試：每 3000ms 檢查一次連線
                if (millis() - last_ping_time > 3000) {
                    last_ping_time = millis();
                    if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) {
                        Serial.println("[ROS] micro-ROS Agent found! Creating entities...");
                        if (create_entities()) {
                            Serial.println("[ROS] Entities created successfully.");
                            portENTER_CRITICAL(&mux);
                            current_agent_state = AGENT_CONNECTED;
                            portEXIT_CRITICAL(&mux);
                            local_state_copy = AGENT_CONNECTED;
                            ping_missed_count = 0;

                            // 嘗試初步時間同步
                            for (int i = 0; i < 5; i++) {
                                (void)rmw_uros_sync_session(100);
                                if (rmw_uros_epoch_synchronized()) break;
                                vTaskDelay(pdMS_TO_TICKS(10));
                            }
                        } else {
                            Serial.println("[ROS] Failed to create entities. Retrying in next loop...");
                            portENTER_CRITICAL(&mux);
                            current_agent_state = AGENT_DISCONNECTED; // 建立失敗，立刻進入清理狀態
                            portEXIT_CRITICAL(&mux);
                            local_state_copy = AGENT_DISCONNECTED;
                        }
                    }
                }
                break;

            case AGENT_CONNECTED:
                // 狀態機：Agent 已連線，處理通訊與時間同步
                if (!rmw_uros_epoch_synchronized()) {
                    unsigned long now = millis();
                    if (now - last_sync_try > 1000) {
                        (void)rmw_uros_sync_session(10);
                        last_sync_try = now;
                    }
                }

                // 執行 micro-ROS 事件迴圈
                (void)rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

                // 定期 Ping 檢查健康度 (每 1000ms 檢查一次)
                if (millis() - last_ping_time > 1000) {
                    last_ping_time = millis();
                    if (rmw_uros_ping_agent(50, 1) != RMW_RET_OK) {
                        Serial.println("[ROS] Ping failed! Entering AGENT_LOSING state...");
                        portENTER_CRITICAL(&mux);
                        current_agent_state = AGENT_LOSING;
                        portEXIT_CRITICAL(&mux);
                        local_state_copy = AGENT_LOSING;
                        ping_missed_count = 1;
                        Serial.printf("[ROS] Disconnected for %d seconds...\n", ping_missed_count);
                    }
                }
                break;

            case AGENT_LOSING:
                // 狀態機：懷疑斷線，進入 Debounce 檢查
                // 盡量處理殘餘封包
                (void)rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

                // 降低 Ping 頻率，每 1000ms 檢查一次，累積 5 次 (5秒)
                if (millis() - last_ping_time > 1000) {
                    last_ping_time = millis();
                    if (rmw_uros_ping_agent(50, 1) == RMW_RET_OK) {
                        Serial.println("[ROS] Ping recovered. Returning to AGENT_CONNECTED.");
                        portENTER_CRITICAL(&mux);
                        current_agent_state = AGENT_CONNECTED;
                        portEXIT_CRITICAL(&mux);
                        local_state_copy = AGENT_CONNECTED;
                        ping_missed_count = 0;
                    } else {
                        ping_missed_count++;
                        Serial.printf("[ROS] Disconnected for %d seconds...\n", ping_missed_count);
                        if (ping_missed_count >= 5) {
                            Serial.println("[ROS] Connection definitively lost. Entering AGENT_DISCONNECTED.");
                            portENTER_CRITICAL(&mux);
                            current_agent_state = AGENT_DISCONNECTED;
                            portEXIT_CRITICAL(&mux);
                            local_state_copy = AGENT_DISCONNECTED;
                        }
                    }
                }
                break;

            case AGENT_DISCONNECTED:
                // 狀態機：確認斷線，執行資源清理
                Serial.println("[ROS] Destroying entities to free memory...");
                destroy_entities();
                
                // 零秒煞停 (第一層防護：斷線瞬間清除 ROS 端的速度快取)
                portENTER_CRITICAL(&mux);
                cmd_target_v = 0.0f;
                cmd_target_w = 0.0f;
                portEXIT_CRITICAL(&mux);
                
                Serial.println("[ROS] Cleanup complete. Returning to WAITING_AGENT.");
                portENTER_CRITICAL(&mux);
                current_agent_state = WAITING_AGENT;
                portEXIT_CRITICAL(&mux);
                local_state_copy = WAITING_AGENT;
                
                // 重置計時器，準備進入低頻等待與超時計算
                last_ping_time = millis(); 
                last_wait_log_time = millis();
                waiting_start_time = millis();
                break;
        }

        // 每次迴圈結束前更新狀態指示燈，確保視覺回饋不延遲
        updateLED(local_state_copy);

        vTaskDelay(pdMS_TO_TICKS(10)); // 讓出 CPU，避免觸發 Task Watchdog
    }
}