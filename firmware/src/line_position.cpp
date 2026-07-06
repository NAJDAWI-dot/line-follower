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
