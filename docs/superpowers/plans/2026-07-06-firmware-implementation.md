# ESP32 Line Follower Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the ESP32 firmware that reads a QTR-8A reflectance sensor array, computes line position, runs a PID line-following loop driving a TB6612FNG motor driver, and streams telemetry / accepts tuning commands over USB serial, WiFi (as its own access point), and BLE simultaneously.

**Architecture:** A PlatformIO project split into hardware-independent "pure" modules (`line_position`, `pid_controller`, `protocol`) that get real unit tests on a native (desktop) build target, and hardware-dependent modules (`sensors`, `motors`, three `transport_*` adapters, `main`) that get manually verified on real hardware since embedded I/O can't be meaningfully unit tested. All three transports carry the same logical messages defined once in `protocol.h/.cpp`.

**Tech Stack:** PlatformIO, Arduino framework for ESP32, ArduinoJson v7, ESPAsyncWebServer + AsyncTCP, NimBLE-Arduino, Unity (PlatformIO's native test framework).

## Global Constraints

- QTR-8A sensor inputs must be wired to ADC1 pins (GPIO32–39 on classic ESP32) — ADC2 conflicts with the WiFi radio. Default mapping: `{32,33,34,35,36,37,38,39}`.
- Default telemetry rate: 50 Hz, configurable via `TELEMETRY_HZ` in `config.h`.
- BLE binary telemetry struct is fixed at exactly 17 bytes (fits the default BLE ATT MTU without relying on MTU negotiation).
- WiFi transport: ESP32 runs as its own access point (not joining an existing network), default IP `192.168.4.1` (ESP32 `WiFi.softAP()` default).
- Motor speed range is -255..255 (8-bit PWM resolution, `PWM_RESOLUTION_BITS = 8`).
- Line position is on a 0–7000 scale, center (on-line) = 3500, classic Pololu-style weighted average.
- All pin assignments live only in `config.h` — no pin numbers hardcoded elsewhere.
- All three transports (serial, WiFi, BLE) must be able to run concurrently.

---

### Task 1: Project scaffold + `line_position` module

**Files:**
- Create: `D:\line\firmware\platformio.ini`
- Create: `D:\line\firmware\include\config.h`
- Create: `D:\line\firmware\include\line_position.h`
- Create: `D:\line\firmware\src\line_position.cpp`
- Test: `D:\line\firmware\test\test_line_position\test_line_position.cpp`

**Interfaces:**
- Produces: `linepos::applyCalibration(uint16_t rawValue, uint16_t minVal, uint16_t maxVal) -> uint16_t` (0–1000, clamped)
- Produces: `linepos::computeLinePosition(const uint16_t calibratedValues[8], uint16_t onLineThreshold, bool* lineDetected) -> int` (0–7000, or -1 if not detected)

- [ ] **Step 1: Create the project structure and `platformio.ini`**

Create directories `D:\line\firmware\include`, `D:\line\firmware\src`, `D:\line\firmware\test\test_line_position`.

Write `D:\line\firmware\platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
lib_deps =
    bblanchon/ArduinoJson@^7.2.0
    ESP32Async/ESPAsyncWebServer@^3.6.0
    ESP32Async/AsyncTCP@^3.3.2
    h2zero/NimBLE-Arduino@^1.4.2
build_flags =
    -DCORE_DEBUG_LEVEL=1

[env:native]
platform = native
test_framework = unity
build_src_filter =
    +<*>
    -<main.cpp>
    -<sensors.cpp>
    -<motors.cpp>
    -<transport_serial.cpp>
    -<transport_wifi.cpp>
    -<transport_ble.cpp>
build_flags =
    -std=gnu++17
lib_deps =
    bblanchon/ArduinoJson@^7.2.0
```

- [ ] **Step 2: Write `config.h`**

```cpp
#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// BOARD PIN CONFIGURATION -- edit this block when switching ESP32 boards.
// ---------------------------------------------------------------------------

// QTR-8A sensor analog inputs. On classic ESP32 (WROOM-32/DevKit V1) these
// are ALL 8 of ADC1's channels (GPIO32-39). IMPORTANT: sensor pins must stay
// on ADC1 -- ADC2 pins (0,2,4,12-15,25-27) stop working correctly once WiFi
// is active, since the radio and ADC2 share hardware.
constexpr uint8_t SENSOR_PINS[8] = {32, 33, 34, 35, 36, 37, 38, 39};

// QTR-8A emitter (IR LED) control pin, a.k.a. LEDON. Set to 255 if your
// module's emitters are wired always-on (no control pin used).
constexpr uint8_t SENSOR_EMITTER_PIN = 27;

// TB6612FNG motor driver pins. Chosen to avoid ESP32 strapping pins
// (0, 2, 5, 12, 15) so motor output states at boot can't affect flash mode.
constexpr uint8_t MOTOR_A_IN1 = 16;
constexpr uint8_t MOTOR_A_IN2 = 17;
constexpr uint8_t MOTOR_A_PWM = 18;
constexpr uint8_t MOTOR_B_IN1 = 19;
constexpr uint8_t MOTOR_B_IN2 = 21;
constexpr uint8_t MOTOR_B_PWM = 22;
constexpr uint8_t MOTOR_STBY  = 23;

// ---------------------------------------------------------------------------
// TUNABLES
// ---------------------------------------------------------------------------

constexpr uint32_t TELEMETRY_HZ = 50;
constexpr uint32_t LINE_LOST_TIMEOUT_MS = 500;

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RESOLUTION_BITS = 8; // duty range 0-255

constexpr float DEFAULT_KP = 0.06f;
constexpr float DEFAULT_KI = 0.0002f;
constexpr float DEFAULT_KD = 0.6f;
constexpr int DEFAULT_BASE_SPEED = 150; // 0-255

constexpr const char* WIFI_AP_SSID = "LineFollower";
constexpr const char* WIFI_AP_PASSWORD = "linefollow"; // WPA2 needs >= 8 chars
```

- [ ] **Step 3: Write `line_position.h`**

```cpp
#pragma once
#include <cstdint>

namespace linepos {

// Converts a raw ADC reading (0-4095 on ESP32's 12-bit ADC) to a calibrated
// 0-1000 value using the given per-channel min/max from calibration.
// Clamps output to [0, 1000]. If maxVal <= minVal (uncalibrated), returns 0.
uint16_t applyCalibration(uint16_t rawValue, uint16_t minVal, uint16_t maxVal);

// Computes line position on a 0-7000 scale (center = 3500) from 8
// calibrated sensor readings (each 0-1000, sensor 0 = leftmost), using the
// classic Pololu-style weighted average. "Calibrated" values must already
// be oriented so higher = more "on the line" -- invert per-channel upstream
// if your track's line/background contrast reads the opposite way.
//
// If the maximum calibrated reading is below `onLineThreshold` (line not
// detected by any sensor), returns -1 and sets *lineDetected to false;
// caller should hold last known position or invoke a failsafe.
int computeLinePosition(const uint16_t calibratedValues[8], uint16_t onLineThreshold, bool* lineDetected);

} // namespace linepos
```

- [ ] **Step 4: Write the failing test**

```cpp
// D:\line\firmware\test\test_line_position\test_line_position.cpp
#include <unity.h>
#include "line_position.h"

void setUp(void) {}
void tearDown(void) {}

void test_apply_calibration_midpoint(void) {
    uint16_t result = linepos::applyCalibration(500, 0, 1000);
    TEST_ASSERT_EQUAL_UINT16(500, result);
}

void test_apply_calibration_clamps_low(void) {
    uint16_t result = linepos::applyCalibration(0, 100, 900);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

void test_apply_calibration_clamps_high(void) {
    uint16_t result = linepos::applyCalibration(2000, 100, 900);
    TEST_ASSERT_EQUAL_UINT16(1000, result);
}

void test_apply_calibration_uncalibrated_returns_zero(void) {
    uint16_t result = linepos::applyCalibration(500, 500, 500);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

void test_compute_line_position_centered(void) {
    uint16_t values[8] = {0, 0, 0, 1000, 1000, 0, 0, 0};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(3500, pos);
}

void test_compute_line_position_left(void) {
    uint16_t values[8] = {1000, 0, 0, 0, 0, 0, 0, 0};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(0, pos);
}

void test_compute_line_position_right(void) {
    uint16_t values[8] = {0, 0, 0, 0, 0, 0, 0, 1000};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(7000, pos);
}

void test_compute_line_position_not_detected(void) {
    uint16_t values[8] = {10, 10, 10, 10, 10, 10, 10, 10};
    bool detected = true;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_FALSE(detected);
    TEST_ASSERT_EQUAL_INT(-1, pos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_calibration_midpoint);
    RUN_TEST(test_apply_calibration_clamps_low);
    RUN_TEST(test_apply_calibration_clamps_high);
    RUN_TEST(test_apply_calibration_uncalibrated_returns_zero);
    RUN_TEST(test_compute_line_position_centered);
    RUN_TEST(test_compute_line_position_left);
    RUN_TEST(test_compute_line_position_right);
    RUN_TEST(test_compute_line_position_not_detected);
    return UNITY_END();
}
```

- [ ] **Step 5: Run the test to verify it fails**

Run: `cd D:\line\firmware && pio test -e native -f test_line_position`
Expected: Build FAILS (or link fails) — `line_position.cpp` doesn't exist yet, so `linepos::applyCalibration`/`computeLinePosition` are undefined.

- [ ] **Step 6: Write the implementation**

```cpp
// D:\line\firmware\src\line_position.cpp
#include "line_position.h"

uint16_t linepos::applyCalibration(uint16_t rawValue, uint16_t minVal, uint16_t maxVal) {
    if (maxVal <= minVal) return 0;
    if (rawValue <= minVal) return 0;
    if (rawValue >= maxVal) return 1000;
    uint32_t scaled = static_cast<uint32_t>(rawValue - minVal) * 1000 / (maxVal - minVal);
    return static_cast<uint16_t>(scaled);
}

int linepos::computeLinePosition(const uint16_t calibratedValues[8], uint16_t onLineThreshold, bool* lineDetected) {
    static const int WEIGHTS[8] = {0, 1000, 2000, 3000, 4000, 5000, 6000, 7000};
    uint32_t weightedSum = 0;
    uint32_t valueSum = 0;
    uint16_t maxValue = 0;
    for (int i = 0; i < 8; i++) {
        weightedSum += static_cast<uint32_t>(calibratedValues[i]) * WEIGHTS[i];
        valueSum += calibratedValues[i];
        if (calibratedValues[i] > maxValue) maxValue = calibratedValues[i];
    }
    if (maxValue < onLineThreshold || valueSum == 0) {
        *lineDetected = false;
        return -1;
    }
    *lineDetected = true;
    return static_cast<int>(weightedSum / valueSum);
}
```

- [ ] **Step 7: Run the test to verify it passes**

Run: `cd D:\line\firmware && pio test -e native -f test_line_position`
Expected: `8 Tests 0 Failures 0 Ignored` and `PASSED`.

- [ ] **Step 8: Commit**

```bash
cd D:\line\firmware
git add platformio.ini include/config.h include/line_position.h src/line_position.cpp test/test_line_position
git commit -m "feat(firmware): scaffold PlatformIO project + line position module"
```

---

### Task 2: `pid_controller` module

**Files:**
- Create: `D:\line\firmware\include\pid_controller.h`
- Create: `D:\line\firmware\src\pid_controller.cpp`
- Test: `D:\line\firmware\test\test_pid\test_pid.cpp`

**Interfaces:**
- Produces: `class PidController { PidController(float kp, float ki, float kd); void setGains(float,float,float); void reset(); float step(float error, float dtSeconds); }`

- [ ] **Step 1: Write `pid_controller.h`**

```cpp
#pragma once

class PidController {
public:
    PidController(float kp, float ki, float kd);

    void setGains(float kp, float ki, float kd);
    void reset();

    // Computes PID output for the given error, given elapsed time in
    // seconds since the last call. Call reset() before reusing after a
    // pause to avoid integral windup / a derivative spike from a stale
    // last-error value.
    float step(float error, float dtSeconds);

private:
    float kp_;
    float ki_;
    float kd_;
    float integral_;
    float lastError_;
    bool hasLastError_;
};
```

- [ ] **Step 2: Write the failing test**

```cpp
// D:\line\firmware\test\test_pid\test_pid.cpp
#include <unity.h>
#include "pid_controller.h"

void setUp(void) {}
void tearDown(void) {}

void test_proportional_only_output(void) {
    PidController pid(2.0f, 0.0f, 0.0f);
    float output = pid.step(10.0f, 0.02f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_integral_accumulates(void) {
    PidController pid(0.0f, 1.0f, 0.0f);
    pid.step(10.0f, 1.0f);
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_derivative_reacts_to_change(void) {
    PidController pid(0.0f, 0.0f, 1.0f);
    pid.step(0.0f, 1.0f);
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

void test_reset_clears_state(void) {
    PidController pid(0.0f, 1.0f, 1.0f);
    pid.step(10.0f, 1.0f);
    pid.reset();
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_proportional_only_output);
    RUN_TEST(test_integral_accumulates);
    RUN_TEST(test_derivative_reacts_to_change);
    RUN_TEST(test_reset_clears_state);
    return UNITY_END();
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd D:\line\firmware && pio test -e native -f test_pid`
Expected: Build/link FAILS — `PidController` not yet implemented.

- [ ] **Step 4: Write the implementation**

```cpp
// D:\line\firmware\src\pid_controller.cpp
#include "pid_controller.h"

namespace {
constexpr float INTEGRAL_CLAMP = 10000.0f;
}

PidController::PidController(float kp, float ki, float kd)
    : kp_(kp), ki_(ki), kd_(kd), integral_(0), lastError_(0), hasLastError_(false) {}

void PidController::setGains(float kp, float ki, float kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}

void PidController::reset() {
    integral_ = 0;
    lastError_ = 0;
    hasLastError_ = false;
}

float PidController::step(float error, float dtSeconds) {
    integral_ += error * dtSeconds;
    if (integral_ > INTEGRAL_CLAMP) integral_ = INTEGRAL_CLAMP;
    if (integral_ < -INTEGRAL_CLAMP) integral_ = -INTEGRAL_CLAMP;

    float derivative = 0.0f;
    if (hasLastError_ && dtSeconds > 0.0f) {
        derivative = (error - lastError_) / dtSeconds;
    }
    lastError_ = error;
    hasLastError_ = true;

    return kp_ * error + ki_ * integral_ + kd_ * derivative;
}
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd D:\line\firmware && pio test -e native -f test_pid`
Expected: `4 Tests 0 Failures 0 Ignored` and `PASSED`.

- [ ] **Step 6: Commit**

```bash
cd D:\line\firmware
git add include/pid_controller.h src/pid_controller.cpp test/test_pid
git commit -m "feat(firmware): add PID controller module"
```

---

### Task 3: `protocol` module (JSON telemetry, binary telemetry, command parsing)

**Files:**
- Create: `D:\line\firmware\include\protocol.h`
- Create: `D:\line\firmware\src\protocol.cpp`
- Test: `D:\line\firmware\test\test_protocol\test_protocol.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks (standalone, but conceptually paired with `line_position`/`pid_controller` output fields)
- Produces: `protocol::Telemetry` struct, `protocol::TelemetryBinary` struct (17 bytes), `protocol::Command` struct, `protocol::CommandType` enum, `protocol::CalibrationState` enum, `protocol::encodeTelemetryJson(...)`, `protocol::encodeTelemetryBinary(...)`, `protocol::parseCommand(...)` — these are consumed by Tasks 4–9.

- [ ] **Step 1: Write `protocol.h`**

```cpp
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
```

- [ ] **Step 2: Write the failing test**

```cpp
// D:\line\firmware\test\test_protocol\test_protocol.cpp
#include <unity.h>
#include <cstring>
#include "protocol.h"

void setUp(void) {}
void tearDown(void) {}

void test_encode_telemetry_json_contains_fields(void) {
    protocol::Telemetry t{};
    t.timestampMs = 12345;
    for (int i = 0; i < 8; i++) t.sensors[i] = i * 100;
    t.linePosition = 3500;
    t.lineDetected = true;
    t.pidError = -50;
    t.pidOutput = 1.5f;
    t.leftSpeed = 120;
    t.rightSpeed = 130;
    t.calState = protocol::CalibrationState::Idle;
    t.pidEnabled = true;

    char buf[512];
    size_t n = protocol::encodeTelemetryJson(t, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pos\":3500"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"t\":12345"));
}

