// test_polar_coords.cpp
// Unit tests for polar coordinate conversion

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "polar_coords.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  Testing: %s\n", name); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("    FAILED: %s at line %d\n", #condition, __LINE__); \
        return; \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_MSG(condition, msg) do { \
    if (!(condition)) { \
        printf("    FAILED: %s - %s at line %d\n", #condition, msg, __LINE__); \
        return; \
    } \
    tests_passed++; \
} while(0)

// Test positive X axis (angle should be ~0)
void test_positive_x_axis() {
    TEST("Positive X axis gives angle ~0");

    // x = 100, y = 0 (scaled up as in original: << 22)
    int32_t x = 100 << 22;
    int32_t y = 0;

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    // Angle should be 0 (pointing right)
    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x >> 22, y >> 22, polar.angle, polar.distance);
    ASSERT_MSG(polar.angle == 0 || polar.angle == 1, "Angle should be ~0 for positive X");
    ASSERT_MSG(polar.distance > 0, "Distance should be positive");
}

// Test positive Y axis (angle should be ~90 degrees = 0x40000000)
void test_positive_y_axis() {
    TEST("Positive Y axis gives angle ~90 degrees");

    int32_t x = 0;
    int32_t y = 100 << 22;

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x >> 22, y >> 22, polar.angle, polar.distance);
    // Angle should be 0x40000000 (90 degrees)
    // Allow some tolerance due to lookup table discretization
    int32_t expected = 0x40000000;
    int32_t diff = polar.angle - expected;
    if (diff < 0) diff = -diff;
    ASSERT_MSG(diff < 0x02000000, "Angle should be ~0x40000000 for positive Y");
}

// Test negative X axis (angle should be ~180 degrees = 0x80000000)
void test_negative_x_axis() {
    TEST("Negative X axis gives angle ~180 degrees");

    int32_t x = -(100 << 22);
    int32_t y = 0;

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x >> 22, y >> 22, polar.angle, polar.distance);
    // Angle should be near 0x80000000 (180 degrees)
    int32_t expected = static_cast<int32_t>(0x80000000);
    int32_t diff = polar.angle - expected;
    if (diff < 0) diff = -diff;
    ASSERT_MSG(diff < 0x02000000, "Angle should be ~0x80000000 for negative X");
}

// Test negative Y axis (angle should be ~270 degrees = 0xC0000000)
void test_negative_y_axis() {
    TEST("Negative Y axis gives angle ~270 degrees");

    int32_t x = 0;
    int32_t y = -(100 << 22);

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x >> 22, y >> 22, polar.angle, polar.distance);
    // Angle should be near 0xC0000000 (270 degrees / -90 degrees)
    int32_t expected = static_cast<int32_t>(0xC0000000);
    int32_t diff = polar.angle - expected;
    if (diff < 0) diff = -diff;
    ASSERT_MSG(diff < 0x02000000, "Angle should be ~0xC0000000 for negative Y");
}

// Test 45 degree diagonal (x = y, positive)
void test_45_degree_diagonal() {
    TEST("45 degree diagonal (x=y positive)");

    int32_t x = 100 << 22;
    int32_t y = 100 << 22;

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x >> 22, y >> 22, polar.angle, polar.distance);
    // Angle should be near 0x20000000 (45 degrees)
    int32_t expected = 0x20000000;
    int32_t diff = polar.angle - expected;
    if (diff < 0) diff = -diff;
    ASSERT_MSG(diff < 0x04000000, "Angle should be ~0x20000000 for 45 degree diagonal");
}

// Test distance calculation
void test_distance_calculation() {
    TEST("Distance increases with input magnitude");

    int32_t x1 = 50 << 22;
    int32_t y1 = 0;
    PolarCoordinates polar1 = getMouseInPolarCoordinates(x1, y1);

    int32_t x2 = 100 << 22;
    int32_t y2 = 0;
    PolarCoordinates polar2 = getMouseInPolarCoordinates(x2, y2);

    printf("    Distance at 50: 0x%08X\n", polar1.distance);
    printf("    Distance at 100: 0x%08X\n", polar2.distance);
    ASSERT_MSG(polar2.distance > polar1.distance, "Larger input should give larger distance");
}

