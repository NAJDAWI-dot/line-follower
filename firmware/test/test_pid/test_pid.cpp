// D:\line\firmware\test\test_pid\test_pid.cpp
#include <unity.h>
#include "pid_controller.h"

void setUp(void) {}
void tearDown(void) {}

void test_proportional_only_output(void) {
    PidController pid(2.0f, 0.0f, 0.0f);
    float output = pid.step(10.0f, 0.02f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_integral_accumulates(void) {
    PidController pid(0.0f, 1.0f, 0.0f);
    pid.step(10.0f, 1.0f);
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, output);
}

void test_derivative_reacts_to_change(void) {
    PidController pid(0.0f, 0.0f, 1.0f);
    pid.step(0.0f, 1.0f);
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

void test_reset_clears_state(void) {
    PidController pid(0.0f, 1.0f, 1.0f);
    pid.step(10.0f, 1.0f);
    pid.reset();
    float output = pid.step(10.0f, 1.0f);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, output);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_proportional_only_output);
    RUN_TEST(test_integral_accumulates);
    RUN_TEST(test_derivative_reacts_to_change);
    RUN_TEST(test_reset_clears_state);
    return UNITY_END();
}
