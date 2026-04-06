#ifndef WEB_SERVER_MANAGER_H
#define WEB_SERVER_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include "config.h"

class WebServerManager {
public:
    WebServerManager() : _server(WEB_SERVER_PORT) {}

    void begin(const char* ssid, const char* password) {
        Serial.print("Connecting to WiFi");
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) { 
            delay(500); 
            Serial.print(".");
        }
        Serial.println();
        Serial.printf("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());

        esp_wifi_set_ps(WIFI_PS_NONE); 
        WiFi.setSleep(false); 
        
        // 啟動 mDNS
        if (MDNS.begin(MDNS_HOSTNAME)) {
            Serial.printf("mDNS responder started. Access via http://%s.local\n", MDNS_HOSTNAME);
        } else {
            Serial.println("Error setting up MDNS responder!");
        }
    }

    void start() {
        _server.begin();
        Serial.println("Web Server started.");
    }

    AsyncWebServer* getServer() {
        return &_server;
    }

private:
    AsyncWebServer _server;
};

#endif // WEB_SERVER_MANAGER_H