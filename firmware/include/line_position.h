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
