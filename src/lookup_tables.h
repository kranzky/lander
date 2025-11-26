#ifndef LANDER_LOOKUP_TABLES_H
#define LANDER_LOOKUP_TABLES_H

#include <cstdint>

// =============================================================================
// Lookup Tables
// =============================================================================
//
// These lookup tables are copied directly from the original Lander.arm source.
// They use a 32-bit signed fixed-point format where 0x7FFFFFFF represents ~1.0.
//
// The original uses (2^31 - 1) = 0x7FFFFFFF as the maximum value.
//
// =============================================================================

// Number of entries in each table
constexpr int SIN_TABLE_SIZE = 1024;
constexpr int ARCTAN_TABLE_SIZE = 128;
constexpr int SQRT_TABLE_SIZE = 1024;

// The sine table: sin(2π * n / 1024) * 0x7FFFFFFF for n = 0..1023
// This covers a full circle (0 to 2π)
// Index 0 = 0°, Index 256 = 90°, Index 512 = 180°, Index 768 = 270°
extern const int32_t sinTable[SIN_TABLE_SIZE];

// The arctan table: (0x7FFFFFFF / π) * arctan(n / 128) for n = 0..127
// Used for converting mouse position to polar coordinates
extern const int32_t arctanTable[ARCTAN_TABLE_SIZE];

// The square root table: sqrt(n / 1024) * 0x7FFFFFFF for n = 0..1023
// Used for calculating distance from mouse position
extern const int32_t squareRootTable[SQRT_TABLE_SIZE];

// =============================================================================
// Access Functions
// =============================================================================

// Get sine value for angle (0-1023 maps to 0-2π)
// Returns value in range -0x7FFFFFFF to +0x7FFFFFFF
inline int32_t getSin(int angle) {
    return sinTable[angle & (SIN_TABLE_SIZE - 1)];
}

// Get cosine value for angle (0-1023 maps to 0-2π)
// cos(x) = sin(x + 90°) = sin(x + 256)
inline int32_t getCos(int angle) {
    return sinTable[(angle + 256) & (SIN_TABLE_SIZE - 1)];
}

// Get arctan value for index 0-127
// The original uses this with the ratio of mouse coordinates
inline int32_t getArctan(int index) {
    if (index < 0) index = 0;
    if (index >= ARCTAN_TABLE_SIZE) index = ARCTAN_TABLE_SIZE - 1;
    return arctanTable[index];
}

// Get square root value for index 0-1023
inline int32_t getSqrt(int index) {
    if (index < 0) index = 0;
    if (index >= SQRT_TABLE_SIZE) index = SQRT_TABLE_SIZE - 1;
    return squareRootTable[index];
}

#endif // LANDER_LOOKUP_TABLES_H
