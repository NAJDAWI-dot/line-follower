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