// Test origin (x=0, y=0)
void test_origin() {
    TEST("Origin returns zero");

    int32_t x = 0;
    int32_t y = 0;

    PolarCoordinates polar = getMouseInPolarCoordinates(x, y);

    printf("    x=%d, y=%d -> angle=0x%08X, distance=0x%08X\n", x, y, polar.angle, polar.distance);
    // Distance should be 0 at origin
    ASSERT_MSG(polar.distance == 0, "Distance should be 0 at origin");
}

// Test all four quadrants produce different angles
void test_all_quadrants() {
    TEST("All four quadrants produce different angles");

    int32_t val = 100 << 22;

    // Q1: +x, +y (0 to 90 degrees)
    PolarCoordinates q1 = getMouseInPolarCoordinates(val, val);
    // Q2: -x, +y (90 to 180 degrees)
    PolarCoordinates q2 = getMouseInPolarCoordinates(-val, val);
    // Q3: -x, -y (180 to 270 degrees)
    PolarCoordinates q3 = getMouseInPolarCoordinates(-val, -val);
    // Q4: +x, -y (270 to 360 degrees)
    PolarCoordinates q4 = getMouseInPolarCoordinates(val, -val);

    printf("    Q1 (+,+): angle=0x%08X\n", q1.angle);
    printf("    Q2 (-,+): angle=0x%08X\n", q2.angle);
    printf("    Q3 (-,-): angle=0x%08X\n", q3.angle);
    printf("    Q4 (+,-): angle=0x%08X\n", q4.angle);

    // All angles should be different
    ASSERT_MSG(q1.angle != q2.angle, "Q1 and Q2 should have different angles");
    ASSERT_MSG(q2.angle != q3.angle, "Q2 and Q3 should have different angles");
    ASSERT_MSG(q3.angle != q4.angle, "Q3 and Q4 should have different angles");
    ASSERT_MSG(q4.angle != q1.angle, "Q4 and Q1 should have different angles");

    // Angles should be in order (increasing as we go counterclockwise)
    // Q1 < Q2 < Q3 < Q4 (treating as unsigned for wrap-around)
    uint32_t uq1 = static_cast<uint32_t>(q1.angle);
    uint32_t uq2 = static_cast<uint32_t>(q2.angle);
    uint32_t uq3 = static_cast<uint32_t>(q3.angle);
    uint32_t uq4 = static_cast<uint32_t>(q4.angle);
    ASSERT_MSG(uq1 < uq2, "Q1 angle should be less than Q2");
    ASSERT_MSG(uq2 < uq3, "Q2 angle should be less than Q3");
    ASSERT_MSG(uq3 < uq4, "Q3 angle should be less than Q4");
}

// Test symmetry: same magnitude, different signs should give same distance
void test_distance_symmetry() {
    TEST("Distance is symmetric across quadrants");

    int32_t val = 100 << 22;

    PolarCoordinates q1 = getMouseInPolarCoordinates(val, val);
    PolarCoordinates q2 = getMouseInPolarCoordinates(-val, val);
    PolarCoordinates q3 = getMouseInPolarCoordinates(-val, -val);
    PolarCoordinates q4 = getMouseInPolarCoordinates(val, -val);

    printf("    Q1 distance: 0x%08X\n", q1.distance);
    printf("    Q2 distance: 0x%08X\n", q2.distance);
    printf("    Q3 distance: 0x%08X\n", q3.distance);
    printf("    Q4 distance: 0x%08X\n", q4.distance);

    // All distances should be equal (same magnitude input)
    ASSERT_MSG(q1.distance == q2.distance, "Q1 and Q2 distances should match");
    ASSERT_MSG(q2.distance == q3.distance, "Q2 and Q3 distances should match");
    ASSERT_MSG(q3.distance == q4.distance, "Q3 and Q4 distances should match");
}

int main() {
    printf("=== Polar Coordinate Conversion Tests ===\n\n");

    test_positive_x_axis();
    test_positive_y_axis();
    test_negative_x_axis();
    test_negative_y_axis();
    test_45_degree_diagonal();
    test_distance_calculation();
    test_origin();
    test_all_quadrants();
    test_distance_symmetry();

    printf("\n=== Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Assertions passed: %d\n", tests_passed);

    // All tests passed if we have the expected number of assertions
    // (each test has 1-4 assertions, total expected ~18)
    if (tests_passed >= 18) {
        printf("\nAll tests passed!\n");
        return 0;
    } else {
        printf("\nSome tests failed.\n");
        return 1;
    }
}
