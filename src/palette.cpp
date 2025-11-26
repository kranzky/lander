#include "palette.h"
#include <algorithm>

// =============================================================================
// VIDC Color Conversion
// =============================================================================
//
// The Archimedes VIDC encodes 4-bit RGB channels into an 8-bit palette index
// using a scrambled bit layout. This allows 256 colors from 4096 possible RGB
// combinations (4 bits per channel).
//
// VIDC byte layout:
//   Bit 7 = B3 (blue bit 3)
//   Bit 6 = G3 (green bit 3)
//   Bit 5 = G2 (green bit 2)
//   Bit 4 = R3 (red bit 3)
//   Bit 3 = B2 (blue bit 2)
//   Bit 2 = R2 (red bit 2)
//   Bit 1 = tint bit 1 (shared)
//   Bit 0 = tint bit 0 (shared)
//
// The tint bits affect all channels equally, providing 4 brightness levels
// for each unique hue combination.
//
// =============================================================================

Color vidc256ToColor(uint8_t vidc) {
    // Extract the scrambled bits
    int tint = vidc & 0x03;           // Bits 0-1: shared tint
    int r2 = (vidc >> 2) & 1;         // Bit 2: red bit 2
    int b2 = (vidc >> 3) & 1;         // Bit 3: blue bit 2
    int r3 = (vidc >> 4) & 1;         // Bit 4: red bit 3
    int g2 = (vidc >> 5) & 1;         // Bit 5: green bit 2
    int g3 = (vidc >> 6) & 1;         // Bit 6: green bit 3
    int b3 = (vidc >> 7) & 1;         // Bit 7: blue bit 3

    // Reconstruct 4-bit channels
    // The tint provides bits 0-1, the extracted bits provide bits 2-3
    int red   = tint | (r2 << 2) | (r3 << 3);
    int green = tint | (g2 << 2) | (g3 << 3);
    int blue  = tint | (b2 << 2) | (b3 << 3);

    // Scale from 4-bit (0-15) to 8-bit (0-255)
    // Multiply by 17 to expand: 0->0, 1->17, 15->255
    return Color(
        static_cast<uint8_t>(red * 17),
        static_cast<uint8_t>(green * 17),
        static_cast<uint8_t>(blue * 17)
    );
}

uint8_t buildVidcColor(int red, int green, int blue) {
    // Clamp to 4 bits
    red = std::clamp(red, 0, 15);
    green = std::clamp(green, 0, 15);
    blue = std::clamp(blue, 0, 15);

    // Build the VIDC byte using the original algorithm from Lander.arm
    // This matches GetLandscapeTileColour exactly
    uint8_t result = 0;

    // Bits 0-1: OR of the low bits from all channels
    // Original: ORR R8, R1, R2; AND R8, #3; ORR R8, R0; AND R8, #7
    result = ((green | blue) & 0x03) | red;
    result &= 0x07;

    // Bit 4: red bit 3
    if (red & 0x08) result |= 0x10;

    // Bits 5-6: green bits 2-3 shifted left by 3
    result |= (green & 0x0C) << 3;

    // Bit 3: blue bit 2
    if (blue & 0x04) result |= 0x08;

    // Bit 7: blue bit 3
    if (blue & 0x08) result |= 0x80;

    return result;
}

// =============================================================================
// Landscape Colors
// =============================================================================

Color getLandscapeTileColor(int32_t altitude, int tileRow, int32_t slope, TileType type) {
    int red, green, blue;

    // Base color calculation from altitude bits (matches original)
    // Green from bit 3 of altitude: 4 if clear, 8 if set
    green = ((altitude & 0x08) >> 1) + 4;  // (bit3 * 4) + 4

    // Red from bit 2 of altitude
    red = altitude & 0x04;

    // Blue is zero for land
    blue = 0;

    // Special cases
    if (type == TileType::Launchpad) {
        // Grey: all channels equal
        red = 4;
        green = 4;
        blue = 4;
    } else if (type == TileType::Sea) {
        // Blue: only blue channel
        red = 0;
        green = 0;
        blue = 4;
    }

    // Calculate brightness from row and slope
    // tileRow: 1 (far) to 10 (near)
    // slope: altitude difference >> 22 (adds to brightness for left-facing tiles)
    int brightness = tileRow + (slope >> 22);

    // Add brightness to all channels
    red += brightness;
    green += brightness;
    blue += brightness;

    // Clamp to 4-bit range
    red = std::min(red, 15);
    green = std::min(green, 15);
    blue = std::min(blue, 15);

    // Build VIDC color and convert to RGB
    uint8_t vidc = buildVidcColor(red, green, blue);
    return vidc256ToColor(vidc);
}

// =============================================================================
// Object Colors
// =============================================================================

Color objectColorToRGB(uint16_t objectColor, int brightness) {
    // Object colors are stored as &RGB (12-bit, 4 bits per channel)
    int red   = (objectColor >> 8) & 0x0F;
    int green = (objectColor >> 4) & 0x0F;
    int blue  = objectColor & 0x0F;

    // Add brightness (clamped)
    red = std::min(red + brightness, 15);
    green = std::min(green + brightness, 15);
    blue = std::min(blue + brightness, 15);

    // Build VIDC and convert to RGB
    uint8_t vidc = buildVidcColor(red, green, blue);
    return vidc256ToColor(vidc);
}

// =============================================================================
// Game Colors
// =============================================================================

namespace GameColors {

Color fuelBar() {
    // &37 from original
    return vidc256ToColor(0x37);
}

Color white() {
    return vidc256ToColor(0xFF);
}

Color smokeGrey(int level) {
    // Grey colors have equal R, G, B
    level = std::clamp(level, 0, 15);
    return vidc256ToColor(buildVidcColor(level, level, level));
}

Color fromVidc(uint8_t vidc) {
    return vidc256ToColor(vidc);
}

} // namespace GameColors
