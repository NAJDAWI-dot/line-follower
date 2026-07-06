#include "pid_controller.h"

namespace {
constexpr float INTEGRAL_CLAMP = 10000.0f;
}

PidController::PidController(float kp, float ki, float kd)
    : kp_(kp), ki_(ki), kd_(kd), integral_(0), lastError_(0), hasLastError_(false) {}

void PidController::setGains(float kp, float ki, float kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}

void PidController::reset() {
    integral_ = 0;
    lastError_ = 0;
    hasLastError_ = false;
}

float PidController::step(float error, float dtSeconds) {
    integral_ += error * dtSeconds;
    if (integral_ > INTEGRAL_CLAMP) integral_ = INTEGRAL_CLAMP;
    if (integral_ < -INTEGRAL_CLAMP) integral_ = -INTEGRAL_CLAMP;

    float derivative = 0.0f;
    if (hasLastError_ && dtSeconds > 0.0f) {
        derivative = (error - lastError_) / dtSeconds;
    }
    lastError_ = error;
    hasLastError_ = true;

    return kp_ * error + ki_ * integral_ + kd_ * derivative;
}
