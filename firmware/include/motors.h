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
    uint8_t channelA_ = 0, channelB_ = 1;
    int pwmMax_ = 255;

    void setMotor(uint8_t in1, uint8_t in2, uint8_t channel, int speed);
};
