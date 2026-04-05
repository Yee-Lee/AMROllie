#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <nav_msgs/msg/odometry.h>
#include <geometry_msgs/msg/twist.h>
#include <rmw_microros/rmw_microros.h> // 引入時間同步相關功能

// --- Pins Configuration ---
#define MOTOR_LEFT_DIR_PIN  18
#define MOTOR_LEFT_PWM_PIN  19
#define MOTOR_RIGHT_DIR_PIN 21
#define MOTOR_RIGHT_PWM_PIN 22

#define ENCODER_LEFT_A_PIN  32
#define ENCODER_LEFT_B_PIN  33
#define ENCODER_RIGHT_A_PIN 25
#define ENCODER_RIGHT_B_PIN 26

// --- UART Pins ---
#define RXD2 16
#define TXD2 17

// --- PWM Channels ---
#define PWM_CHAN_LEFT  0
#define PWM_CHAN_RIGHT 1

// --- ROS Objects ---
rcl_publisher_t odom_publisher;
rcl_subscription_t cmd_vel_subscriber;
nav_msgs__msg__Odometry odom_msg;
geometry_msgs__msg__Twist twist_msg;
rclc_executor_t executor;
rclc_support_t support;
rcl_allocator_t allocator;
rcl_node_t node;

// --- Shared Variables ---
volatile long left_encoder_ticks = 0;
volatile long right_encoder_ticks = 0;

float target_linear_x = 0.0;
float target_angular_z = 0.0;
float current_x = 0.0;
float current_y = 0.0;
float current_theta = 0.0;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
bool emergency_stop = false;

// --- Function Declarations ---
void IRAM_ATTR leftEncoderISR();
void IRAM_ATTR rightEncoderISR();
void stopMotors();
void taskPID_code(void * pvParameters);
void taskROS_code(void * pvParameters);
void cmd_vel_callback(const void * msgin);

void setup() {
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2); // 連接 RPi 的 UART
    set_microros_serial_transports(Serial2);       // 將 micro-ROS 的通訊層綁定到 Serial2

    // 1. Hardware Initialization
    pinMode(MOTOR_LEFT_DIR_PIN, OUTPUT);
    pinMode(MOTOR_RIGHT_DIR_PIN, OUTPUT);
    
    ledcSetup(PWM_CHAN_LEFT, 5000, 8);
    ledcAttachPin(MOTOR_LEFT_PWM_PIN, PWM_CHAN_LEFT);
    ledcSetup(PWM_CHAN_RIGHT, 5000, 8);
    ledcAttachPin(MOTOR_RIGHT_PWM_PIN, PWM_CHAN_RIGHT);

    pinMode(ENCODER_LEFT_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_LEFT_B_PIN, INPUT_PULLUP);
    pinMode(ENCODER_RIGHT_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_RIGHT_B_PIN, INPUT_PULLUP);

    // 2. Bind ISR (Extremely lightweight, with IRAM_ATTR)
    attachInterrupt(digitalPinToInterrupt(ENCODER_LEFT_A_PIN), leftEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_RIGHT_A_PIN), rightEncoderISR, CHANGE);

    // 3. Create FreeRTOS Tasks
    xTaskCreatePinnedToCore(
        taskPID_code,   // Task function
        "taskPID",      // Task name
        4096,           // Stack size
        NULL,           // Parameters
        2,              // High priority
        NULL,           // Task handle
        0               // Core 0 (PRO_CPU)
    );

    xTaskCreatePinnedToCore(
        taskROS_code,   // Task function
        "taskROS",      // Task name
        8192,           // Stack size
        NULL,           // Parameters
        1,              // Medium priority
        NULL,           // Task handle
        1               // Core 1 (APP_CPU)
    );

    // 4. Delete loopTask as we are using FreeRTOS tasks
    vTaskDelete(NULL);
}

void loop() {
    // Left blank intentionally, the default Arduino loopTask is deleted in setup().
}

// --- Interrupt Service Routines ---
void IRAM_ATTR leftEncoderISR() {
    // Minimal integer operations only
    if (digitalRead(ENCODER_LEFT_A_PIN) == digitalRead(ENCODER_LEFT_B_PIN)) {
        left_encoder_ticks++;
    } else {
        left_encoder_ticks--;
    }
}

void IRAM_ATTR rightEncoderISR() {
    // Minimal integer operations only
    if (digitalRead(ENCODER_RIGHT_A_PIN) == digitalRead(ENCODER_RIGHT_B_PIN)) {
        right_encoder_ticks++;
    } else {
        right_encoder_ticks--;
    }
}

// --- System Safety ---
void stopMotors() {
    digitalWrite(MOTOR_LEFT_DIR_PIN, LOW);
    digitalWrite(MOTOR_RIGHT_DIR_PIN, LOW);
    ledcWrite(PWM_CHAN_LEFT, 0);
    ledcWrite(PWM_CHAN_RIGHT, 0);
}

// --- Tasks ---

