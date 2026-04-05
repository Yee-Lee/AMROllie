#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <Arduino.h>

struct RobotState {
    unsigned long timestamp;
    float joy_x;
    float joy_y;
    float v;
    float w;
    float x;
    float y;
    float th;
    float rpm_l;
    float rpm_r;
    float gyro_w;
    float w_corr;
};

class DataLogger {
private:
    // 儲存最近 512 筆資料 (約 51.2 秒的數據，佔用約 16KB RAM)
    static const int MAX_RECORDS = 512;
    RobotState records[MAX_RECORDS];
    int head = 0;
    int count = 0;

public:
    DataLogger() {}

    void logData(float joy_x, float joy_y, float v, float w, float x, float y, float th, float rpm_l, float rpm_r, float gyro_w, float w_corr) {
        records[head] = {millis(), joy_x, joy_y, v, w, x, y, th, rpm_l, rpm_r, gyro_w, w_corr};
        head = (head + 1) % MAX_RECORDS;
        if (count < MAX_RECORDS) {
            count++;
        }
    }

    int getCount() const {
        return count;
    }

    // 取得環形緩衝區中的第 i 筆資料 (i 從 0 開始，0 為最舊的資料)
    const RobotState& getRecord(int i) const {
        int start = (count < MAX_RECORDS) ? 0 : head;
        int index = (start + i) % MAX_RECORDS;
        return records[index];
    }
};

#endif