// polar_coords.cpp
// Mouse polar coordinate conversion for ship control

#include "polar_coords.h"
#include "lookup_tables.h"

// =============================================================================
// Polar Coordinate Conversion
// =============================================================================
//
// This is a direct port of GetMouseInPolarCoordinates from Lander.arm
// (lines 6568-6882). The algorithm:
//
// Part 1 - Calculate angle:
// 1. Take absolute values of x and y
// 2. Track signs in a flags variable for quadrant correction
// 3. Divide smaller by larger using shift-and-subtract division
// 4. Look up arctan in table
// 5. Adjust angle based on quadrant
//
// Part 2 - Calculate distance:
// 1. Square both x and y using shift-and-add multiplication
// 2. Add the squares
// 3. Look up square root in table
//
// =============================================================================

PolarCoordinates getMouseInPolarCoordinates(int32_t x, int32_t y) {
    PolarCoordinates result;

    // Track quadrant information in flags
    // Bit 0: set if we swapped x/y in division
    // Bit 1: set based on sign of x
    // Bit 2: set based on sign of y
    int32_t flags = 0;

    // Take absolute values and track signs
    int32_t absX = x;
    int32_t absY = y;

    if (x < 0) {
        flags ^= 0x03;  // Flip bits 0 and 1
        absX = -x;
    }

    if (y < 0) {
        flags ^= 0x07;  // Flip bits 0, 1, and 2
        absY = -y;
    }

    // Store absolute values for distance calculation later
    uint32_t savedAbsX = static_cast<uint32_t>(absX);
    uint32_t savedAbsY = static_cast<uint32_t>(absY);

    // =========================================================================
    // Part 1: Calculate the angle using arctan
    // =========================================================================

    // Divide smaller by larger to get ratio in [0, 1]
    // Result is in range 0 to 2^24
    uint32_t ratio;

    if (static_cast<uint32_t>(absX) < static_cast<uint32_t>(absY)) {
        // |x| < |y|, so calculate x/y
        flags ^= 0x01;  // Flip bit 0 to track swap

        // Shift-and-subtract division: ratio = (absX * 256) / absY
        // Using 8 iterations to get 8 bits of precision
        uint32_t numerator = static_cast<uint32_t>(absX);
        uint32_t denominator = static_cast<uint32_t>(absY);
        ratio = 0;
        uint32_t bit = 0x80;  // Start with bit 7

        for (int i = 0; i < 8; i++) {
            numerator <<= 1;
            if (numerator >= denominator) {
                numerator -= denominator;
                ratio |= bit;
            }
            bit >>= 1;
        }

        // Scale up result: ratio = 2^24 * (absX / absY)
        ratio <<= 24;
    } else {
        // |x| >= |y|, so calculate y/x
        uint32_t numerator = static_cast<uint32_t>(absY);
        uint32_t denominator = static_cast<uint32_t>(absX);

        // Handle division by zero case
        if (denominator == 0) {
            result.angle = 0;
            result.distance = 0;
            return result;
        }

        ratio = 0;
        uint32_t bit = 0x80;

        for (int i = 0; i < 8; i++) {
            numerator <<= 1;
            if (numerator >= denominator) {
                numerator -= denominator;
                ratio |= bit;
            }
            bit >>= 1;
        }

        // Scale up result: ratio = 2^24 * (absY / absX)
        ratio <<= 24;
    }

    // Look up arctan in table
    // Clear bits 23-24 for word alignment, then shift right by 23
    // This gives us an index in range 0-127 (128 entries in table)
    uint32_t index = (ratio & ~0x01800000u) >> 23;
    int32_t angle = getArctan(static_cast<int>(index));

    // Adjust angle based on quadrant
    // The original uses clever bit manipulation based on flags
    if ((flags & 0x01) == 0) {
        // Bit 0 is clear: R1 = R1 + R3 << 29
        angle = angle + (flags << 29);
    } else {
        // Bit 0 is set: R1 = (R3 + 1) << 29 - R1
        angle = ((flags + 1) << 29) - angle;
    }

    result.angle = angle;

    // =========================================================================
    // Part 2: Calculate the distance using Pythagoras
    // =========================================================================

    // Calculate x² using shift-and-add multiplication
    // The original multiplies by 2*absX and masks to top byte
    uint32_t xSquared = 0;
    uint32_t multiplicand = savedAbsX;
    uint32_t multiplier = (savedAbsX << 1) & 0xFE000000u;
    multiplier |= 0x01000000u;  // Ensure at least bit 24 is set

    while (multiplier != 0) {
        multiplicand >>= 1;
        if (multiplier & 0x80000000u) {
            xSquared += multiplicand;
        }
        multiplier <<= 1;
    }

    // Calculate y² using shift-and-add multiplication
    uint32_t ySquared = 0;
    multiplicand = savedAbsY;
    multiplier = (savedAbsY << 1) & 0xFE000000u;
    multiplier |= 0x01000000u;

    while (multiplier != 0) {
        multiplicand >>= 1;
        if (multiplier & 0x80000000u) {
            ySquared += multiplicand;
        }
        multiplier <<= 1;
    }

    // Sum of squares
    uint32_t sumSquares = xSquared + ySquared;

    // Look up square root
    // Clear bits 20-21 for word alignment, then shift right by 20
    // This gives us an index in range 0-1023 (1024 entries in table)
    index = (sumSquares & ~0x00300000u) >> 20;
    result.distance = getSqrt(static_cast<int>(index));

    return result;
}
