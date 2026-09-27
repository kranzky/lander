// polar_coords.cpp
// Mouse polar coordinate conversion for ship control

#include "polar_coords.h"
#include "lookup_tables.h"

// =============================================================================
// Polar Coordinate Conversion
// =============================================================================
//
// A direct port of GetMouseInPolarCoordinates from Lander.arm (lines
// 6568-6882), reproducing its fixed-point arithmetic bit for bit, including
// the carries its shift loops depend on.
//
// Part 1 - Calculate angle:
// 1. Take absolute values of x and y, tracking the quadrant in flags
// 2. Divide the smaller by the larger with 8-step shift-and-subtract division
// 3. Look up the arctan of that ratio, then move it into the right quadrant
//
// Part 2 - Calculate distance:
// 1. Square both coordinates with 8-step shift-and-add multiplication
// 2. Look up the square root of the sum
//
// =============================================================================

namespace {
    // 256 * numerator / denominator (numerator <= denominator) in the top byte,
    // as the original's shift-and-subtract division (pole1/pole3). The
    // remainder is 64-bit so a bit shifted out of the top still counts, as the
    // original's carry flag does.
    uint32_t divideToTopByte(uint32_t numerator, uint32_t denominator) {
        uint64_t remainder = numerator;
        uint32_t quotient = 0;
        for (uint32_t bit = 0x80; bit != 0; bit >>= 1) {
            remainder <<= 1;
            if (remainder >= denominator) {
                remainder -= denominator;
                quotient |= bit;
            }
        }
        return quotient << 24;
    }

    // value^2 / 2^32 as the original's shift-and-add multiplication
    // (pole5/pole6), which uses only the top byte of value as the multiplier:
    // the first add uses bit 31 (the carry from doubling value) with value / 2,
    // and so on down to bit 24 with value / 256
    uint32_t square(uint32_t value) {
        uint32_t result = 0;
        for (int k = 1; k <= 8; k++) {
            if (value & (0x80000000u >> (k - 1))) {
                result += value >> k;
            }
        }
        return result;
    }
}

PolarCoordinates getMouseInPolarCoordinates(int32_t x, int32_t y) {
    // =========================================================================
    // Part 1: Calculate the angle using arctan
    // =========================================================================

    // Take unsigned magnitudes (so 0x80000000 is 2^31) and track the quadrant:
    // bits 0 and 1 flip for negative x, bits 0-2 for negative y
    uint32_t flags = 0;
    uint32_t absX = static_cast<uint32_t>(x);
    uint32_t absY = static_cast<uint32_t>(y);

    if (x < 0) {
        flags ^= 0x03;
        absX = 0u - absX;
    }
    if (y < 0) {
        flags ^= 0x07;
        absY = 0u - absY;
    }

    // Divide the smaller magnitude by the larger, flipping bit 0 if |x| < |y|
    uint32_t ratio;
    if (absX < absY) {
        flags ^= 0x01;
        ratio = divideToTopByte(absX, absY);
    } else {
        ratio = divideToTopByte(absY, absX);
    }

    // The table has 128 words indexed by the top 7 bits of the ratio (the
    // original clears bits 23-24 and uses ratio >> 23 as a byte offset)
    uint32_t angle = static_cast<uint32_t>(getArctan(static_cast<int>(ratio >> 25)));

    // Move the angle into the right octant (each is 2^29, or 45 degrees)
    if ((flags & 0x01) == 0) {
        angle += flags << 29;
    } else {
        angle = ((flags + 1) << 29) - angle;
    }

    // =========================================================================
    // Part 2: Calculate the distance using Pythagoras
    // =========================================================================

    // The table has 1024 words indexed by the top 10 bits of the sum (the
    // original clears bits 20-21 and uses sum >> 20 as a byte offset)
    uint32_t sumSquares = square(absX) + square(absY);
    int32_t distance = getSqrt(static_cast<int>(sumSquares >> 22));

    return {static_cast<int32_t>(angle), distance};
}