void test_encode_telemetry_binary_size(void) {
    TEST_ASSERT_EQUAL(17, sizeof(protocol::TelemetryBinary));
}

void test_encode_telemetry_binary_scales_sensors(void) {
    protocol::Telemetry t{};
    for (int i = 0; i < 8; i++) t.sensors[i] = 1000;
    t.linePosition = 3500;
    t.pidError = 0;
    t.leftSpeed = 255;
    t.rightSpeed = -255;
    t.lineDetected = true;
    t.pidEnabled = false;
    t.calState = protocol::CalibrationState::Idle;

    protocol::TelemetryBinary bin;
    protocol::encodeTelemetryBinary(t, 20, &bin);
    TEST_ASSERT_EQUAL(255, bin.sensors[0]);
    TEST_ASSERT_EQUAL(127, bin.leftSpeed);
    TEST_ASSERT_EQUAL(-127, bin.rightSpeed);
    TEST_ASSERT_EQUAL(20, bin.timestampDeltaMs);
    TEST_ASSERT_EQUAL(1, bin.flags & 0x01);
}

void test_parse_command_set_pid(void) {
    const char* json = "{\"cmd\":\"setPid\",\"kp\":1.5,\"ki\":0.02,\"kd\":0.3,\"base\":180}";
    protocol::Command cmd;
    bool ok = protocol::parseCommand(json, strlen(json), &cmd);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(static_cast<int>(protocol::CommandType::SetPid), static_cast<int>(cmd.type));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.5, cmd.kp);
    TEST_ASSERT_EQUAL(180, cmd.baseSpeed);
}

