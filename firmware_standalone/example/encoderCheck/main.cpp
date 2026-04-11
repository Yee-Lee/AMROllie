#include <Arduino.h>
#include "../../src/config.h"

volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;

// 採用 2x Decoding 雙沿觸發邏輯，與主程式保持一致
void IRAM_ATTR leftEncoderISR() {
    bool stateA = digitalRead(MOTOR_L_ENC_A);
    bool stateB = digitalRead(MOTOR_L_ENC_B);
    if (stateA == stateB) {
        leftEncoderCount++;
    } else {
        leftEncoderCount--;
    }
}

void IRAM_ATTR rightEncoderISR() {
    bool stateA = digitalRead(MOTOR_R_ENC_A);
    bool stateB = digitalRead(MOTOR_R_ENC_B);
    if (stateA == stateB) {
        rightEncoderCount++;
    } else {
        rightEncoderCount--;
    }
}

void setup() {
    Serial.begin(115200);
    
    // 等待 Serial 初始化
    delay(1000); 
    
    Serial.println("\n--- Encoder Pulse Check ---");
    Serial.println("請手動將左、右輪分別旋轉『剛好一圈』");
    Serial.println("觀察下方印出的數值，即可得知實際的 Encoder CPR");
    Serial.println("在序列埠監控視窗輸入 'r' 並發送，即可將計數歸零");
    Serial.println("---------------------------\n");

    // 初始化 Encoder 腳位 (使用 INPUT_PULLUP 避免浮接導致誤觸發)
    pinMode(MOTOR_L_ENC_A, INPUT_PULLUP);
    pinMode(MOTOR_L_ENC_B, INPUT_PULLUP);
    pinMode(MOTOR_R_ENC_A, INPUT_PULLUP);
    pinMode(MOTOR_R_ENC_B, INPUT_PULLUP);

    // 綁定中斷 (改為 CHANGE 雙沿觸發)
    attachInterrupt(digitalPinToInterrupt(MOTOR_L_ENC_A), leftEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(MOTOR_R_ENC_A), rightEncoderISR, CHANGE);
}

void loop() {
    static unsigned long lastPrintTime = 0;
    
    // 每 500ms 印出一次當前數值，並加上「原始腳位狀態」方便硬體除錯
    if (millis() - lastPrintTime > 500) {
        Serial.printf("Count [ L: %ld | R: %ld ]\t Raw Pins [ LA:%d LB:%d | RA:%d RB:%d ]\n", 
            leftEncoderCount, rightEncoderCount,
            digitalRead(MOTOR_L_ENC_A), digitalRead(MOTOR_L_ENC_B),
            digitalRead(MOTOR_R_ENC_A), digitalRead(MOTOR_R_ENC_B));
        lastPrintTime = millis();
    }

    // 接收序列埠指令以隨時歸零數值，方便你分次測量
    if (Serial.available() > 0) {
        char c = Serial.read();
        if (c == 'r' || c == 'R') {
            leftEncoderCount = 0;
            rightEncoderCount = 0;
            Serial.println("\n>>> 數值已歸零 <<<\n");
        }
    }
}