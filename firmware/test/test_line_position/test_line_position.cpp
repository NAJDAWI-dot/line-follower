// D:\line\firmware\test\test_line_position\test_line_position.cpp
#include <unity.h>
#include "line_position.h"

void setUp(void) {}
void tearDown(void) {}

void test_apply_calibration_midpoint(void) {
    uint16_t result = linepos::applyCalibration(500, 0, 1000);
    TEST_ASSERT_EQUAL_UINT16(500, result);
}

void test_apply_calibration_clamps_low(void) {
    uint16_t result = linepos::applyCalibration(0, 100, 900);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

void test_apply_calibration_clamps_high(void) {
    uint16_t result = linepos::applyCalibration(2000, 100, 900);
    TEST_ASSERT_EQUAL_UINT16(1000, result);
}

void test_apply_calibration_uncalibrated_returns_zero(void) {
    uint16_t result = linepos::applyCalibration(500, 500, 500);
    TEST_ASSERT_EQUAL_UINT16(0, result);
}

void test_compute_line_position_centered(void) {
    uint16_t values[8] = {0, 0, 0, 1000, 1000, 0, 0, 0};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(3500, pos);
}

void test_compute_line_position_left(void) {
    uint16_t values[8] = {1000, 0, 0, 0, 0, 0, 0, 0};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(0, pos);
}

void test_compute_line_position_right(void) {
    uint16_t values[8] = {0, 0, 0, 0, 0, 0, 0, 1000};
    bool detected = false;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_TRUE(detected);
    TEST_ASSERT_EQUAL_INT(7000, pos);
}

void test_compute_line_position_not_detected(void) {
    uint16_t values[8] = {10, 10, 10, 10, 10, 10, 10, 10};
    bool detected = true;
    int pos = linepos::computeLinePosition(values, 200, &detected);
    TEST_ASSERT_FALSE(detected);
    TEST_ASSERT_EQUAL_INT(-1, pos);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_calibration_midpoint);
    RUN_TEST(test_apply_calibration_clamps_low);
    RUN_TEST(test_apply_calibration_clamps_high);
    RUN_TEST(test_apply_calibration_uncalibrated_returns_zero);
    RUN_TEST(test_compute_line_position_centered);
    RUN_TEST(test_compute_line_position_left);
    RUN_TEST(test_compute_line_position_right);
    RUN_TEST(test_compute_line_position_not_detected);
    return UNITY_END();
}