void test_parse_command_calibrate(void) {
    const char* json = "{\"cmd\":\"calibrate\",\"start\":false}";
    protocol::Command cmd;
    bool ok = protocol::parseCommand(json, strlen(json), &cmd);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(static_cast<int>(protocol::CommandType::Calibrate), static_cast<int>(cmd.type));
    TEST_ASSERT_FALSE(cmd.calibrateStart);
}

void test_parse_command_malformed_returns_false(void) {
    const char* json = "{not json";
    protocol::Command cmd;
    bool ok = protocol::parseCommand(json, strlen(json), &cmd);
    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_EQUAL(static_cast<int>(protocol::CommandType::Unknown), static_cast<int>(cmd.type));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_telemetry_json_contains_fields);
    RUN_TEST(test_encode_telemetry_binary_size);
    RUN_TEST(test_encode_telemetry_binary_scales_sensors);
    RUN_TEST(test_parse_command_set_pid);
    RUN_TEST(test_parse_command_calibrate);
    RUN_TEST(test_parse_command_malformed_returns_false);
    return UNITY_END();
}
```

- [ ] **Step 3: Run test to verify it fails**

Run: `cd D:\line\firmware && pio test -e native -f test_protocol`
Expected: Build/link FAILS — `protocol.cpp` doesn't exist yet.

- [ ] **Step 4: Write the implementation**

```cpp
// D:\line\firmware\src\protocol.cpp
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
```

- [ ] **Step 5: Run test to verify it passes**

Run: `cd D:\line\firmware && pio test -e native -f test_protocol`
Expected: `6 Tests 0 Failures 0 Ignored` and `PASSED`.

- [ ] **Step 6: Commit**

```bash
cd D:\line\firmware
git add include/protocol.h src/protocol.cpp test/test_protocol
git commit -m "feat(firmware): add shared protocol module (JSON + BLE binary + commands)"
```

---

### Task 4: `sensors` module (hardware)

**Files:**
- Create: `D:\line\firmware\include\sensors.h`
- Create: `D:\line\firmware\src\sensors.cpp`

**Interfaces:**
- Consumes: `linepos::applyCalibration` (Task 1)
- Produces: `class Sensors { void begin(const uint8_t pins[8], uint8_t emitterPin); void readRaw(uint16_t rawOut[8]); void applyCalibration(const uint16_t rawIn[8], uint16_t calibratedOut[8]); void invertChannel(uint8_t index, bool inverted); void startCalibration(); void sampleCalibration(const uint16_t rawIn[8]); void finishCalibration(); bool isCalibrating() const; void loadCalibration(); const uint16_t* calMin() const; const uint16_t* calMax() const; }` — consumed by Task 9 (`main.cpp`).

This module is hardware-dependent (ADC reads, NVS flash access) and cannot be unit tested on the native target — it is verified manually on real hardware in Step 4 below.

- [ ] **Step 1: Write `sensors.h`**

```cpp
#pragma once
#include <cstdint>
#include "line_position.h"

