#pragma once
#include "protocol.h"
#include <functional>

class TransportWifi {
public:
    void begin(const char* apSsid, const char* apPassword);
    void sendTelemetry(const protocol::Telemetry& t);

    // Registers a callback invoked once per fully-received JSON command
    // text frame from any connected WebSocket client.
    void onCommand(std::function<void(const protocol::Command&)> cb);
};
