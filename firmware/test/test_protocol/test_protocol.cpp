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

void test_parse_command_start(void) {
    const char* json = "{\"cmd\":\"start\"}";
    protocol::Command cmd;
    bool ok = protocol::parseCommand(json, strlen(json), &cmd);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(static_cast<int>(protocol::CommandType::Start), static_cast<int>(cmd.type));
}

void test_parse_command_stop(void) {
    const char* json = "{\"cmd\":\"stop\"}";
    protocol::Command cmd;
    bool ok = protocol::parseCommand(json, strlen(json), &cmd);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL(static_cast<int>(protocol::CommandType::Stop), static_cast<int>(cmd.type));
}

void test_encode_telemetry_json_all_fields_present(void) {
    protocol::Telemetry t{};
    t.timestampMs = 12345;
    for (int i = 0; i < 8; i++) {
        t.sensors[i] = i * 100;
    }
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

    // Verify all required fields are present
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"t\":"));          // timestamp
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"s\":"));          // sensors array
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pos\":"));        // line position
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"det\":"));        // line detected
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"err\":"));        // PID error
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"out\":"));        // PID output
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"ls\":"));         // left speed
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"rs\":"));         // right speed
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"cal\":"));        // calibration state
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"pid\":"));        // PID enabled
}

void test_encode_telemetry_json_cmin_cmax_conditional(void) {
    // Test with Idle state: cmin/cmax should NOT be present
    protocol::Telemetry t{};
    t.timestampMs = 12345;
    for (int i = 0; i < 8; i++) t.sensors[i] = 500;
    t.linePosition = 3500;
    t.lineDetected = true;
    t.pidError = 0;
    t.pidOutput = 0.0f;
    t.leftSpeed = 100;
    t.rightSpeed = 100;
    t.calState = protocol::CalibrationState::Idle;
    t.pidEnabled = true;

    char buf[512];
    size_t n = protocol::encodeTelemetryJson(t, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NULL(strstr(buf, "\"cmin\":"));
    TEST_ASSERT_NULL(strstr(buf, "\"cmax\":"));

    // Test with Calibrating state: cmin/cmax SHOULD be present
    t.calState = protocol::CalibrationState::Calibrating;
    for (int i = 0; i < 8; i++) {
        t.calMin[i] = i * 50;
        t.calMax[i] = 1000 - (i * 50);
    }

    n = protocol::encodeTelemetryJson(t, buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"cmin\":"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"cmax\":"));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_encode_telemetry_json_contains_fields);
    RUN_TEST(test_encode_telemetry_binary_size);
    RUN_TEST(test_encode_telemetry_binary_scales_sensors);
    RUN_TEST(test_parse_command_set_pid);
    RUN_TEST(test_parse_command_calibrate);
    RUN_TEST(test_parse_command_malformed_returns_false);
    RUN_TEST(test_parse_command_start);
    RUN_TEST(test_parse_command_stop);
    RUN_TEST(test_encode_telemetry_json_all_fields_present);
    RUN_TEST(test_encode_telemetry_json_cmin_cmax_conditional);
    return UNITY_END();
}