class Sensors {
public:
    void begin(const uint8_t pins[8], uint8_t emitterPin);

    // Reads raw ADC values (0-4095) from all 8 channels into rawOut.
    void readRaw(uint16_t rawOut[8]);

    // Applies stored calibration to raw values, writing calibrated 0-1000
    // values into calibratedOut. Orientation: higher = more "on line" as
    // recorded by calibration's max samples. If your track's line reads
    // the opposite way (e.g. white line on black vs. black line on white),
    // call invertChannel() for the affected sensors.
    void applyCalibration(const uint16_t rawIn[8], uint16_t calibratedOut[8]);

    void invertChannel(uint8_t index, bool inverted);

    // Calibration lifecycle: call startCalibration(), then feed it samples
    // every loop iteration via sampleCalibration(raw) while sweeping the
    // sensor across the line by hand, then finishCalibration() to persist.
    void startCalibration();
    void sampleCalibration(const uint16_t rawIn[8]);
    void finishCalibration(); // persists to NVS
    bool isCalibrating() const { return calibrating_; }

    void loadCalibration(); // called from begin(); reads NVS if present
    const uint16_t* calMin() const { return calMin_; }
    const uint16_t* calMax() const { return calMax_; }

private:
    uint8_t pins_[8] = {};
    uint8_t emitterPin_ = 255;
    uint16_t calMin_[8] = {};
    uint16_t calMax_[8] = {};
    bool inverted_[8] = {};
    bool calibrating_ = false;
};
```

- [ ] **Step 2: Write `sensors.cpp`**

```cpp
#include "sensors.h"
#include <Arduino.h>
#include <Preferences.h>

namespace {
constexpr const char* NVS_NAMESPACE = "qtr_cal";
}

void Sensors::begin(const uint8_t pins[8], uint8_t emitterPin) {
    for (int i = 0; i < 8; i++) {
        pins_[i] = pins[i];
        pinMode(pins_[i], INPUT);
        calMin_[i] = 4095;
        calMax_[i] = 0;
        inverted_[i] = false;
    }
    emitterPin_ = emitterPin;
    if (emitterPin_ != 255) {
        pinMode(emitterPin_, OUTPUT);
        digitalWrite(emitterPin_, HIGH); // emitters on
    }
    analogReadResolution(12); // 0-4095
    loadCalibration();
}

void Sensors::readRaw(uint16_t rawOut[8]) {
    for (int i = 0; i < 8; i++) {
        rawOut[i] = analogRead(pins_[i]);
    }
}

void Sensors::applyCalibration(const uint16_t rawIn[8], uint16_t calibratedOut[8]) {
    for (int i = 0; i < 8; i++) {
        uint16_t v = linepos::applyCalibration(rawIn[i], calMin_[i], calMax_[i]);
        calibratedOut[i] = inverted_[i] ? (1000 - v) : v;
    }
}

void Sensors::invertChannel(uint8_t index, bool inverted) {
    if (index < 8) inverted_[index] = inverted;
}

void Sensors::startCalibration() {
    calibrating_ = true;
    for (int i = 0; i < 8; i++) {
        calMin_[i] = 4095;
        calMax_[i] = 0;
    }
}

void Sensors::sampleCalibration(const uint16_t rawIn[8]) {
    if (!calibrating_) return;
    for (int i = 0; i < 8; i++) {
        if (rawIn[i] < calMin_[i]) calMin_[i] = rawIn[i];
        if (rawIn[i] > calMax_[i]) calMax_[i] = rawIn[i];
    }
}

void Sensors::finishCalibration() {
    calibrating_ = false;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes("min", calMin_, sizeof(calMin_));
    prefs.putBytes("max", calMax_, sizeof(calMax_));
    prefs.end();
}

void Sensors::loadCalibration() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    if (prefs.isKey("min") && prefs.isKey("max")) {
        prefs.getBytes("min", calMin_, sizeof(calMin_));
        prefs.getBytes("max", calMax_, sizeof(calMax_));
    }
    prefs.end();
}
```

- [ ] **Step 3: Compile for the ESP32 target**

Run: `cd D:\line\firmware && pio run -e esp32dev`
Expected: `SUCCESS` — this only checks compilation; the board doesn't need to be plugged in yet.

- [ ] **Step 4: Manual hardware verification (requires QTR-8A wired per `docs/wiring.md`, written in Task 10 — safe to defer this exact check until after Task 10, but compilation must pass now)**

Note in the commit message that hardware verification of readings is deferred to the Task 9 end-to-end check, since `sensors` alone has no way to print its output without `main.cpp`.

- [ ] **Step 5: Commit**

```bash
cd D:\line\firmware
git add include/sensors.h src/sensors.cpp
git commit -m "feat(firmware): add QTR-8A sensor reading + calibration module"
```

---

### Task 5: `motors` module (hardware)

**Files:**
- Create: `D:\line\firmware\include\motors.h`
- Create: `D:\line\firmware\src\motors.cpp`

**Interfaces:**
- Produces: `class Motors { void begin(uint8_t in1A, uint8_t in2A, uint8_t pwmA, uint8_t in1B, uint8_t in2B, uint8_t pwmB, uint8_t stby, int pwmFreqHz, int pwmResolutionBits); void setSpeeds(int leftSpeed, int rightSpeed); void stop(); }` — consumed by Task 9 (`main.cpp`).

- [ ] **Step 1: Write `motors.h`**

```cpp
#pragma once
#include <cstdint>

