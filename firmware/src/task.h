#ifndef TASK_H
#define TASK_H

#include <Arduino.h>

// --- 系統狀態與 LED 定義 ---
#define LED_PIN 2
#define NUM_LEDS 1

enum AgentState {
    WAITING_AGENT,      // 等待連線 (低頻 Ping)
    AGENT_CONNECTED,    // 正常連線中
    AGENT_LOSING,       // 懷疑斷線 (高頻 Ping Debounce)
    AGENT_DISCONNECTED  // 確認斷線，執行清理
};

enum SensorStatus {
    SENSOR_SAFE,        // 安全：無障礙物 (綠燈)
    SENSOR_WARNING,     // 警告：進入減速區 (橘燈)
    SENSOR_BRAKE        // 危險：進入煞停區 (紅燈)
};

// --- 3. 共享變數與執行緒安全 ---
extern portMUX_TYPE mux;

// 3.1 下行指令 (taskROS -> taskBaseController)
extern volatile float cmd_target_v;
extern volatile float cmd_target_w;
extern volatile bool flag_reset_odom;
extern volatile unsigned long last_cmd_vel_time; // 速度指令最後接收時間

// 3.1 上行遙測 (taskBaseController -> taskROS)
extern volatile float odom_x, odom_y, odom_theta;
extern volatile float actual_v, actual_w;
extern volatile float sonar_left_dist, sonar_right_dist;
extern volatile bool status_emergency_brake;
extern volatile SensorStatus current_sensor_status; // 感測器安全等級

// 3.2 系統連線狀態
extern volatile AgentState current_agent_state;

// --- 任務宣告 ---
void initBaseControllerHardware();
void taskBaseController(void *pvParameters);
void taskROS(void *pvParameters);

#endif // TASK_H