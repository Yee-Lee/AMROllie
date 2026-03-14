
/**
 * ============================================================================
 * 檔案: MotorSim.cpp
 * ============================================================================
 */
#include "MotorSim.h"

MotorSim::MotorSim(float maxR, int interval) 
    : maxRPM(maxR), updateInterval(interval), currRPM(0), currentPWM(0), lastUpdate(0) {}

void MotorSim::init() { lastUpdate = millis(); }

bool MotorSim::update() {
        unsigned long now = millis();
        if (now - lastUpdate >= updateInterval) {
            // 將模擬邏輯直接併入 update
            const int minPWM = 120;
            const int satPWM = 240;
            const float maxRPM = 300.0; 
            int absPWM = abs(currentPWM);

            if (absPWM < minPWM) {
                currRPM = 0.0; // 死區
            } 
            else if (absPWM > satPWM) {
                currRPM = maxRPM; // 飽和區
            } 
            else {
                // 線性映射公式
                currRPM = (float)(absPWM - minPWM) * maxRPM / (satPWM - minPWM);
            }
            
            lastUpdate = now;
            return true;
        }
        return false;
    }

float MotorSim::getCurrRPM() { return currRPM; }
void MotorSim::drive(int pwm) { currentPWM = pwm; }
void MotorSim::stop() { currentPWM = 0; currRPM = 0;}