class Motors {
public:
    void begin(uint8_t in1A, uint8_t in2A, uint8_t pwmA,
               uint8_t in1B, uint8_t in2B, uint8_t pwmB,
               uint8_t stby, int pwmFreqHz, int pwmResolutionBits);

    // speed range -255..255; negative = reverse.
    void setSpeeds(int leftSpeed, int rightSpeed);
    void stop();

private:
    uint8_t in1A_ = 0, in2A_ = 0, pwmA_ = 0;
    uint8_t in1B_ = 0, in2B_ = 0, pwmB_ = 0;
    uint8_t stby_ = 0;
    int pwmMax_ = 255;

    void setMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin, int speed);
};
```

- [ ] **Step 2: Write `motors.cpp`**

```cpp
#include "motors.h"
#include <Arduino.h>

void Motors::begin(uint8_t in1A, uint8_t in2A, uint8_t pwmA,
                    uint8_t in1B, uint8_t in2B, uint8_t pwmB,
                    uint8_t stby, int pwmFreqHz, int pwmResolutionBits) {
    in1A_ = in1A; in2A_ = in2A; pwmA_ = pwmA;
    in1B_ = in1B; in2B_ = in2B; pwmB_ = pwmB;
    stby_ = stby;
    pwmMax_ = (1 << pwmResolutionBits) - 1;

    pinMode(in1A_, OUTPUT);
    pinMode(in2A_, OUTPUT);
    pinMode(in1B_, OUTPUT);
    pinMode(in2B_, OUTPUT);
    pinMode(stby_, OUTPUT);
    digitalWrite(stby_, HIGH); // take driver out of standby

    ledcAttach(pwmA_, pwmFreqHz, pwmResolutionBits);
    ledcAttach(pwmB_, pwmFreqHz, pwmResolutionBits);

    stop();
}

void Motors::setMotor(uint8_t in1, uint8_t in2, uint8_t pwmPin, int speed) {
    int clamped = speed;
    if (clamped > pwmMax_) clamped = pwmMax_;
    if (clamped < -pwmMax_) clamped = -pwmMax_;

    if (clamped >= 0) {
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
    } else {
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        clamped = -clamped;
    }
    ledcWrite(pwmPin, clamped);
}

void Motors::setSpeeds(int leftSpeed, int rightSpeed) {
    setMotor(in1A_, in2A_, pwmA_, leftSpeed);
    setMotor(in1B_, in2B_, pwmB_, rightSpeed);
}

void Motors::stop() {
    setSpeeds(0, 0);
}
```

- [ ] **Step 3: Compile for the ESP32 target**

Run: `cd D:\line\firmware && pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
cd D:\line\firmware
git add include/motors.h src/motors.cpp
git commit -m "feat(firmware): add TB6612FNG motor driver module"
```

Manual hardware verification (both motors spin the right direction at a safe low speed) happens as part of Task 9's end-to-end checklist, once `main.cpp` exists to drive them from a real command.

---

### Task 6: `transport_serial` module (hardware)

**Files:**
- Create: `D:\line\firmware\include\transport_serial.h`
- Create: `D:\line\firmware\src\transport_serial.cpp`

**Interfaces:**
- Consumes: `protocol::Telemetry`, `protocol::Command`, `protocol::encodeTelemetryJson`, `protocol::parseCommand` (Task 3)
- Produces: `class TransportSerial { void begin(unsigned long baud); void sendTelemetry(const protocol::Telemetry& t); bool pollCommand(protocol::Command* out); }` — consumed by Task 9.

- [ ] **Step 1: Write `transport_serial.h`**

```cpp
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
```

- [ ] **Step 2: Write `transport_serial.cpp`**

```cpp
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
```

- [ ] **Step 3: Compile for the ESP32 target**

Run: `cd D:\line\firmware && pio run -e esp32dev`
Expected: `SUCCESS`

- [ ] **Step 4: Commit**

```bash
cd D:\line\firmware
git add include/transport_serial.h src/transport_serial.cpp
git commit -m "feat(firmware): add USB serial transport"
```

Manual verification (flash, open a serial monitor, see JSON telemetry lines and send a command) happens in Task 9's end-to-end checklist once `main.cpp` ties everything together.

---

### Task 7: `transport_wifi` module (hardware)

**Files:**
- Create: `D:\line\firmware\include\transport_wifi.h`
- Create: `D:\line\firmware\src\transport_wifi.cpp`

**Interfaces:**
- Consumes: `protocol::Telemetry`, `protocol::Command`, `protocol::encodeTelemetryJson`, `protocol::parseCommand` (Task 3)
- Produces: `class TransportWifi { void begin(const char* apSsid, const char* apPassword); void sendTelemetry(const protocol::Telemetry& t); void onCommand(std::function<void(const protocol::Command&)> cb); }` — consumed by Task 9.

- [ ] **Step 1: Write `transport_wifi.h`**

```cpp
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
```

- [ ] **Step 2: Write `transport_wifi.cpp`**

```cpp
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
```

- [ ] **Step 3: Compile for the ESP32 target**

Run: `cd D:\line\firmware && pio run -e esp32dev`
Expected: `SUCCESS`. If it fails with library errors, confirm `ESP32Async/ESPAsyncWebServer` and `ESP32Async/AsyncTCP` resolved correctly in `.pio/libdeps` — these are the actively maintained forks (the older `me-no-dev` originals are unmaintained); adjust the version pin in `platformio.ini` if a newer release is needed.

- [ ] **Step 4: Commit**

```bash
cd D:\line\firmware
git add include/transport_wifi.h src/transport_wifi.cpp
git commit -m "feat(firmware): add WiFi access-point + WebSocket transport"
```

Manual verification (connect a laptop to the `LineFollower` WiFi network, open a WebSocket client to `ws://192.168.4.1/ws`, see JSON telemetry) happens in Task 9's end-to-end checklist.

