/**
 * ============================================================================
 * 檔案: i2cScanner.cpp
 * 描述: 一個簡單的 I2C 設備掃描器。
 *       它會掃描 0x01 到 0x7F 之間的所有 I2C 位址，並在 Serial Monitor 上
 *       印出找到的設備位址。
 * 如何使用: 
 *   1. PlatformIO: 切換到 'scanner' 環境 (platformio run -e scanner -t upload)
 *   2. Arduino IDE: 直接編譯並上傳此檔案。
 * ============================================================================
 */
#include <Arduino.h>
#include <Wire.h>

void setup() {
    Wire.begin();
    Serial.begin(115200);
    Serial.println("\nI2C Scanner");
}

void loop() {
    byte error, address;
    int nDevices;

    Serial.println("Scanning...");

    nDevices = 0;
    for (address = 1; address < 127; address++) {
        // 使用 Wire.beginTransmission 來探測設備
        // 0 表示成功, 1 表示數據太長, 2 表示收到 NACK, 3 表示收到 NACK, 4 表示其他錯誤
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.print(address, HEX);
            Serial.println("  !");

            nDevices++;
        } else if (error == 4) {
            Serial.print("Unknown error at address 0x");
            if (address < 16) {
                Serial.print("0");
            }
            Serial.println(address, HEX);
        }
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    } else {
        Serial.println("done\n");
    }

    delay(5000); // 每 5 秒掃描一次
}