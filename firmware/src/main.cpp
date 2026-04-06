#include <Arduino.h>
#include "task.h"

// --- 3. 共享變數與執行緒安全 ---
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// 3.1 下行指令 (taskROS -> taskBaseController)
volatile float cmd_target_v = 0.0;
volatile float cmd_target_w = 0.0;
volatile bool flag_reset_odom = false;

// 3.1 上行遙測 (taskBaseController -> taskROS)
volatile float odom_x = 0.0, odom_y = 0.0, odom_theta = 0.0;
volatile float actual_v = 0.0, actual_w = 0.0;
volatile float sonar_left_dist = 0.0, sonar_right_dist = 0.0;

// --- 主線程 ---
void setup() {
    Serial.begin(115200);
    
    // 初始化所有底層硬體 (IMU、馬達、PID、超音波等)
    initBaseControllerHardware();

    // 啟動 Base Controller (Core 0, 高優先級)
    xTaskCreatePinnedToCore(taskBaseController, "BaseCtrl", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 0);

    // 啟動 ROS 通訊 (Core 1, 中優先級)
    // micro-ROS 需要較大的 Stack 空間
    xTaskCreatePinnedToCore(taskROS, "ROS", 16384, NULL, configMAX_PRIORITIES - 2, NULL, 1);

    // 終止自身，釋放資源
    vTaskDelete(NULL); 
}

void loop() {
    // setup 已將 loopTask 刪除，此處不會執行
}