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

    // The sine table values are scaled so 0x7FFFFFFF = 1.0
    // We need to convert to our 8.24 fixed-point format
    // In the original: result = sum >> 8 (divide by 256)
    // But sine values are in a different scale, so we need to convert:
    // sin_value / 0x7FFFFFFF gives us the sine in range [-1, 1]
    // Multiply by TILE_SIZE to get the altitude variation
    //
    // Original algorithm: altitude = LAND_MID_HEIGHT - (sum / 256)
    // where sum is already in an appropriate scale.
    //
    // The sine table has values scaled to 0x7FFFFFFF = 1.0
    // We want altitude variation of a few tiles, so:
    // Convert from sine scale to fixed-point: value * TILE_SIZE / 0x7FFFFFFF
    // Then divide by 256 as in original

    // Scale: (sum * TILE_SIZE) / (0x7FFFFFFF * 256)
    // = sum / (0x7FFFFFFF * 256 / TILE_SIZE)
    // = sum / (0x7FFFFFFF * 256 / 0x01000000)
    // = sum / (0x7FFFFFFF / 0x40000)  [256 / TILE_SIZE = 256 / 2^24 = 1/2^18 = 1/0x40000]
    // Actually: 256 / 0x01000000 = 256 / 16777216 = 1/65536 = 1/0x10000
    // So divisor = 0x7FFFFFFF / 0x10000 = 0x7FFF
    // Actually let's recalculate:
    // We have sum in sine-scale (max ~10 * 0x7FFFFFFF)
    // Original divides by 256 to get altitude in game units
    // Original sine table also uses 0x7FFFFFFF scale
    // So: altitude_contribution = sum / 256
    // But we need to convert this to our 8.24 format
    // In original, TILE_SIZE = 0x01000000 and altitude is in same units
    // So the division by 256 on sine values (scaled to 0x7FFFFFFF)
    // gives us: sum / 256 in range of about [-40 * 0x7FFFFFFF / 256, +40 * ...]
    //         = about [-0x13000000, +0x13000000] which is about [-19, +19] tiles
    //
    // Actually the original likely operates in the same 8.24 format
    // Let's assume: sum (sine scale) / 256 should give altitude offset in 8.24
    // We need: (sum / 0x7FFFFFFF) * TILE_SIZE / 256
    //        = sum * TILE_SIZE / (0x7FFFFFFF * 256)
    //        = sum * 0x01000000 / (0x7FFFFFFF * 256)
    //        = sum / (0x7FFFFFFF * 256 / 0x01000000)
    //        = sum / (0x7FFFFFFF >> 16)  approximately
    //        = sum / 0x7FFF (approximately)
    //
    // Let's use: sum >> 23 which is dividing by 0x800000
    // This gives altitude variation of about sum * TILE_SIZE / (0x7FFFFFFF * 8)
    // With max sum of 10 * 0x7FFFFFFF, result is about 1.25 * TILE_SIZE
    // That seems too small...
    //
    // Looking at the original more carefully:
    // The sin values from the table are in [-0x7FFFFFFF, +0x7FFFFFFF]
    // Dividing by 256 (>> 8) gives [-0x7FFFFF, +0x7FFFFF]
    // That's about ±0.5 in 8.24 format
    // With 10 total coefficient weight, that's ±5 tiles of variation
    // That seems reasonable for terrain

    // So the correct scaling is: sum >> 8 to get to 8.24 format
    // Actually we need: (sum >> 8) and then convert from sine scale to 8.24
    // (sum >> 8) is in range [-10 * 0x7FFFFF, +10 * 0x7FFFFF]
    // = [-0x4FFFFF6, +0x4FFFFF6] approximately
    // But that's already close to 8.24 format values!
    //
    // Wait, let's check: 0x7FFFFFFF >> 8 = 0x7FFFFF = about 0.5 in 8.24 format
    // Because 0x01000000 = 1.0, so 0x7FFFFF ≈ 0.5
    // So altitude offset = sum >> 8, in range [-5, +5] tiles with full coefficients
    //
    // Hmm, but 0x7FFFFF / 0x01000000 = 0.499999... yes approximately 0.5

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
