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
