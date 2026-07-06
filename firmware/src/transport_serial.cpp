#include "transport_serial.h"
#include <Arduino.h>

void TransportSerial::begin(unsigned long baud) {
    Serial.begin(baud);
    lineLen_ = 0;
}

void TransportSerial::sendTelemetry(const protocol::Telemetry& t) {
    char buf[512];
    size_t n = protocol::encodeTelemetryJson(t, buf, sizeof(buf));
    if (n > 0) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
        Serial.write('\n');
    }
}

bool TransportSerial::pollCommand(protocol::Command* out) {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n') {
            bool ok = protocol::parseCommand(lineBuf_, lineLen_, out);
            lineLen_ = 0;
            if (ok) return true;
            continue;
        }
        if (lineLen_ < sizeof(lineBuf_) - 1) {
            lineBuf_[lineLen_++] = c;
        } else {
            lineLen_ = 0; // overflow guard: drop the oversized line
        }
    }
    return false;
}
