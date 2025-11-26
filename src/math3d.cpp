#include "math3d.h"

// =============================================================================
// Rotation Matrix Calculation
// =============================================================================
//
// This implements the original CalculateRotationMatrix routine from Lander.arm.
//
// The original uses the sine table where:
//   - Index 0-1023 covers 0 to 2π
//   - Adding 0x40000000 to the angle before lookup converts sin to cos
//   - The angle is shifted right by 20 bits to get the table index
//
// The resulting matrix is:
//   [  cos(a)*cos(b)   -sin(a)*cos(b)    sin(b) ]
//   [       sin(a)           cos(a)         0   ]
//   [ -cos(a)*sin(b)    sin(a)*sin(b)    cos(b) ]
//
// =============================================================================

// Helper: Convert angle to table index
// The original shifts the 32-bit angle right by 22 bits and masks to 10 bits (0-1023)
static inline int angleToIndex(int32_t angle) {
    // The angle is a 32-bit value where the full range represents 0-2π
    // We need to convert to a 0-1023 index
    // Original does: (angle >> 20) & 0x3FC (which is 0-1023 * 4 for word alignment)
    // We just need the index 0-1023
    return (angle >> 22) & 0x3FF;
}

// Helper: Convert sin table value to Fixed format
// Sin table uses 31-bit fractional (0x7FFFFFFF = 1.0)
// Fixed uses 24-bit fractional (0x01000000 = 1.0)
// Need to shift right by 7 bits to convert
static inline Fixed sinToFixed(int32_t sinValue) {
    return Fixed::fromRaw(sinValue >> 7);
}

// Helper: Multiply two sine/cosine table values and return as Fixed
// The table values are in the range -0x7FFFFFFF to +0x7FFFFFFF
// The result needs to be converted to Fixed 8.24 format
static inline Fixed multiplySinCos(int32_t a, int32_t b) {
    // The original uses a shift-and-add algorithm
    // For simplicity, we use 64-bit multiplication
    // Table values: [-1, 1] scaled by 0x7FFFFFFF (31 fractional bits)
    // Product: 62 fractional bits total, shift right by 31 to get 31-bit result
    // Then shift right by 7 more to convert to Fixed 24-bit fractional
    int64_t result = (static_cast<int64_t>(a) * b) >> 38;  // 31 + 7 = 38
    return Fixed::fromRaw(static_cast<int32_t>(result));
}

Mat3x3 calculateRotationMatrix(int32_t angleA, int32_t angleB) {
    // Get sin and cos values from the lookup table
    int indexA = angleToIndex(angleA);
    int indexB = angleToIndex(angleB);

    int32_t sinA = getSin(indexA);
    int32_t cosA = getCos(indexA);
    int32_t sinB = getSin(indexB);
    int32_t cosB = getCos(indexB);

    // Calculate the matrix elements
    // Row 0: [cos(a)*cos(b), -sin(a)*cos(b), sin(b)]
    Fixed xNoseV = multiplySinCos(cosA, cosB);
    Fixed xRoofV = -multiplySinCos(sinA, cosB);
    Fixed xSideV = sinToFixed(sinB);

    // Row 1: [sin(a), cos(a), 0]
    Fixed yNoseV = sinToFixed(sinA);
    Fixed yRoofV = sinToFixed(cosA);
    Fixed ySideV = Fixed::fromRaw(0);

    // Row 2: [-cos(a)*sin(b), sin(a)*sin(b), cos(b)]
    Fixed zNoseV = -multiplySinCos(cosA, sinB);
    Fixed zRoofV = multiplySinCos(sinA, sinB);
    Fixed zSideV = sinToFixed(cosB);

    // Build the matrix (column-major order)
    return Mat3x3(
        Vec3(xNoseV, yNoseV, zNoseV),  // Nose vector (column 0)
        Vec3(xRoofV, yRoofV, zRoofV),  // Roof vector (column 1)
        Vec3(xSideV, ySideV, zSideV)   // Side vector (column 2)
    );
}

// =============================================================================
// Fixed-point multiplication utility
// =============================================================================

Fixed multiplyFixed(Fixed a, Fixed b) {
    // This is already implemented in the Fixed class
    return a * b;
}