---

### Task 8: `transport_ble` module (hardware)

**Files:**
- Create: `D:\line\firmware\include\transport_ble.h`
- Create: `D:\line\firmware\src\transport_ble.cpp`

**Interfaces:**
- Consumes: `protocol::Telemetry`, `protocol::TelemetryBinary`, `protocol::Command`, `protocol::encodeTelemetryBinary`, `protocol::parseCommand` (Task 3)
- Produces: `class TransportBle { void begin(const char* deviceName); void sendTelemetry(const protocol::Telemetry& t, uint16_t timestampDeltaMs); void onCommand(std::function<void(const protocol::Command&)> cb); }` — consumed by Task 9.

- [ ] **Step 1: Write `transport_ble.h`**

```cpp
#pragma once
#include "protocol.h"
#include <functional>

class TransportBle {
public:
    void begin(const char* deviceName);
    void sendTelemetry(const protocol::Telemetry& t, uint16_t timestampDeltaMs);
    void onCommand(std::function<void(const protocol::Command&)> cb);
};
```

- [ ] **Step 2: Write `transport_ble.cpp`**

```cpp
#include "transport_ble.h"
#include <NimBLEDevice.h>

namespace {
constexpr const char* SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* TELEMETRY_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* COMMAND_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

NimBLECharacteristic* telemetryChar = nullptr;
std::function<void(const protocol::Command&)> commandCallback;
char cmdLineBuf[256];
size_t cmdLineLen = 0;

void handleCommandData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = static_cast<char>(data[i]);
        if (c == '\n') {
            protocol::Command cmd;
            if (protocol::parseCommand(cmdLineBuf, cmdLineLen, &cmd) && commandCallback) {
                commandCallback(cmd);
            }
            cmdLineLen = 0;
            continue;
        }
        if (cmdLineLen < sizeof(cmdLineBuf) - 1) {
            cmdLineBuf[cmdLineLen++] = c;
        } else {
            cmdLineLen = 0;
        }
    }
}

// NOTE: NimBLE-Arduino's callback signature has changed across major
// versions (1.x passes only the characteristic; 2.x also passes a
// NimBLEConnInfo&). If this fails to compile against the resolved library
// version, check the installed NimBLE-Arduino examples for the current
// NimBLECharacteristicCallbacks::onWrite signature and adjust here.
class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        const std::string& v = c->getValue();
        handleCommandData(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

CommandCallbacks commandCallbacks;
} // namespace

void TransportBle::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEServer* bleServer = NimBLEDevice::createServer();
    NimBLEService* service = bleServer->createService(SERVICE_UUID);

    telemetryChar = service->createCharacteristic(
        TELEMETRY_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic* commandChar = service->createCharacteristic(
        COMMAND_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
    commandChar->setCallbacks(&commandCallbacks);

    service->start();
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();
}

void TransportBle::sendTelemetry(const protocol::Telemetry& t, uint16_t timestampDeltaMs) {
    if (!telemetryChar) return;
    protocol::TelemetryBinary bin;
    protocol::encodeTelemetryBinary(t, timestampDeltaMs, &bin);
    telemetryChar->setValue(reinterpret_cast<uint8_t*>(&bin), sizeof(bin));
    telemetryChar->notify();
}

void TransportBle::onCommand(std::function<void(const protocol::Command&)> cb) {
    commandCallback = cb;
}
```

- [ ] **Step 3: Compile for the ESP32 target**

Run: `cd D:\line\firmware && pio run -e esp32dev`
Expected: `SUCCESS`. If `NimBLECharacteristicCallbacks::onWrite` doesn't match the installed library version's signature, fix it per the note in the code above and re-run.

- [ ] **Step 4: Commit**

```bash
cd D:\line\firmware
git add include/transport_ble.h src/transport_ble.cpp
git commit -m "feat(firmware): add BLE transport (NimBLE GATT server)"
```

Manual verification (use a BLE scanner app such as nRF Connect to find "LineFollower", subscribe to the telemetry characteristic, confirm 17-byte notifications arrive) happens in Task 9's end-to-end checklist.

---

### Task 9: `main.cpp` integration + end-to-end hardware verification

**Files:**
- Create: `D:\line\firmware\src\main.cpp`

**Interfaces:**
- Consumes: everything from Tasks 1–8 (`Sensors`, `Motors`, `PidController`, `protocol::*`, `TransportSerial`, `TransportWifi`, `TransportBle`)
- Produces: the running firmware — no further tasks in this plan consume `main.cpp` directly, but the webapp plan's manual hardware verification steps depend on this firmware being flashed and running.

- [ ] **Step 1: Write `main.cpp`**

