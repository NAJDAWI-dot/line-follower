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
int lastKnownPosition = 3500;

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
        lastKnownPosition = position;
    }
    bool lineLost = (now - lastLineSeenMs) > LINE_LOST_TIMEOUT_MS;

    if (pidEnabled && !lineLost) {
        int effectivePosition = lineDetected ? position : lastKnownPosition;
        error = effectivePosition - 3500;
        pidOutput = pid.step(static_cast<float>(error), dtSeconds);
        leftSpeed = baseSpeed + static_cast<int>(pidOutput);
        rightSpeed = baseSpeed - static_cast<int>(pidOutput);
        motors.setSpeeds(leftSpeed, rightSpeed);
    } else if (pidEnabled && lineLost) {
        motors.stop();
        pid.reset();
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
