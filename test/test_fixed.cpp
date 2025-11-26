#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "../src/fixed.h"

// =============================================================================
// Simple Test Framework
// =============================================================================

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::printf("  %s... ", #name); \
    testsRun++; \
    try { \
        test_##name(); \
        testsPassed++; \
        std::printf("PASSED\n"); \
    } catch (...) { \
        testsFailed++; \
        std::printf("FAILED (exception)\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", \
                    #cond, __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("FAILED\n    Expected %s == %s\n    at %s:%d\n", \
                    #a, #b, __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, tolerance) do { \
    if (std::fabs((a) - (b)) > (tolerance)) { \
        std::printf("FAILED\n    Expected %s ~= %s (tolerance %f)\n    Got %f vs %f\n    at %s:%d\n", \
                    #a, #b, (double)(tolerance), (double)(a), (double)(b), __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

// =============================================================================
// Fixed-Point Tests
// =============================================================================

TEST(construction_from_raw) {
    Fixed a = Fixed::fromRaw(0x01000000);
    ASSERT_EQ(a.raw, 0x01000000);

    Fixed b = Fixed::fromRaw(-0x01000000);
    ASSERT_EQ(b.raw, -0x01000000);

    Fixed c = Fixed::fromRaw(0);
    ASSERT_EQ(c.raw, 0);
}

TEST(construction_from_int) {
    Fixed a = Fixed::fromInt(1);
    ASSERT_EQ(a.raw, 0x01000000);

    Fixed b = Fixed::fromInt(5);
    ASSERT_EQ(b.raw, 0x05000000);

    Fixed c = Fixed::fromInt(-3);
    ASSERT_EQ(c.raw, -0x03000000);

    Fixed d = Fixed::fromInt(0);
    ASSERT_EQ(d.raw, 0);
}

TEST(construction_from_float) {
    Fixed a = Fixed::fromFloat(1.0f);
    ASSERT_EQ(a.raw, 0x01000000);

    Fixed b = Fixed::fromFloat(0.5f);
    ASSERT_EQ(b.raw, 0x00800000);

    Fixed c = Fixed::fromFloat(0.25f);
    ASSERT_EQ(c.raw, 0x00400000);

    Fixed d = Fixed::fromFloat(-1.5f);
    ASSERT_EQ(d.raw, -0x01800000);
}

TEST(conversion_to_int) {
    ASSERT_EQ(Fixed::fromRaw(0x01000000).toInt(), 1);
    ASSERT_EQ(Fixed::fromRaw(0x05000000).toInt(), 5);
    ASSERT_EQ(Fixed::fromRaw(-0x03000000).toInt(), -3);
    ASSERT_EQ(Fixed::fromRaw(0x01800000).toInt(), 1);  // 1.5 truncates to 1
    ASSERT_EQ(Fixed::fromRaw(-0x01800000).toInt(), -2); // -1.5 truncates toward negative
}

TEST(conversion_to_float) {
    ASSERT_NEAR(Fixed::fromRaw(0x01000000).toFloat(), 1.0f, 0.0001f);
    ASSERT_NEAR(Fixed::fromRaw(0x00800000).toFloat(), 0.5f, 0.0001f);
    ASSERT_NEAR(Fixed::fromRaw(0x00400000).toFloat(), 0.25f, 0.0001f);
    ASSERT_NEAR(Fixed::fromRaw(-0x01800000).toFloat(), -1.5f, 0.0001f);
}

TEST(addition) {
    Fixed a = Fixed::fromInt(3);
    Fixed b = Fixed::fromInt(4);
    Fixed c = a + b;
    ASSERT_EQ(c.toInt(), 7);

    Fixed d = Fixed::fromFloat(1.5f);
    Fixed e = Fixed::fromFloat(2.25f);
    Fixed f = d + e;
    ASSERT_NEAR(f.toFloat(), 3.75f, 0.0001f);

    // Test negative
    Fixed g = Fixed::fromInt(-5);
    Fixed h = Fixed::fromInt(3);
    ASSERT_EQ((g + h).toInt(), -2);
}

TEST(subtraction) {
    Fixed a = Fixed::fromInt(7);
    Fixed b = Fixed::fromInt(4);
    Fixed c = a - b;
    ASSERT_EQ(c.toInt(), 3);

    Fixed d = Fixed::fromFloat(3.75f);
    Fixed e = Fixed::fromFloat(1.25f);
    Fixed f = d - e;
    ASSERT_NEAR(f.toFloat(), 2.5f, 0.0001f);

    // Test negative result
    Fixed g = Fixed::fromInt(3);
    Fixed h = Fixed::fromInt(5);
    ASSERT_EQ((g - h).toInt(), -2);
}

TEST(negation) {
    Fixed a = Fixed::fromInt(5);
    Fixed b = -a;
    ASSERT_EQ(b.toInt(), -5);

    Fixed c = Fixed::fromFloat(-2.5f);
    Fixed d = -c;
    ASSERT_NEAR(d.toFloat(), 2.5f, 0.0001f);
}

TEST(multiplication) {
    Fixed a = Fixed::fromInt(3);
    Fixed b = Fixed::fromInt(4);
    Fixed c = a * b;
    ASSERT_EQ(c.toInt(), 12);

    Fixed d = Fixed::fromFloat(2.5f);
    Fixed e = Fixed::fromFloat(4.0f);
    Fixed f = d * e;
    ASSERT_NEAR(f.toFloat(), 10.0f, 0.0001f);

    // Fractional multiplication
    Fixed g = Fixed::fromFloat(0.5f);
    Fixed h = Fixed::fromFloat(0.5f);
    Fixed i = g * h;
    ASSERT_NEAR(i.toFloat(), 0.25f, 0.0001f);

    // Negative multiplication
    Fixed j = Fixed::fromInt(-3);
    Fixed k = Fixed::fromInt(4);
    ASSERT_EQ((j * k).toInt(), -12);
}

TEST(division) {
    Fixed a = Fixed::fromInt(12);
    Fixed b = Fixed::fromInt(4);
    Fixed c = a / b;
    ASSERT_EQ(c.toInt(), 3);

    Fixed d = Fixed::fromFloat(10.0f);
    Fixed e = Fixed::fromFloat(4.0f);
    Fixed f = d / e;
    ASSERT_NEAR(f.toFloat(), 2.5f, 0.0001f);

    // Division resulting in fraction
    Fixed g = Fixed::fromInt(1);
    Fixed h = Fixed::fromInt(4);
    Fixed i = g / h;
    ASSERT_NEAR(i.toFloat(), 0.25f, 0.0001f);

    // Negative division
    Fixed j = Fixed::fromInt(-12);
    Fixed k = Fixed::fromInt(4);
    ASSERT_EQ((j / k).toInt(), -3);
}

TEST(compound_assignment) {
    Fixed a = Fixed::fromInt(5);
    a += Fixed::fromInt(3);
    ASSERT_EQ(a.toInt(), 8);

    Fixed b = Fixed::fromInt(10);
    b -= Fixed::fromInt(4);
    ASSERT_EQ(b.toInt(), 6);

    Fixed c = Fixed::fromInt(3);
    c *= Fixed::fromInt(4);
    ASSERT_EQ(c.toInt(), 12);

    Fixed d = Fixed::fromInt(12);
    d /= Fixed::fromInt(3);
    ASSERT_EQ(d.toInt(), 4);
}

TEST(comparison) {
    Fixed a = Fixed::fromInt(5);
    Fixed b = Fixed::fromInt(3);
    Fixed c = Fixed::fromInt(5);

    ASSERT(a > b);
    ASSERT(b < a);
    ASSERT(a >= b);
    ASSERT(a >= c);
    ASSERT(b <= a);
    ASSERT(a <= c);
    ASSERT(a == c);
    ASSERT(a != b);
}

TEST(bit_shifts) {
    Fixed a = Fixed::fromRaw(0x01000000);  // 1.0

    // Right shift divides by powers of 2
    Fixed b = a >> 1;
    ASSERT_EQ(b.raw, 0x00800000);  // 0.5

    Fixed c = a >> 2;
    ASSERT_EQ(c.raw, 0x00400000);  // 0.25

    // Left shift multiplies by powers of 2
    Fixed d = a << 1;
    ASSERT_EQ(d.raw, 0x02000000);  // 2.0

    Fixed e = a << 3;
    ASSERT_EQ(e.raw, 0x08000000);  // 8.0

    // Compound assignment
    Fixed f = Fixed::fromRaw(0x01000000);
    f >>= 2;
    ASSERT_EQ(f.raw, 0x00400000);

    Fixed g = Fixed::fromRaw(0x01000000);
    g <<= 2;
    ASSERT_EQ(g.raw, 0x04000000);
}

TEST(absolute_value) {
    Fixed a = Fixed::fromInt(5);
    ASSERT_EQ(a.abs().toInt(), 5);

    Fixed b = Fixed::fromInt(-5);
    ASSERT_EQ(b.abs().toInt(), 5);

    Fixed c = Fixed::fromFloat(-2.5f);
    ASSERT_NEAR(c.abs().toFloat(), 2.5f, 0.0001f);
}

TEST(game_constants) {
    using namespace GameConstants;

    // Verify TILE_SIZE is 1.0
    ASSERT_EQ(TILE_SIZE.raw, 0x01000000);
    ASSERT_NEAR(TILE_SIZE.toFloat(), 1.0f, 0.0001f);

    // Verify LAUNCHPAD_ALTITUDE = 0x03500000 = 3.3125
    ASSERT_EQ(LAUNCHPAD_ALTITUDE.raw, 0x03500000);
    ASSERT_NEAR(LAUNCHPAD_ALTITUDE.toFloat(), 3.3125f, 0.0001f);

    // Verify SEA_LEVEL = 0x05500000 = 5.3125
    ASSERT_EQ(SEA_LEVEL.raw, 0x05500000);
    ASSERT_NEAR(SEA_LEVEL.toFloat(), 5.3125f, 0.0001f);

    // Verify LANDING_SPEED = 0x00200000 = 0.125
    ASSERT_EQ(LANDING_SPEED.raw, 0x00200000);
    ASSERT_NEAR(LANDING_SPEED.toFloat(), 0.125f, 0.0001f);

    // Verify calculated constants
    ASSERT_EQ(LAUNCHPAD_SIZE.raw, TILE_SIZE.raw * 8);
    ASSERT_EQ(HIGHEST_ALTITUDE.raw, 0x34000000);  // 52 * TILE_SIZE

    // Verify tile counts
    ASSERT_EQ(TILES_X, 13);
    ASSERT_EQ(TILES_Z, 11);
    ASSERT_EQ(MAX_PARTICLES, 484);
}

TEST(original_arithmetic_patterns) {
    // Test patterns from the original ARM code
    // The original uses shifts for division by powers of 2

    Fixed velocity = Fixed::fromRaw(0x01000000);  // 1.0

    // Friction: v = v - (v >> 6) reduces by ~1.5%
    Fixed friction = velocity >> 6;
    Fixed afterFriction = velocity - friction;
    ASSERT_NEAR(afterFriction.toFloat(), 0.984375f, 0.0001f);

    // Thrust application: v -= (exhaust >> 11)
    Fixed exhaust = Fixed::fromRaw(0x01000000);
    Fixed thrustDelta = exhaust >> 11;
    ASSERT_EQ(thrustDelta.raw, 0x00002000);  // 0x01000000 >> 11 = 0x00002000

    // Hover thrust: v -= (exhaust >> 13)
    Fixed hoverDelta = exhaust >> 13;
    ASSERT_EQ(hoverDelta.raw, 0x00000800);  // 0x01000000 >> 13 = 0x00000800
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::printf("Fixed-Point Math Unit Tests\n");
    std::printf("===========================\n\n");

    std::printf("Construction tests:\n");
    RUN_TEST(construction_from_raw);
    RUN_TEST(construction_from_int);
    RUN_TEST(construction_from_float);

    std::printf("\nConversion tests:\n");
    RUN_TEST(conversion_to_int);
    RUN_TEST(conversion_to_float);

    std::printf("\nArithmetic tests:\n");
    RUN_TEST(addition);
    RUN_TEST(subtraction);
    RUN_TEST(negation);
    RUN_TEST(multiplication);
    RUN_TEST(division);
    RUN_TEST(compound_assignment);

    std::printf("\nComparison tests:\n");
    RUN_TEST(comparison);

    std::printf("\nBit shift tests:\n");
    RUN_TEST(bit_shifts);

    std::printf("\nUtility tests:\n");
    RUN_TEST(absolute_value);

    std::printf("\nGame constants tests:\n");
    RUN_TEST(game_constants);

    std::printf("\nOriginal code pattern tests:\n");
    RUN_TEST(original_arithmetic_patterns);

    std::printf("\n===========================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