```cpp
#include <Arduino.h>
#include <cstring>
#include "config.h"
#include "protocol.h"
#include "sensors.h"
#include "motors.h"
#include "pid_controller.h"
#include "transport_serial.h"
#include "transport_wifi.h"
#include "transport_ble.h"

namespace {
Sensors sensors;
Motors motors;
PidController pid(DEFAULT_KP, DEFAULT_KI, DEFAULT_KD);
TransportSerial serialTransport;
TransportWifi wifiTransport;
TransportBle bleTransport;

int baseSpeed = DEFAULT_BASE_SPEED;
bool pidEnabled = false;
uint32_t lastTelemetryMs = 0;
uint32_t lastLoopMs = 0;
uint32_t lastLineSeenMs = 0;
uint32_t lastBleTelemetryMs = 0;

void applyCommand(const protocol::Command& cmd) {
    switch (cmd.type) {
        case protocol::CommandType::SetPid:
            pid.setGains(cmd.kp, cmd.ki, cmd.kd);
            baseSpeed = cmd.baseSpeed;
            break;
        case protocol::CommandType::Calibrate:
            if (cmd.calibrateStart) {
                sensors.startCalibration();
            } else {
                sensors.finishCalibration();
            }
            break;
        case protocol::CommandType::Start:
            pidEnabled = true;
            pid.reset();
            break;
        case protocol::CommandType::Stop:
            pidEnabled = false;
            motors.stop();
            break;
        default:
            break;
    }
}
} // namespace

void setup() {
    sensors.begin(SENSOR_PINS, SENSOR_EMITTER_PIN);
    motors.begin(MOTOR_A_IN1, MOTOR_A_IN2, MOTOR_A_PWM,
                 MOTOR_B_IN1, MOTOR_B_IN2, MOTOR_B_PWM,
                 MOTOR_STBY, PWM_FREQ, PWM_RESOLUTION_BITS);

    serialTransport.begin(115200);
    wifiTransport.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD);
    bleTransport.begin("LineFollower");
    wifiTransport.onCommand(applyCommand);
    bleTransport.onCommand(applyCommand);

    uint32_t now = millis();
    lastTelemetryMs = now;
    lastLoopMs = now;
    lastLineSeenMs = now;
    lastBleTelemetryMs = now;
}

void loop() {
    protocol::Command cmd;
    if (serialTransport.pollCommand(&cmd)) {
        applyCommand(cmd);
    }

    uint32_t now = millis();
    uint32_t telemetryIntervalMs = 1000 / TELEMETRY_HZ;
    if (now - lastTelemetryMs < telemetryIntervalMs) {
        return;
    }
    float dtSeconds = (now - lastLoopMs) / 1000.0f;
    lastLoopMs = now;
    lastTelemetryMs = now;

    uint16_t raw[8];
    uint16_t calibrated[8];
    sensors.readRaw(raw);

    if (sensors.isCalibrating()) {
        sensors.sampleCalibration(raw);
    }
    sensors.applyCalibration(raw, calibrated);

    bool lineDetected = false;
    int position = linepos::computeLinePosition(calibrated, 200, &lineDetected);

    int error = 0;
    float pidOutput = 0.0f;
    int leftSpeed = 0;
    int rightSpeed = 0;

    if (lineDetected) {
        lastLineSeenMs = now;
    }
    bool lineLost = (now - lastLineSeenMs) > LINE_LOST_TIMEOUT_MS;

    if (pidEnabled && !lineLost) {
        error = position - 3500;
        pidOutput = pid.step(static_cast<float>(error), dtSeconds);
        leftSpeed = baseSpeed + static_cast<int>(pidOutput);
        rightSpeed = baseSpeed - static_cast<int>(pidOutput);
        motors.setSpeeds(leftSpeed, rightSpeed);
    } else if (pidEnabled && lineLost) {
        motors.stop();
    }

    protocol::Telemetry t{};
    t.timestampMs = now;
    for (int i = 0; i < 8; i++) t.sensors[i] = calibrated[i];
    t.linePosition = static_cast<int16_t>(position);
    t.lineDetected = lineDetected;
    t.pidError = static_cast<int16_t>(error);
    t.pidOutput = pidOutput;
    t.leftSpeed = static_cast<int16_t>(leftSpeed);
    t.rightSpeed = static_cast<int16_t>(rightSpeed);
    t.pidEnabled = pidEnabled;
    t.calState = sensors.isCalibrating() ? protocol::CalibrationState::Calibrating
                                          : protocol::CalibrationState::Idle;
    if (t.calState == protocol::CalibrationState::Calibrating) {
        memcpy(t.calMin, sensors.calMin(), sizeof(t.calMin));
        memcpy(t.calMax, sensors.calMax(), sizeof(t.calMax));
    }

    serialTransport.sendTelemetry(t);
    wifiTransport.sendTelemetry(t);

    uint16_t bleDeltaMs = static_cast<uint16_t>(now - lastBleTelemetryMs);
    lastBleTelemetryMs = now;
    bleTransport.sendTelemetry(t, bleDeltaMs);
}
```

- [ ] **Step 2: Compile and flash**

Run: `cd D:\line\firmware && pio run -e esp32dev -t upload`
Expected: `SUCCESS` (with the ESP32 connected via USB and its port auto-detected, or pass `--upload-port COMx` if needed).

- [ ] **Step 3: Manual hardware verification — serial telemetry**

Run: `pio device monitor -b 115200`
Expected: a stream of JSON lines like `{"t":1234,"s":[12,45,...],"pos":3500,...}` at roughly 50/second. Wave a hand or a dark object over the sensors and confirm the `s` values change.

- [ ] **Step 4: Manual hardware verification — calibration**

Send `{"cmd":"calibrate","start":true}` followed by a newline through the serial monitor's send box while sweeping the sensor array across the line surface by hand for a few seconds, then send `{"cmd":"calibrate","start":false}`.
Expected: telemetry's `cal` field goes `1` (Calibrating) then back to `0` (Idle); reboot the board and confirm calibration persisted (min/max values are no longer the default 4095/0 extremes — verify by starting calibration again briefly and checking `cmin`/`cmax` reflect the previously narrowed range).

- [ ] **Step 5: Manual hardware verification — motors + PID**

With the robot's wheels off the ground (safety), send `{"cmd":"setPid","kp":0.06,"ki":0.0002,"kd":0.6,"base":100}` then `{"cmd":"start"}`.
Expected: both wheels spin; moving a line under the sensor array causes the wheel speeds to differ (turning response). Send `{"cmd":"stop"}` and confirm both wheels stop immediately.

- [ ] **Step 6: Manual hardware verification — WiFi**

On a laptop, connect to the `LineFollower` WiFi network (password `linefollow`). Use any WebSocket test client (e.g. a browser console: `new WebSocket('ws://192.168.4.1/ws')`) to connect and confirm JSON telemetry lines arrive.

- [ ] **Step 7: Manual hardware verification — BLE**

Using a BLE scanner app (e.g. nRF Connect), find the device named `LineFollower`, connect, subscribe (enable notifications) on characteristic `6e400002-b5a3-f393-e0a9-e50e24dcca9e`, and confirm 17-byte binary notifications arrive at roughly the telemetry rate.

- [ ] **Step 8: Commit**

```bash
cd D:\line\firmware
git add src/main.cpp
git commit -m "feat(firmware): wire sensors/motors/pid/transports together in main loop"
```

---

### Task 10: Protocol doc, wiring guide, firmware README

**Files:**
- Create: `D:\line\docs\protocol.md`
- Create: `D:\line\docs\wiring.md`
- Create: `D:\line\firmware\README.md`

**Interfaces:**
- Produces: `docs/protocol.md` — the canonical protocol reference the webapp plan's `Transport` implementations and `protocol.ts` decoder are written against.

- [ ] **Step 1: Write `docs/protocol.md`**

