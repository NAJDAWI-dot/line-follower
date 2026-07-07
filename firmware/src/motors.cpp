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
    if (stby_ != 255) {
        pinMode(stby_, OUTPUT);
        digitalWrite(stby_, HIGH); // take driver out of standby
    } // else: STBY tied directly to a fixed high voltage in hardware, no GPIO control

    ledcAttachPin(pwmA_, channelA_);
    ledcSetup(channelA_, pwmFreqHz, pwmResolutionBits);
    ledcAttachPin(pwmB_, channelB_);
    ledcSetup(channelB_, pwmFreqHz, pwmResolutionBits);

    stop();
}

void Motors::setMotor(uint8_t in1, uint8_t in2, uint8_t channel, int speed) {
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
    ledcWrite(channel, clamped);
}

void Motors::setSpeeds(int leftSpeed, int rightSpeed) {
    setMotor(in1A_, in2A_, channelA_, leftSpeed);
    setMotor(in1B_, in2B_, channelB_, rightSpeed);
}

void Motors::stop() {
    setSpeeds(0, 0);
}
