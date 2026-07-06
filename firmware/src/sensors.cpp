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