```markdown
# Line Follower Protocol Reference

Single logical protocol, two wire encodings. See
`firmware/include/protocol.h` for the authoritative struct/field
definitions — this document mirrors it for the web app implementation.

## Telemetry (ESP32 -> browser)

### JSON encoding (Serial, WiFi) — one line per sample, no trailing newline in the payload itself (transports append `\n` / send as one WS text frame)

```json
{
  "t": 12345,
  "s": [12, 45, 300, 980, 970, 250, 30, 10],
  "pos": 3500,
  "det": true,
  "err": -50,
  "out": 1.5,
  "ls": 120,
  "rs": 130,
  "cal": 0,
  "pid": true
}
```

Field meanings:
- `t`: timestamp, ms since ESP32 boot
- `s`: 8 calibrated sensor readings, 0-1000
- `pos`: line position, 0-7000, center (on-line) = 3500, or `-1` if not detected
- `det`: whether the line was detected this sample
- `err`: PID error (`pos - 3500`)
- `out`: raw PID output
- `ls`/`rs`: applied left/right motor speed, -255..255
- `cal`: calibration state — `0` idle, `1` calibrating, `2` done
- `cmin`/`cmax`: arrays of 8 raw calibration min/max values — present only while `cal != 0`
- `pid`: whether the PID line-following loop is currently enabled

### Binary encoding (BLE only) — 17 bytes, little-endian, packed

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0-1 | timestampDeltaMs | uint16 | ms since previous sample |
| 2-9 | sensors[8] | uint8[8] | calibrated 0-1000 scaled to 0-255 |
| 10-11 | linePosition | int16 | 0-7000, or -1 |
| 12-13 | pidError | int16 | |
| 14 | leftSpeed | int8 | -255..255 scaled to -127..127 |
| 15 | rightSpeed | int8 | same scaling |
| 16 | flags | uint8 | bit0=lineDetected, bit1=pidEnabled, bits2-3=calState |

Calibration min/max are NOT sent over BLE telemetry (JSON-only, since
calibration is a rare/manual flow) — the calibration panel should be used
over serial or WiFi if you need live min/max feedback while sweeping over
the line via BLE, or simply trust the "done" state.

## Commands (browser -> ESP32) — always JSON text, one command per line

```json
{"cmd": "setPid", "kp": 0.06, "ki": 0.0002, "kd": 0.6, "base": 150}
{"cmd": "calibrate", "start": true}
{"cmd": "calibrate", "start": false}
{"cmd": "start"}
{"cmd": "stop"}
```

Over Serial and WiFi, send one JSON object terminated by `\n` (serial) or
as one WebSocket text frame (WiFi — no `\n` needed, the frame boundary is
the message boundary). Over BLE, write to the command characteristic
(`6e400003-b5a3-f393-e0a9-e50e24dcca9e`); the firmware reassembles bytes
until it sees a `\n`, so it's fine to split a command across multiple BLE
writes.

## GATT UUIDs (BLE)

- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Telemetry characteristic (notify): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- Command characteristic (write): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
```

- [ ] **Step 2: Write `docs/wiring.md`**

```markdown
# Wiring Guide

Pin assignments mirror `firmware/include/config.h` — if you change one,
change both.

## QTR-8A Reflectance Sensor Array (8 channels, analog output)

| QTR-8A pin | ESP32 pin | Notes |
|---|---|---|
| CH1 | GPIO32 | ADC1_CH4 |
| CH2 | GPIO33 | ADC1_CH5 |
| CH3 | GPIO34 | ADC1_CH6 (input-only pin, fine for ADC) |
| CH4 | GPIO35 | ADC1_CH7 (input-only pin, fine for ADC) |
| CH5 | GPIO36 | ADC1_CH0 (input-only pin, fine for ADC) |
| CH6 | GPIO37 | ADC1_CH1 (input-only pin, fine for ADC) |
| CH7 | GPIO38 | ADC1_CH2 (input-only pin, fine for ADC) |
| CH8 | GPIO39 | ADC1_CH3 (input-only pin, fine for ADC) |
| LEDON | GPIO27 | digital output, emitter control |
| VCC | 5V (or 3.3V per your QTR-8A variant) | |
| GND | GND | |

**Do not move the 8 sensor channels to ADC2 pins (0,2,4,12-15,25-27)** —
ADC2 becomes unusable once WiFi is active on the ESP32, and this firmware
runs WiFi continuously.

## TB6612FNG Motor Driver

| TB6612FNG pin | ESP32 pin | Notes |
|---|---|---|
| AIN1 | GPIO16 | |
| AIN2 | GPIO17 | |
| PWMA | GPIO18 | LEDC PWM |
| BIN1 | GPIO19 | |
| BIN2 | GPIO21 | |
| PWMB | GPIO22 | LEDC PWM |
| STBY | GPIO23 | driven HIGH at boot to leave standby |
| VM | Motor battery voltage | |
| VCC | 3.3V | logic supply |
| GND | GND | shared with ESP32 GND |
| AO1/AO2 | Left motor | |
| BO1/BO2 | Right motor | |

## Board swap checklist

To use a different ESP32 board/variant:
1. Update `SENSOR_PINS` in `config.h` — must be 8 ADC1-capable pins.
2. Update the motor driver pins — avoid strapping pins (0, 2, 5, 12, 15 on
   classic ESP32; check your variant's datasheet for its own strapping
   pins, e.g. ESP32-S3 uses 0, 3, 45, 46).
3. Re-flash and re-run the Task 9 manual verification checklist.
```

- [ ] **Step 3: Write `firmware/README.md`**

```markdown
# Line Follower Firmware

PlatformIO project for the ESP32. Reads a QTR-8A sensor array, runs a PID
line-following loop driving a TB6612FNG motor driver, and streams
telemetry / accepts commands over USB serial, WiFi (as its own access
point), and BLE simultaneously.

## Build & flash

```bash
cd firmware
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

## Run native unit tests (no hardware required)

```bash
cd firmware
pio test -e native
```

Covers the pure logic modules: line position calculation, PID math, and
protocol encode/decode. Hardware-dependent modules (sensors, motors,
transports) are verified manually — see `docs/wiring.md` and the
end-to-end checklist in the implementation plan.

## Pin configuration

All pin assignments live in `include/config.h`. See `docs/wiring.md` for
the full wiring table and instructions for swapping to a different ESP32
board.

## WiFi

The ESP32 broadcasts its own access point:
- SSID: `LineFollower`
- Password: `linefollow`
- IP: `192.168.4.1`
- WebSocket endpoint: `ws://192.168.4.1/ws`

## Protocol

See `docs/protocol.md` for the full message reference (JSON schema, BLE
binary layout, GATT UUIDs).
```

- [ ] **Step 4: Commit**

```bash
cd D:\line
git add docs/protocol.md docs/wiring.md firmware/README.md
git commit -m "docs: add protocol reference, wiring guide, firmware README"
```
