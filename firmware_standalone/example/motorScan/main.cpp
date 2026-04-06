#include <Arduino.h>

// 共用主程式模組
#include "../../src/config.h" 
#include "../../src/Motor.h"
#include "../../src/WebServerManager.h"

// 本地掃描模組
#include "MotorScanner.h"
#include "web/indexPage.h"
#include "web/motorScanPage.h"

WebServerManager webServer;
IndexPage indexPage;
MotorScanPage scanPage;

// 實例化真實馬達 (沿用主系統設定)
Motor leftMotor(MOTOR_L_IN1, MOTOR_L_IN2, MOTOR_L_PWM, MOTOR_L_ENC_A, MOTOR_L_ENC_B, MOTOR_CPR, false, 10);
Motor rightMotor(MOTOR_R_IN1, MOTOR_R_IN2, MOTOR_R_PWM, MOTOR_R_ENC_A, MOTOR_R_ENC_B, MOTOR_CPR, true, 10);

MotorScanner scanner;
IMotor* currentMotor = &leftMotor;

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- Starting MotorScan Environment ---");

    // 初始化馬達硬體
    leftMotor.init();
    rightMotor.init();

    // 初始化並啟動網頁伺服器
    webServer.begin(WIFI_SSID, WIFI_PASSWORD);
    indexPage.attachToServer(webServer.getServer());
    scanPage.attachToServer(webServer.getServer());
    webServer.start();

    Serial.println("Initialization complete. Waiting for WebUI commands...");
}

void loop() {
    scanPage.cleanup();
    leftMotor.update();
    rightMotor.update();

    // 1. 處理使用者從 WebUI 傳來的馬達切換
    if (scanPage.hasMotorChanged()) {
        scanner.reset();
        currentMotor = (scanPage.selectedMotorIdx == 0) ? &leftMotor : &rightMotor;
        Serial.printf("Switched to %s motor.\n", (scanPage.selectedMotorIdx == 0) ? "LEFT" : "RIGHT");
    }

    // 2. 處理掃描狀態機切換
    static int lastState = 0;
    if (scanPage.state != lastState) {
        if (scanPage.state == 1 && !scanPage.manualMode) scanner.begin(currentMotor);
        else { scanner.reset(); currentMotor->stop(); }
        lastState = scanPage.state;
    }

    // 3. 執行自動掃描邏輯
    if (scanPage.state == 1 && !scanPage.manualMode) {
        scanner.run();
        if (scanner.getScannerState() == MotorScanner::DONE) scanPage.state = 0; // 完成後回到 IDLE
    }

    // 4. 將當前 RPM 與 PWM 定期推播回網頁
    static unsigned long lastPush = 0;
    if (scanPage.state == 1 && millis() - lastPush > 50) {
        lastPush = millis();
        scanPage.broadcastData(millis(), currentMotor->getCurrRPM(), scanner.getCurrentPWM(), (scanner.getScannerState() == MotorScanner::SCAN) ? "SCAN" : "DONE");
    }
}