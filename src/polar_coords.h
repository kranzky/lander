// polar_coords.h
// Mouse polar coordinate conversion for ship control

#ifndef LANDER_POLAR_COORDS_H
#define LANDER_POLAR_COORDS_H

#include <cstdint>

// =============================================================================
// Mouse Polar Coordinate Conversion
// =============================================================================
//
// Based on GetMouseInPolarCoordinates from Lander.arm (lines 6568-6882)
//
// Converts mouse (x, y) coordinates into polar coordinates (angle, distance):
//
//           .|
//      R0 .´ |
//       .´   | y
//     .´R1   |
//   +´-------+
//        x
//
// Where R1 is the angle and R0 is the distance.
//
// The angle is used to control ship direction (rotation around Y axis)
// The distance is used to control ship pitch (tilt forward/backward)
//
// Algorithm:
// 1. Take absolute values of x and y
// 2. Divide smaller by larger to get ratio in range [0, 1]
// 3. Use arctan lookup table to get angle
// 4. Adjust angle based on quadrant (signs of original x and y)
// 5. Calculate distance using Pythagoras: sqrt(x² + y²)
//
// =============================================================================

// Result of polar coordinate conversion
struct PolarCoordinates {
    int32_t angle;      // Angle in scaled format (0x80000000 = 180 degrees)
    int32_t distance;   // Distance from origin (scaled)
};

// Convert mouse coordinates to polar coordinates
// Input: mouse coordinates in the range -512 to +511, shifted left by 22 bits
// (so -512 becomes 0x80000000, whose magnitude is treated as unsigned 2^31)
// Output: PolarCoordinates with angle and distance
//
// The angle uses the same format as the original:
//   - 0x00000000 = 0 degrees (pointing right/positive X)
//   - 0x40000000 = 90 degrees (pointing up/positive Y)
//   - 0x80000000 = 180 degrees (pointing left/negative X)
//   - 0xC0000000 = 270 degrees (pointing down/negative Y)
//
// The distance is sqrt(x^2 + y^2) / 2^32 with 0x7FFFFFFF = 1.0, so a point
// 512 units from the centre gives 0.5 (0x40000000), which is where the
// caller's full pitch is reached. At the centre the angle is the original's
// arctan(255/256), about 45 degrees, with a distance of zero.
PolarCoordinates getMouseInPolarCoordinates(int32_t x, int32_t y);

#endif // LANDER_POLAR_COORDS_H
