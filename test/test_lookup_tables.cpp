#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "../src/lookup_tables.h"

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
        std::printf("FAILED\n    Expected %s == %s\n    Got 0x%08X vs 0x%08X\n    at %s:%d\n", \
                    #a, #b, (unsigned)(a), (unsigned)(b), __FILE__, __LINE__); \
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
// Helper: Convert table value to float (-1.0 to 1.0 range)
// =============================================================================

constexpr double TABLE_MAX = 2147483647.0;  // 0x7FFFFFFF

double tableToFloat(int32_t value) {
    return static_cast<double>(value) / TABLE_MAX;
}

// =============================================================================
// Sine Table Tests
// =============================================================================

TEST(sin_table_size) {
    ASSERT_EQ(SIN_TABLE_SIZE, 1024);
}

TEST(sin_table_key_values) {
    // sin(0) = 0
    ASSERT_EQ(sinTable[0], 0x00000000);

    // sin(90°) = sin(256) = 1.0 = 0x7FFFFFFF (approximately)
    ASSERT_EQ(sinTable[256], 0x7FFFFFFE);  // Maximum positive value

    // sin(180°) = sin(512) = 0
    ASSERT_EQ(sinTable[512], 0x00000000);

    // sin(270°) = sin(768) = -1.0
    ASSERT_EQ(sinTable[768], static_cast<int32_t>(0x80000001));  // Maximum negative value
}

TEST(sin_table_symmetry) {
    // sin(x) = -sin(x + 180°) = -sin(x + 512)
    // The original table has small rounding errors from the Archimedes' FP unit
    int max_diff = 0;
    for (int i = 0; i < 512; i++) {
        int32_t positive = sinTable[i];
        int32_t negative = sinTable[i + 512];
        int diff = std::abs(positive + negative);
        if (diff > max_diff) max_diff = diff;
    }
    // The original table has differences of at most 3 (at index 470)
    ASSERT(max_diff <= 3);
}

TEST(sin_table_mathematical_accuracy) {
    // Verify values are close to actual sine function
    for (int i = 0; i < 1024; i++) {
        double expected = std::sin(2.0 * M_PI * i / 1024.0);
        double actual = tableToFloat(sinTable[i]);
        // Allow 0.001% error
        ASSERT_NEAR(actual, expected, 0.00001);
    }
}

TEST(getSin_wrapping) {
    // Test that getSin wraps correctly for values outside 0-1023
    ASSERT_EQ(getSin(0), getSin(1024));
    ASSERT_EQ(getSin(256), getSin(256 + 1024));
    ASSERT_EQ(getSin(100), getSin(100 + 2048));
    ASSERT_EQ(getSin(-1), getSin(1023));  // -1 should wrap to 1023
}

TEST(getCos_values) {
    // cos(0) = 1.0
    ASSERT_EQ(getCos(0), 0x7FFFFFFE);

    // cos(90°) = cos(256) = 0
    ASSERT_EQ(getCos(256), 0x00000000);

    // cos(180°) = cos(512) = -1.0
    ASSERT_EQ(getCos(512), static_cast<int32_t>(0x80000001));

    // cos(270°) = cos(768) = 0
    // Note: Due to wrapping, cos(768) = sin(768+256) = sin(1024) = sin(0) = 0
    ASSERT_EQ(getCos(768), 0x00000000);
}

TEST(sin_cos_relationship) {
    // sin²(x) + cos²(x) = 1 (approximately, allowing for fixed-point error)
    for (int i = 0; i < 1024; i += 32) {
        double s = tableToFloat(getSin(i));
        double c = tableToFloat(getCos(i));
        double sum = s*s + c*c;
        ASSERT_NEAR(sum, 1.0, 0.0001);
    }
}

// =============================================================================
// Arctan Table Tests
// =============================================================================

TEST(arctan_table_size) {
    ASSERT_EQ(ARCTAN_TABLE_SIZE, 128);
}

TEST(arctan_table_key_values) {
    // arctan(0) = 0
    ASSERT_EQ(arctanTable[0], 0x00000000);

    // arctan(1) = π/4, scaled by (0x7FFFFFFF / π)
    // arctan(128/128) = arctan(1) should be at index 128, but table only goes to 127
    // arctan(127/128) ≈ 0.782 radians
    // Last entry should be close to arctan(127/128) * (0x7FFFFFFF / π)
    ASSERT(arctanTable[127] > 0x1F000000);  // Should be a significant positive value
}

TEST(arctan_table_monotonic) {
    // Table should be strictly increasing
    for (int i = 1; i < 128; i++) {
        ASSERT(arctanTable[i] > arctanTable[i-1]);
    }
}

TEST(arctan_table_mathematical_accuracy) {
    // Verify values are close to actual arctan function
    // Formula: ((2^31 - 1) / π) * arctan(n / 128)
    double scale = TABLE_MAX / M_PI;

    for (int i = 0; i < 128; i++) {
        double expected = scale * std::atan(static_cast<double>(i) / 128.0);
        double actual = static_cast<double>(arctanTable[i]);
        // Allow 0.01% error
        double tolerance = std::max(1.0, std::abs(expected) * 0.0001);
        ASSERT_NEAR(actual, expected, tolerance);
    }
}

TEST(getArctan_clamping) {
    // Test boundary clamping
    ASSERT_EQ(getArctan(-10), arctanTable[0]);
    ASSERT_EQ(getArctan(0), arctanTable[0]);
    ASSERT_EQ(getArctan(127), arctanTable[127]);
    ASSERT_EQ(getArctan(200), arctanTable[127]);
}

// =============================================================================
// Square Root Table Tests
// =============================================================================

TEST(sqrt_table_size) {
    ASSERT_EQ(SQRT_TABLE_SIZE, 1024);
}

TEST(sqrt_table_key_values) {
    // sqrt(0) = 0
    ASSERT_EQ(squareRootTable[0], 0x00000000);

    // sqrt(1/1024) = 1/32 ≈ 0.03125
    // squareRootTable[1] should be approximately 0.03125 * 0x7FFFFFFF
    ASSERT(squareRootTable[1] > 0x03000000);
    ASSERT(squareRootTable[1] < 0x05000000);

    // sqrt(1023/1024) ≈ 0.9995, so should be close to 0x7FFFFFFF
    // Actual value from original is 0x7FEFFEFF
    ASSERT(squareRootTable[1023] > 0x7FE00000);
    ASSERT_EQ(squareRootTable[1023], 0x7FEFFEFF);
}

TEST(sqrt_table_monotonic) {
    // Table should be strictly increasing (except first entry)
    for (int i = 2; i < 1024; i++) {
        ASSERT(squareRootTable[i] > squareRootTable[i-1]);
    }
}

TEST(sqrt_table_mathematical_accuracy) {
    // Verify values are close to actual sqrt function
    // Formula: sqrt(n / 1024) * 0x7FFFFFFF
    for (int i = 1; i < 1024; i++) {
        double expected = std::sqrt(static_cast<double>(i) / 1024.0) * TABLE_MAX;
        double actual = static_cast<double>(squareRootTable[i]);
        // Allow 0.01% error
        double tolerance = std::max(1.0, std::abs(expected) * 0.0001);
        ASSERT_NEAR(actual, expected, tolerance);
    }
}

TEST(getSqrt_clamping) {
    // Test boundary clamping
    ASSERT_EQ(getSqrt(-10), squareRootTable[0]);
    ASSERT_EQ(getSqrt(0), squareRootTable[0]);
    ASSERT_EQ(getSqrt(1023), squareRootTable[1023]);
    ASSERT_EQ(getSqrt(2000), squareRootTable[1023]);
}

// =============================================================================
// Original Value Verification (spot checks from Lander.arm)
// =============================================================================

TEST(original_sin_values) {
    // Verify a few values directly from the original source
    ASSERT_EQ(sinTable[0], 0x00000000);
    ASSERT_EQ(sinTable[1], 0x00C90F87);
    ASSERT_EQ(sinTable[2], 0x01921D1F);
    ASSERT_EQ(sinTable[3], 0x025B26D7);
    ASSERT_EQ(sinTable[256], 0x7FFFFFFE);  // sin(90°)
}

TEST(original_arctan_values) {
    // Verify a few values directly from the original source
    ASSERT_EQ(arctanTable[0], 0x00000000);
    ASSERT_EQ(arctanTable[1], 0x00517C55);
    ASSERT_EQ(arctanTable[2], 0x00A2F61E);
    ASSERT_EQ(arctanTable[3], 0x00F46AD0);
}

TEST(original_sqrt_values) {
    // Verify a few values directly from the original source
    ASSERT_EQ(squareRootTable[0], 0x00000000);
    ASSERT_EQ(squareRootTable[1], 0x03FFFFFF);
    ASSERT_EQ(squareRootTable[2], 0x05A82799);
    ASSERT_EQ(squareRootTable[3], 0x06ED9EBA);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::printf("Lookup Tables Unit Tests\n");
    std::printf("========================\n\n");

    std::printf("Sine table tests:\n");
    RUN_TEST(sin_table_size);
    RUN_TEST(sin_table_key_values);
    RUN_TEST(sin_table_symmetry);
    RUN_TEST(sin_table_mathematical_accuracy);
    RUN_TEST(getSin_wrapping);
    RUN_TEST(getCos_values);
    RUN_TEST(sin_cos_relationship);

    std::printf("\nArctan table tests:\n");
    RUN_TEST(arctan_table_size);
    RUN_TEST(arctan_table_key_values);
    RUN_TEST(arctan_table_monotonic);
    RUN_TEST(arctan_table_mathematical_accuracy);
    RUN_TEST(getArctan_clamping);

    std::printf("\nSquare root table tests:\n");
    RUN_TEST(sqrt_table_size);
    RUN_TEST(sqrt_table_key_values);
    RUN_TEST(sqrt_table_monotonic);
    RUN_TEST(sqrt_table_mathematical_accuracy);
    RUN_TEST(getSqrt_clamping);

    std::printf("\nOriginal value verification:\n");
    RUN_TEST(original_sin_values);
    RUN_TEST(original_arctan_values);
    RUN_TEST(original_sqrt_values);

    std::printf("\n========================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