// Core 0 (PRO_CPU) - Immediate Control & PID
void taskPID_code(void * pvParameters) {
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 100Hz frequency
    xLastWakeTime = xTaskGetTickCount();

    while(true) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

        // Optional emergency stop trigger check via Serial polling
        if (Serial.available()) {
            char c = Serial.read();
            if (c == 's' || c == 'S') {
                emergency_stop = true;
            }
        }

        if (emergency_stop) {
            stopMotors();
            continue; // Lock motor output until system reset
        }

        // 1. Read shared variables and protect with Spinlock
        // 產生 Fake Variables (模擬位姿變化)
        portENTER_CRITICAL(&mux);
        long left_ticks = left_encoder_ticks;
        long right_ticks = right_encoder_ticks;
        float tgt_v = target_linear_x;
        float tgt_w = target_angular_z;
        current_x += 0.01;      // 模擬 x 不斷前進
        current_y += 0.005;     // 模擬 y 不斷偏移
        current_theta += 0.02;  // 模擬持續旋轉
        portEXIT_CRITICAL(&mux);

        // 2. PID calculation (floating-point)
        // ... (PID logic to calculate left_pwm and right_pwm from tgt_v, tgt_w, left_ticks, right_ticks) ...
        
        // 3. Output PWM to motor drivers
        // ledcWrite(PWM_CHAN_LEFT, left_pwm);
        // ledcWrite(PWM_CHAN_RIGHT, right_pwm);

        // 4. Update Odometry locally
        // ... (Calculate dx, dy, dtheta) ...
        
        // portENTER_CRITICAL(&mux);
        // current_x += dx;
        // current_y += dy;
        // current_theta += dtheta;
        // portEXIT_CRITICAL(&mux);
    }
}

// Subscription Callback
void cmd_vel_callback(const void * msgin) {
    const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;
    
    // Protect shared variables update with Spinlock
    portENTER_CRITICAL(&mux);
    target_linear_x = msg->linear.x;
    target_angular_z = msg->angular.z;
    portEXIT_CRITICAL(&mux);

    // 打印收到的速度指令，方便在 Serial Monitor 確認
    Serial.printf("[taskROS] 收到 cmd_vel -> V: %.2f, W: %.2f\n", target_linear_x, target_angular_z);
}

// Core 1 (APP_CPU) - Communications (micro-ROS)
void taskROS_code(void * pvParameters) {
    allocator = rcl_get_default_allocator();

    // Init support
    rclc_support_init(&support, 0, NULL, &allocator);

    // Create node
    rclc_node_init_default(&node, "esp32_ollie_node", "", &support);

    // Create publisher (/odom)
    rclc_publisher_init_default(
        &odom_publisher,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(nav_msgs, msg, Odometry),
        "odom");

    // Create subscriber (/cmd_vel)
    rclc_subscription_init_default(
        &cmd_vel_subscriber,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "cmd_vel");

    // Create executor
    rclc_executor_init(&executor, &support.context, 1, &allocator);
    rclc_executor_add_subscription(&executor, &cmd_vel_subscriber, &twist_msg, &cmd_vel_callback, ON_NEW_DATA);

    // 初始化 Odometry 的 Frame IDs (由於在 C 語言中處理 micro-ROS 字串需指定指標與長度)
    odom_msg.header.frame_id.data = (char*)"odom";
    odom_msg.header.frame_id.size = 4;
    odom_msg.header.frame_id.capacity = 5;
    odom_msg.child_frame_id.data = (char*)"base_link";
    odom_msg.child_frame_id.size = 9;
    odom_msg.child_frame_id.capacity = 10;

    // 與 micro-ROS Agent 進行時間同步 (Timeout: 1000ms)
    rmw_uros_sync_session(1000);

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 目標頻率 10Hz (100ms 週期)

    while(true) {
        // Spin micro-ROS (設定極短的 timeout 避免阻塞迴圈)
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));

        // Publish Odometry
        // 1. 安全地讀取共享變數並打包成 ROS Message
        portENTER_CRITICAL(&mux);
        odom_msg.pose.pose.position.x = current_x;
        odom_msg.pose.pose.position.y = current_y;
        odom_msg.pose.pose.orientation.z = sin(current_theta / 2.0);
        odom_msg.pose.pose.orientation.w = cos(current_theta / 2.0);
        portEXIT_CRITICAL(&mux);

        // 取得同步後的時間戳記 (nanoseconds)
        int64_t time_ns = rmw_uros_epoch_nanos();
        odom_msg.header.stamp.sec = (int32_t)(time_ns / 1000000000);
        odom_msg.header.stamp.nanosec = (uint32_t)(time_ns % 1000000000);

        rcl_ret_t rc = rcl_publish(&odom_publisher, &odom_msg, NULL);
        
        // 2. 只有在發生錯誤時才在本地端 (PC) 顯示警告，避免洗版
        if (rc != RCL_RET_OK) {
            Serial.println("[taskROS] 警告: 發佈 Odometry 失敗");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // 精準維持 10Hz 的迴圈頻率
    }
}
