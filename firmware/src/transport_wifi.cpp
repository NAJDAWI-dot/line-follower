#include "transport_wifi.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

namespace {
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
std::function<void(const protocol::Command&)> commandCallback;

void handleWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                    AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo* info = static_cast<AwsFrameInfo*>(arg);
        if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
            protocol::Command cmd;
            if (protocol::parseCommand(reinterpret_cast<const char*>(data), len, &cmd) && commandCallback) {
                commandCallback(cmd);
            }
        }
    }
}
} // namespace

void TransportWifi::begin(const char* apSsid, const char* apPassword) {
    WiFi.softAP(apSsid, apPassword);
    ws.onEvent(handleWsEvent);
    server.addHandler(&ws);
    server.begin();
}

void TransportWifi::sendTelemetry(const protocol::Telemetry& t) {
    char buf[512];
    size_t n = protocol::encodeTelemetryJson(t, buf, sizeof(buf));
    if (n > 0) {
        ws.textAll(buf, n);
    }
}

void TransportWifi::onCommand(std::function<void(const protocol::Command&)> cb) {
    commandCallback = cb;
}
