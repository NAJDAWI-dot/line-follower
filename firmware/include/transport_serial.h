#pragma once
#include "protocol.h"

class TransportSerial {
public:
    void begin(unsigned long baud);
    void sendTelemetry(const protocol::Telemetry& t);

    // Polls Serial for incoming bytes, reassembling newline-terminated
    // command lines. Returns true and fills `out` when a full command line
    // was received and parsed successfully during this call.
    bool pollCommand(protocol::Command* out);

private:
    char lineBuf_[256];
    size_t lineLen_ = 0;
};
