#pragma once
#include <cstdint>
#include <cstddef>

namespace protocol {

enum class CalibrationState : uint8_t { Idle = 0, Calibrating = 1, Done = 2 };

struct Telemetry {
    uint32_t timestampMs;
    uint16_t sensors[8];       // calibrated 0-1000
    int16_t linePosition;      // 0-7000, -1 if line not detected
    bool lineDetected;
    int16_t pidError;
    float pidOutput;
    int16_t leftSpeed;         // -255..255
    int16_t rightSpeed;        // -255..255
    CalibrationState calState;
    uint16_t calMin[8];
    uint16_t calMax[8];
    bool pidEnabled;
};

// Serializes telemetry as a single JSON line (no trailing newline) for
// serial/WiFi transports. Returns the number of bytes written into `out`
// (excluding null terminator), or 0 if `out` is too small or encoding fails.
size_t encodeTelemetryJson(const Telemetry& t, char* out, size_t outSize);

// Packed binary struct for BLE notify. Exactly 17 bytes, safe for the
// default BLE ATT MTU. Does not carry calibration min/max (those are only
// needed during the rare calibration flow, sent over JSON transports).
#pragma pack(push, 1)
struct TelemetryBinary {
    uint16_t timestampDeltaMs; // ms since previous sample (wraps at 65535)
    uint8_t sensors[8];        // calibrated 0-1000 scaled down to 0-255
    int16_t linePosition;      // 0-7000, -1 if not detected
    int16_t pidError;
    int8_t leftSpeed;          // -127..127 (scaled from -255..255)
    int8_t rightSpeed;
    uint8_t flags;             // bit0=lineDetected, bit1=pidEnabled, bits2-3=calState
};
#pragma pack(pop)
static_assert(sizeof(TelemetryBinary) == 17, "TelemetryBinary must stay 17 bytes to fit default BLE MTU");

void encodeTelemetryBinary(const Telemetry& t, uint16_t timestampDeltaMs, TelemetryBinary* out);

enum class CommandType { Unknown, SetPid, Calibrate, Start, Stop };

struct Command {
    CommandType type = CommandType::Unknown;
    float kp = 0, ki = 0, kd = 0;
    int baseSpeed = 0;
    bool calibrateStart = true; // true = start calibration, false = stop
};

// Parses one newline-terminated (or bare) JSON command line. Returns true
// and fills `out` on success; returns false (out->type == Unknown) on
// malformed JSON or an unrecognized "cmd" field.
bool parseCommand(const char* jsonLine, size_t len, Command* out);

} // namespace protocol
