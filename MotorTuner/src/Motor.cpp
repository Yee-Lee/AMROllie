
/**
 * ============================================================================
 * 檔案: Motor.cpp
 * ============================================================================
 */
#include "Motor.h"


Motor::Motor(int pa, int pb, int pd, int pwm, int _cpr, int interval) 
    : pinA(pa), pinB(pb), pinDir(pd), pinPWM(pwm), cpr(_cpr), updateInterval(interval), 
      pos(0), currRPM(0), lastUpdate(0) {}

void IRAM_ATTR Motor::isrWrapper(void* arg) {
    Motor* instance = (Motor*)arg;
    if (digitalRead(instance->pinB)) {
        instance->pos++;
    } else {
        instance->pos--;
    }
}

void Motor::init() {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinDir, OUTPUT);
    pinMode(pinPWM, OUTPUT);
    attachInterruptArg(digitalPinToInterrupt(pinA), isrWrapper, this, RISING);
    lastUpdate = millis();
}

bool Motor::update() {
    unsigned long now = millis();
    if (now - lastUpdate >= updateInterval) {
        portENTER_CRITICAL_ISR(&mux);
        long count = pos;
        pos = 0; 
        portEXIT_CRITICAL_ISR(&mux);

        float dt = (float)(now - lastUpdate) / 1000.0f;
        float rawRPM = ((float)count / (float)cpr) / dt * 60.0f;

        if (abs(rawRPM) < 1000.0f) {
            currRPM = currRPM * 0.7f + rawRPM * 0.3f; // 低通濾波
        }

        lastUpdate = now;
        return true;
    }
    return false;
}

float Motor::getCurrRPM() { return currRPM; }

void Motor::drive(int pwm) {
    digitalWrite(pinDir, pwm >= 0 ? HIGH : LOW);
    analogWrite(pinPWM, abs(pwm));
}

void Motor::stop() {
    analogWrite(pinPWM, 0);
    digitalWrite(pinDir, LOW);
}