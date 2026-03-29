#ifndef STAT_WEB_UI_H
#define STAT_WEB_UI_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "DataLogger.h"
#include <freertos/FreeRTOS.h>

static const char stats_html[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="zh-TW">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OdoTracker 數據監控</title>
    <style>
        body { background-color: #121212; color: #e0e0e0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 15px; height: 100vh; box-sizing: border-box; display: flex; flex-direction: column; }
        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; flex-shrink: 0; }
        h2 { margin: 0; font-size: 16px; font-weight: 500; color: #bbb; letter-spacing: 1px; }
        .table-container { overflow-y: auto; border: 1px solid #2a2a2a; border-radius: 4px; flex-grow: 1; background: #181818; }
        table { width: 100%; border-collapse: collapse; }
        th, td { border-bottom: 1px solid #222; padding: 4px 8px; text-align: center; font-size: 12px; font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace; }
        th { background-color: #202020; color: #888; position: sticky; top: 0; z-index: 1; box-shadow: 0 1px 0 #333; font-weight: normal; padding: 6px 8px; }
        tr:nth-child(even) { background-color: #1c1c1c; }
        tr:hover { background-color: #252525; }
        button { padding: 5px 12px; cursor: pointer; border: 1px solid #444; background: #2a2a2a; color: #ccc; border-radius: 4px; font-size: 12px; transition: all 0.2s ease; }
        button:hover { background: #3a3a3a; border-color: #555; }
        .btn-toggle { background-color: #4a2525; border-color: #663333; color: #e0e0e0; }
        .btn-toggle.paused { background-color: #254a25; border-color: #336633; }
    </style>
</head>
<body>
    <div class="header">
        <h2>車體運行數據 (OdoTracker)</h2>
        <div>
            <button id="toggle-btn" class="btn-toggle paused" onclick="toggleAutoRefresh()">恢復讀取</button>
            <button onclick="fetchData()">手動刷新</button>
        </div>
    </div>
    <div class="table-container">
        <table>
            <thead>
                <tr>
                    <th>時間 (ms)</th>
                    <th>Joy X</th>
                    <th>Joy Y</th>
                    <th>V (線速度)</th>
                    <th>W (角速度)</th>
                    <th>X (座標)</th>
                    <th>Y (座標)</th>
                    <th>θ (角度)</th>
                    <th>L-RPM</th>
                    <th>R-RPM</th>
                    <th>Gyro W</th>
                    <th>W-Corr</th>
                </tr>
            </thead>
            <tbody id="data-body">
                <tr><td colspan="12">載入中...</td></tr>
            </tbody>
        </table>
    </div>

    <script>
        function fetchData() {
            fetch('/api/stats')
                .then(response => response.json())
                .then(data => {
                    const tbody = document.getElementById('data-body');
                    tbody.innerHTML = '';
                    // 反轉陣列，讓最新產生的數據顯示在表格最上方
                    data.reverse().forEach(row => {
                        const tr = document.createElement('tr');
                        tr.innerHTML = `
                            <td>${row.time}</td>
                            <td>${row.joy_x.toFixed(2)}</td>
                            <td>${row.joy_y.toFixed(2)}</td>
                            <td>${row.v.toFixed(2)}</td>
                            <td>${row.w.toFixed(2)}</td>
                            <td>${row.x.toFixed(2)}</td>
                            <td>${row.y.toFixed(2)}</td>
                            <td>${row.th.toFixed(2)}</td>
                            <td>${row.rpm_l.toFixed(1)}</td>
                            <td>${row.rpm_r.toFixed(1)}</td>
                            <td>${row.gyro_w.toFixed(2)}</td>
                            <td>${row.w_corr.toFixed(2)}</td>
                        `;
                        tbody.appendChild(tr);
                    });
                })
                .catch(err => console.error('Error fetching data:', err));
        }
        
        let autoUpdate = false;
        let updateTimer = null;

        function toggleAutoRefresh() {
            autoUpdate = !autoUpdate;
            const btn = document.getElementById('toggle-btn');
            if (autoUpdate) {
                btn.innerText = "暫停讀取";
                btn.classList.remove('paused');
                fetchData();
                updateTimer = setInterval(fetchData, 1000);
            } else {
                btn.innerText = "恢復讀取";
                btn.classList.add('paused');
                clearInterval(updateTimer);
            }
        }

        fetchData();
    </script>
</body>
</html>
)raw";

// --- 自定義的 JSON Chunked 傳輸回應 ---
class StatsJSONResponse : public AsyncAbstractResponse {
private:
    RobotState* _records;
    int _total;
    int _current_idx;
    bool _started;
public:
    StatsJSONResponse(RobotState* records, int total) : 
        _records(records), _total(total), _current_idx(0), _started(false) {
        _code = 200;
        _contentType = "application/json";
        _sendContentLength = false; 
        _chunked = true;            
    }
    ~StatsJSONResponse() {
        if (_records) free(_records);
    }
    bool _sourceValid() const { return true; }
    virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
        size_t len = 0;
        if (!_started && maxLen > 0) {
            buf[len++] = '[';
            _started = true;
        }
        while (_current_idx < _total) {
            char temp[256];
            const RobotState& record = _records[_current_idx];
            int written = snprintf(temp, sizeof(temp),
                "%s{\"time\":%lu,\"joy_x\":%.2f,\"joy_y\":%.2f,\"v\":%.2f,\"w\":%.2f,\"x\":%.2f,\"y\":%.2f,\"th\":%.2f,\"rpm_l\":%.2f,\"rpm_r\":%.2f,\"gyro_w\":%.2f,\"w_corr\":%.2f}",
                (_current_idx > 0) ? "," : "", 
                record.timestamp, record.joy_x, record.joy_y,
                record.v, record.w, record.x, record.y, record.th,
                record.rpm_l, record.rpm_r, record.gyro_w, record.w_corr
            );
            if (written > 0 && (size_t)written <= (maxLen - len)) {
                memcpy(buf + len, temp, written);
                len += written;
                _current_idx++;
            } else break; 
        }
        if (_current_idx == _total && (maxLen - len) > 0) {
            buf[len++] = ']';
            _current_idx++; 
        }
        return len; 
    }
};

// --- 註冊路由端點函式 ---
inline void setupStatWebUI(AsyncWebServer* server, DataLogger* logger, portMUX_TYPE* mux) {
    server->on("/stats", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", stats_html);
    });

    server->on("/api/stats", HTTP_GET, [logger, mux](AsyncWebServerRequest *request) {
        portENTER_CRITICAL(mux);
        int alloc_count = logger->getCount();
        portEXIT_CRITICAL(mux);

        if (alloc_count == 0) {
            request->send(200, "application/json", "[]");
            return;
        }

        RobotState* records_copy = (RobotState*)malloc(alloc_count * sizeof(RobotState));
        if (!records_copy) {
            request->send(500, "text/plain", "Memory Error");
            return;
        }

        int actual_count = 0;
        portENTER_CRITICAL(mux);
        actual_count = logger->getCount();
        if (actual_count > alloc_count) actual_count = alloc_count; 
        for (int i = 0; i < actual_count; i++) records_copy[i] = logger->getRecord(i);
        portEXIT_CRITICAL(mux);

        StatsJSONResponse *response = new StatsJSONResponse(records_copy, actual_count);
        request->send(response);
    });
}

#endif