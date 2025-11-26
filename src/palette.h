#ifndef LANDER_PALETTE_H
#define LANDER_PALETTE_H

#include "screen.h"

// =============================================================================
// Color Palette
// =============================================================================
//
// The original Lander runs in Archimedes Mode 13, which uses 256 colors.
// Colors are encoded in a VIDC byte format where the RGB channels (4 bits each)
// are scrambled into an 8-bit value:
//
//   Bit 7 = blue bit 3
//   Bit 6 = green bit 3
//   Bit 5 = green bit 2
//   Bit 4 = red bit 3
//   Bit 3 = blue bit 2
//   Bit 2 = red bit 2
//   Bit 1 = shared low bit (OR of R1, G1, B1)
//   Bit 0 = shared low bit (OR of R0, G0, B0)
//
// We convert these to standard RGB for rendering.
//
// =============================================================================

// Convert VIDC palette index (0-255) to RGB Color
Color vidc256ToColor(uint8_t vidc);

// Build a VIDC color index from separate R, G, B channels (each 0-15)
// This matches the original's color building code
uint8_t buildVidcColor(int red, int green, int blue);

// =============================================================================
// Landscape Colors
// =============================================================================
//
// The landscape color depends on:
//   - Altitude (affects green/red tint)
//   - Distance (row number, 1=far, 10=near)
//   - Slope (left-facing = brighter)
//   - Special cases: launchpad (grey), sea (blue)
//
// =============================================================================

// Landscape tile types
enum class TileType {
    Land,       // Normal terrain
    Launchpad,  // Grey launchpad
    Sea         // Blue water
};

// Calculate landscape tile color
// - altitude: current tile altitude (fixed-point raw value, for bit extraction)
// - tileRow: distance row (1=far to 10=near)
// - slope: altitude difference for brightness (0 or positive)
// - type: tile type (land, launchpad, sea)
Color getLandscapeTileColor(int32_t altitude, int tileRow, int32_t slope, TileType type);

// =============================================================================
// Object Face Colors
// =============================================================================
//
// Object faces use 12-bit color values (&RGB format) that get converted
// to VIDC colors with additional brightness based on face normal.
//
// =============================================================================

// Convert a 12-bit object color (&RGB, 4 bits per channel) to Color
// brightness: additional brightness to add (0-15)
Color objectColorToRGB(uint16_t objectColor, int brightness = 0);

// =============================================================================
// Predefined Game Colors
// =============================================================================

namespace GameColors {
    // UI elements
    Color fuelBar();      // &37 = green-ish

    // Particle colors
    Color white();        // &FF = white
    Color smokeGrey(int level);  // Grey gradient for smoke (0=dark, 15=light)

    // Debug/test colors
    Color fromVidc(uint8_t vidc);  // Direct VIDC to RGB
}

#endif // LANDER_PALETTE_H
