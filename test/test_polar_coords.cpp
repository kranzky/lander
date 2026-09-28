// test_polar_coords.cpp
// Unit tests for polar coordinate conversion

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "polar_coords.h"
#include "lookup_tables.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  Testing: %s\n", name); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("    FAILED: %s at line %d\n", #condition, __LINE__); \
        tests_failed++; \
        return; \
    } \
    tests_passed++; \
} while(0)

#define ASSERT_MSG(condition, msg) do { \
    if (!(condition)) { \
        printf("    FAILED: %s - %s at line %d\n", #condition, msg, __LINE__); \
        tests_failed++; \
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
    TEST("Origin gives zero distance and the original's ~45 degree angle");

    PolarCoordinates polar = getMouseInPolarCoordinates(0, 0);

    // The original divides 0 by 0, which sets every quotient bit
    ASSERT_MSG(polar.distance == 0, "Distance should be 0 at origin");
    ASSERT_MSG(polar.angle == arctanTable[127], "Angle should be arctan(127/128)");
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

// =============================================================================
// Reference: the original ARM routine, instruction by instruction
// =============================================================================
//
// An independent emulation of GetMouseInPolarCoordinates (Lander.arm lines
// 6568-6882) with explicit registers and carry flag, including the byte
// offsets used for the table lookups.
//
// =============================================================================

static uint32_t armMultiplySquare(uint32_t r0) {
    bool c = (r0 & 0x80000000u) != 0;      // MOVS R3, R3, LSL #1 (R3 = R0)
    uint32_t r3 = r0 << 1;
    r3 &= 0xFE000000u;                      // AND R3, R3, #&FE000000
    r3 |= 0x01000000u;                      // ORR R3, R3, #&01000000
    uint32_t r4 = 0;
    do {
        r0 >>= 1;                           // MOV R0, R0, LSR #1
        if (c) r4 += r0;                    // ADDHS R4, R4, R0
        c = (r3 & 0x80000000u) != 0;        // MOVS R3, R3, LSL #1
        r3 <<= 1;
    } while (r3 != 0);                      // BNE pole5
    return r4;
}

static uint32_t armDivide(uint32_t num, uint32_t den) {
    uint32_t r2 = 0;
    uint32_t r14 = 0x80;
    bool c;
    do {
        c = (num & 0x80000000u) != 0;       // MOVS R0, R0, LSL #1
        num <<= 1;
        if (!c) c = num >= den;             // CMPCC R0, R1
        if (c) {                            // SUBCS R0, R0, R1
            num -= den;                     // ORRCS R2, R2, R14
            r2 |= r14;
        }
        c = (r14 & 1) != 0;                 // MOVS R14, R14, LSR #1
        r14 >>= 1;
    } while (!c);                           // BCC pole1
    return r2 << 24;                        // MOVS R2, R2, LSL #24
}

static PolarCoordinates armReference(uint32_t r0, uint32_t r1) {
    uint32_t r3 = 0;
    if (static_cast<int32_t>(r0) < 0) { r3 ^= 3; r0 = 0u - r0; }
    if (static_cast<int32_t>(r1) < 0) { r3 ^= 7; r1 = 0u - r1; }
    uint32_t absX = r0;
    uint32_t absY = r1;

    uint32_t r2;
    if (r0 < r1) {                          // CMP R0, R1 / EORLO / BHS pole2
        r3 ^= 1;
        r2 = armDivide(r0, r1);
    } else {
        r2 = armDivide(r1, r0);
    }

    r2 &= ~0x01800000u;                     // BIC R2, R2, #&01800000
    uint32_t angle = static_cast<uint32_t>(arctanTable[(r2 >> 23) / 4]);  // byte offset
    if (r3 & 1) {                           // TST R3, #1
        r3 += 1;                            // ADDNE R3, R3, #1
        angle = (r3 << 29) - angle;         // RSBNE R1, R1, R3, LSL #29
    } else {
        angle += r3 << 29;                  // ADDEQ R1, R1, R3, LSL #29
    }

    uint32_t sum = armMultiplySquare(absY) + armMultiplySquare(absX);
    sum &= ~0x00300000u;                    // BIC R2, R2, #&00300000
    int32_t distance = squareRootTable[(sum >> 20) / 4];  // byte offset

    return {static_cast<int32_t>(angle), distance};
}

static int32_t mouse(int value) {
    // The original's MOV R0, R0, LSL #22
    return static_cast<int32_t>(static_cast<uint32_t>(value) << 22);
}

// Test the port against the original for every mouse position
void test_matches_original_exhaustively() {
    TEST("Matches the original for every mouse position (-512..511)");

    int mismatches = 0;
    for (int x = -512; x <= 511; x++) {
        for (int y = -512; y <= 511; y++) {
            PolarCoordinates port = getMouseInPolarCoordinates(mouse(x), mouse(y));
            PolarCoordinates original = armReference(mouse(x), mouse(y));
            if (port.angle != original.angle || port.distance != original.distance) {
                if (mismatches++ < 5) {
                    printf("    (%d, %d): port 0x%08X/0x%08X, original 0x%08X/0x%08X\n", x, y,
                           port.angle, port.distance, original.angle, original.distance);
                }
            }
        }
    }
    ASSERT_MSG(mismatches == 0, "Port should match the original bit for bit");
}

// Test steering direction tracks the true mouse angle
void test_angle_accuracy() {
    TEST("Angle is within 1 degree of the true mouse angle");

    constexpr double DEGREES_PER_UNIT = 360.0 / 4294967296.0;
    double worst = 0.0;
    for (int degrees = 0; degrees < 360; degrees += 3) {
        double radians = degrees * M_PI / 180.0;
        int x = static_cast<int>(std::lround(400 * std::cos(radians)));
        int y = static_cast<int>(std::lround(400 * std::sin(radians)));
        double expected = std::atan2(y, x) * 180.0 / M_PI;
        double actual = static_cast<uint32_t>(getMouseInPolarCoordinates(mouse(x), mouse(y)).angle) *
                        DEGREES_PER_UNIT;
        double error = std::fabs(std::remainder(actual - expected, 360.0));
        worst = std::fmax(worst, error);
    }
    printf("    Worst angle error: %.2f degrees\n", worst);
    ASSERT_MSG(worst < 1.0, "Angles between the axes should not snap to the diagonal");
}

// Test distance is linear in mouse radius, reaching 0.5 at radius 512
void test_distance_accuracy() {
    TEST("Distance is radius / 1024, so full pitch is at radius 512");

    // Small radii are coarse in the original too (its squares use an 8-bit
    // multiplier and the square root table is sparse near zero)
    const int radii[] = {128, 256, 384, 511};
    for (int r : radii) {
        int d = r * 707 / 1000;  // Diagonal point at (about) the same radius
        double onAxis = getMouseInPolarCoordinates(mouse(r), 0).distance / 2147483647.0;
        double onDiagonal = getMouseInPolarCoordinates(mouse(d), mouse(d)).distance / 2147483647.0;
        double diagonalRadius = std::hypot(d, d);
        printf("    Radius %3d: axis %.4f (expected %.4f), diagonal %.4f (expected %.4f)\n",
               r, onAxis, r / 1024.0, onDiagonal, diagonalRadius / 1024.0);
        ASSERT_MSG(std::fabs(onAxis / (r / 1024.0) - 1.0) < 0.05, "Axis distance should be radius / 1024");
        ASSERT_MSG(std::fabs(onDiagonal / (diagonalRadius / 1024.0) - 1.0) < 0.05,
                   "Diagonal distance should be radius / 1024");
    }

    // The left edge (-512) is 2^31 in magnitude, which must not lose its top bit
    double leftEdge = getMouseInPolarCoordinates(mouse(-512), 0).distance / 2147483647.0;
    printf("    Left edge (-512): %.4f\n", leftEdge);
    ASSERT_MSG(std::fabs(leftEdge - 0.5) < 0.01, "Left edge should give full pitch");
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
    test_matches_original_exhaustively();
    test_angle_accuracy();
    test_distance_accuracy();

    printf("\n=== Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Assertions passed: %d, tests failed: %d\n", tests_passed, tests_failed);

    if (tests_failed == 0) {
        printf("\nAll tests passed!\n");
        return 0;
    }
    printf("\nSome tests failed.\n");
    return 1;
}
