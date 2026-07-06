#include "protocol.h"
#include <ArduinoJson.h>
#include <cstring>

size_t protocol::encodeTelemetryJson(const Telemetry& t, char* out, size_t outSize) {
    JsonDocument doc;
    doc["t"] = t.timestampMs;
    JsonArray s = doc["s"].to<JsonArray>();
    for (int i = 0; i < 8; i++) s.add(t.sensors[i]);
    doc["pos"] = t.linePosition;
    doc["det"] = t.lineDetected;
    doc["err"] = t.pidError;
    doc["out"] = t.pidOutput;
    doc["ls"] = t.leftSpeed;
    doc["rs"] = t.rightSpeed;
    doc["cal"] = static_cast<uint8_t>(t.calState);
    if (t.calState != CalibrationState::Idle) {
        JsonArray mn = doc["cmin"].to<JsonArray>();
        JsonArray mx = doc["cmax"].to<JsonArray>();
        for (int i = 0; i < 8; i++) {
            mn.add(t.calMin[i]);
            mx.add(t.calMax[i]);
        }
    }
    doc["pid"] = t.pidEnabled;
    return serializeJson(doc, out, outSize);
}

void protocol::encodeTelemetryBinary(const Telemetry& t, uint16_t timestampDeltaMs, TelemetryBinary* out) {
    out->timestampDeltaMs = timestampDeltaMs;
    for (int i = 0; i < 8; i++) {
        out->sensors[i] = static_cast<uint8_t>(t.sensors[i] * 255 / 1000);
    }
    out->linePosition = t.linePosition;
    out->pidError = t.pidError;
    out->leftSpeed = static_cast<int8_t>(t.leftSpeed * 127 / 255);
    out->rightSpeed = static_cast<int8_t>(t.rightSpeed * 127 / 255);
    uint8_t flags = 0;
    if (t.lineDetected) flags |= 0x01;
    if (t.pidEnabled) flags |= 0x02;
    flags |= (static_cast<uint8_t>(t.calState) & 0x03) << 2;
    out->flags = flags;
}

bool protocol::parseCommand(const char* jsonLine, size_t len, Command* out) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, jsonLine, len);
    if (err) {
        out->type = CommandType::Unknown;
        return false;
    }
    const char* cmd = doc["cmd"] | "";
    if (strcmp(cmd, "setPid") == 0) {
        out->type = CommandType::SetPid;
        out->kp = doc["kp"] | 0.0f;
        out->ki = doc["ki"] | 0.0f;
        out->kd = doc["kd"] | 0.0f;
        out->baseSpeed = doc["base"] | 0;
        return true;
    }
    if (strcmp(cmd, "calibrate") == 0) {
        out->type = CommandType::Calibrate;
        out->calibrateStart = doc["start"] | true;
        return true;
    }
    if (strcmp(cmd, "start") == 0) {
        out->type = CommandType::Start;
        return true;
    }
    if (strcmp(cmd, "stop") == 0) {
        out->type = CommandType::Stop;
        return true;
    }
    out->type = CommandType::Unknown;
    return false;
}
