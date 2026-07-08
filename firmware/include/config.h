#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// BOARD PIN CONFIGURATION -- edit this block when switching ESP32 boards.
// ---------------------------------------------------------------------------

// QTR-8A sensor analog inputs, far-left (index 0) to far-right (index 7) --
// this order must match physical left-to-right placement, since
// line_position.cpp's weighted-centroid calc assumes it.
//
// KNOWN LIMITATION (accepted): indices 6-7 (GPIO 25, 26) are ADC2 channels.
// ADC2 becomes unreliable while the ESP32 WiFi radio is active, and this
// firmware runs a WiFi access point continuously for the WiFi transport.
// Expect noisy/garbage readings on the two far-right sensors whenever a
// WiFi client is connected (or the AP is simply up). USB serial and BLE
// connections do not trigger this -- only WiFi does. If line-following
// accuracy degrades specifically when WiFi is in use, this is why.
constexpr uint8_t SENSOR_PINS[8] = {36, 39, 34, 35, 32, 33, 25, 26};

// QTR-8A emitter (IR LED) control pin, a.k.a. LEDON. 255 = no control pin
// (emitters wired always-on).
constexpr uint8_t SENSOR_EMITTER_PIN = 255;

// TB6612FNG motor driver pins.
//
// KNOWN LIMITATION (accepted): MOTOR_A_PWM (15), MOTOR_A_IN1 (2), and
// MOTOR_B_PWM (5) are ESP32 strapping pins, sampled at boot to select boot
// mode / flash timing. A motor driver holding these lines in an unexpected
// state during the boot window can in principle cause boot glitches; this
// is a known ESP32 gotcha, accepted here as low-risk in practice for a
// TB6612 (its inputs don't drive these lines during the ESP32's own boot
// sampling window). If you see intermittent boot failures or flash-mode
// errors, this is the first thing to suspect and remap.
constexpr uint8_t MOTOR_A_PWM = 15;
constexpr uint8_t MOTOR_A_IN1 = 2;
constexpr uint8_t MOTOR_A_IN2 = 4;
constexpr uint8_t MOTOR_B_PWM = 5;
constexpr uint8_t MOTOR_B_IN1 = 18;
constexpr uint8_t MOTOR_B_IN2 = 19;

// STBY tied directly to 3.3V in hardware -- no GPIO control. 255 tells
// Motors::begin() to skip configuring a standby pin.
constexpr uint8_t MOTOR_STBY = 255;

// ---------------------------------------------------------------------------
// TUNABLES
// ---------------------------------------------------------------------------

constexpr uint32_t TELEMETRY_HZ = 50;
constexpr uint32_t LINE_LOST_TIMEOUT_MS = 500;
constexpr uint16_t LINE_DETECT_THRESHOLD = 200;

constexpr int PWM_FREQ = 5000;
constexpr int PWM_RESOLUTION_BITS = 8; // duty range 0-255

constexpr float DEFAULT_KP = 0.06f;
constexpr float DEFAULT_KI = 0.0002f;
constexpr float DEFAULT_KD = 0.6f;
constexpr int DEFAULT_BASE_SPEED = 150; // 0-255

// WiFi transport disabled: the always-on AP locks ADC2, which hard-kills
// sensors 7-8 on GPIO25/26 (reads time out entirely, not just noise) --
// and this board has no spare ADC1 pins to move them to. USB serial and
// BLE remain. Set to 1 only if you drop to 6 sensors or add an external
// ADC for the right side.
#define ENABLE_WIFI_TRANSPORT 0

constexpr const char* WIFI_AP_SSID = "LineFollower";
constexpr const char* WIFI_AP_PASSWORD = "linefollow"; // WPA2 needs >= 8 chars
