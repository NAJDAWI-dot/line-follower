#pragma once
#include "protocol.h"
#include <functional>

class TransportBle {
public:
    void begin(const char* deviceName);
    void sendTelemetry(const protocol::Telemetry& t, uint16_t timestampDeltaMs);
    void onCommand(std::function<void(const protocol::Command&)> cb);
};
