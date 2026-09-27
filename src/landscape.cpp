// landscape.cpp
// Landscape altitude generation using Fourier synthesis
// Port of the original Lander terrain generation algorithm

#include "landscape.h"
#include "lookup_tables.h"

using namespace GameConstants;

// =============================================================================
// Fourier Synthesis Terrain Generation
// =============================================================================
//
// The original Lander generates terrain procedurally using Fourier synthesis.
// Six sine waves of different frequencies and phases are combined to create
// interesting, varied terrain. The formula is:
//
//   altitude = LAND_MID_HEIGHT - (sum of 6 sine terms) / 256
//
// Where the 6 sine terms are:
//   2 * sin(x - 2z)      - Primary wave
//   2 * sin(4x + 3z)     - Cross-pattern wave
//   2 * sin(3z - 5x)     - Diagonal wave
//   2 * sin(3x + 3z)     - Diagonal wave (other direction)
//   1 * sin(5x + 11z)    - High frequency detail
//   1 * sin(10x + 7z)    - High frequency detail
//
// The coordinates x,z are in fixed-point format. The upper 10 bits of each
// coordinate (after multiplication by frequency) are used to index the
// 1024-entry sine table.
//
// =============================================================================

// Helper: extract sine table index from a coordinate expression
// The coordinate is in 8.24 fixed-point. To get the 10-bit table index,
// we use bits 22-31 (the top 10 bits), wrapping around naturally.
static inline int toSineIndex(int32_t value) {
    // Right shift by 22 to get the top 10 bits as the index
    // The mask is handled by getSin() with & (SIN_TABLE_SIZE - 1)
    return static_cast<int>(value >> 22);
}

Fixed getLandscapeAltitude(Fixed x, Fixed z) {
    // Check for launchpad area - flat area at the origin for takeoff/landing
    // Both x and z must be less than LAUNCHPAD_SIZE (8 tiles)
    if (x < LAUNCHPAD_SIZE && z < LAUNCHPAD_SIZE &&
        x.raw >= 0 && z.raw >= 0) {
        return LAUNCHPAD_ALTITUDE;
    }

    // Get raw coordinate values for Fourier synthesis
    int32_t xr = x.raw;
    int32_t zr = z.raw;

    // Compute the 6 sine terms
    // Each term: coefficient * sin(freq_x * x + freq_z * z)
    // The multiplication by frequency is done on raw values before indexing

    // Term 1: 2 * sin(x - 2z)
    int32_t angle1 = xr - 2 * zr;
    int64_t term1 = 2LL * getSin(toSineIndex(angle1));

    // Term 2: 2 * sin(4x + 3z)
    int32_t angle2 = 4 * xr + 3 * zr;
    int64_t term2 = 2LL * getSin(toSineIndex(angle2));

    // Term 3: 2 * sin(3z - 5x)
    int32_t angle3 = 3 * zr - 5 * xr;
    int64_t term3 = 2LL * getSin(toSineIndex(angle3));

    // Term 4: 2 * sin(3x + 3z)
    int32_t angle4 = 3 * xr + 3 * zr;
    int64_t term4 = 2LL * getSin(toSineIndex(angle4));

    // Term 5: 1 * sin(5x + 11z)
    int32_t angle5 = 5 * xr + 11 * zr;
    int64_t term5 = getSin(toSineIndex(angle5));

    // Term 6: 1 * sin(10x + 7z)
    int32_t angle6 = 10 * xr + 7 * zr;
    int64_t term6 = getSin(toSineIndex(angle6));

    // Sum all terms
    // Total coefficient sum: 2+2+2+2+1+1 = 10
    // Each sin value is in range [-0x7FFFFFFF, +0x7FFFFFFF]
    // So sum is in range [-10 * 0x7FFFFFFF, +10 * 0x7FFFFFFF]
    int64_t sum = term1 + term2 + term3 + term4 + term5 + term6;

    // As in the original, divide the sum by 256. Sine values of ±0x7FFFFFFF
    // become ±0x7FFFFF (about half a tile in 8.24), so with the total
    // coefficient weight of 10 the terrain varies by up to ±5 tiles.
    int32_t altitudeOffset = static_cast<int32_t>(sum >> 8);

    // Calculate final altitude
    Fixed altitude = Fixed::fromRaw(LAND_MID_HEIGHT.raw - altitudeOffset);

    // Clamp to sea level (lower altitude values = higher physical position)
    // If altitude > SEA_LEVEL, we're below sea level, so clamp
    if (altitude > SEA_LEVEL) {
        altitude = SEA_LEVEL;
    }

    return altitude;
}

Fixed getLandscapeAltitudeAtTile(int tileX, int tileZ) {
    // Convert tile coordinates to world coordinates
    // Each tile is TILE_SIZE units
    Fixed x = Fixed::fromRaw(tileX * TILE_SIZE.raw);
    Fixed z = Fixed::fromRaw(tileZ * TILE_SIZE.raw);

    return getLandscapeAltitude(x, z);
}
