#pragma once

class PidController {
public:
    PidController(float kp, float ki, float kd);

    void setGains(float kp, float ki, float kd);
    void reset();

    // Computes PID output for the given error, given elapsed time in
    // seconds since the last call. Call reset() before reusing after a
    // pause to avoid integral windup / a derivative spike from a stale
    // last-error value.
    float step(float error, float dtSeconds);

private:
    float kp_;
    float ki_;
    float kd_;
    float integral_;
    float lastError_;
    bool hasLastError_;
};
