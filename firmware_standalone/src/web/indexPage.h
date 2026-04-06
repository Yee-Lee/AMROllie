#ifndef INDEX_PAGE_H
#define INDEX_PAGE_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

static const char welcome_html[] PROGMEM = R"raw(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ARMOllie standalone portal</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: #121212; color: #e0e0e0; padding: 2rem; max-width: 640px; margin: 0 auto; }
        h1 { border-bottom: 1px solid #333; padding-bottom: 12px; font-size: 1.8rem; font-weight: 600; margin-top: 0; }
        p.subtitle { color: #aaa; margin-bottom: 24px; font-size: 0.95rem; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 16px; }
        .card { display: block; background: #1a1a1a; border: 1px solid #2a2a2a; border-radius: 8px; padding: 20px; text-decoration: none; color: inherit; transition: all 0.2s ease; }
        .card:hover { background: #222; border-color: #00ff88; transform: translateY(-2px); box-shadow: 0 4px 12px rgba(0,255,136,0.05); }
        .card-title { color: #00ff88; font-size: 1.1rem; font-weight: 500; margin-bottom: 8px; }
        .tip { color: #777; font-size: 0.85rem; line-height: 1.5; }
    </style>
</head>
<body>
    <h1>ARMOllie standalone portal</h1>
    <p class="subtitle">Please select a module to access:</p>
    <div class="grid">
        <a href="/joystick" class="card">
            <div class="card-title">Joystick Control Dashboard</div>
            <div class="tip">Dual-stick remote control with real-time telemetry overlay.</div>
        </a>
        <a href="/stats" class="card">
            <div class="card-title">OdoTracker Data Monitor</div>
            <div class="tip">Real-time motion logs and odometry tracking table.</div>
        </a>
    </div>
</body>
</html>
)raw";

class IndexPage {
public:
    IndexPage() {}

    inline void attachToServer(AsyncWebServer* server) {
        server->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/html", welcome_html);
        });
    }
};

#endif // INDEX_PAGE_H