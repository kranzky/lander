#ifndef LANDER_SCREEN_H
#define LANDER_SCREEN_H

#include <cstdint>
#include <cstring>
#include "constants.h"
#include "font.h"

// =============================================================================
// Screen Buffer
// =============================================================================
//
// This module provides a software framebuffer that renders at high resolution
// (1280x1024) while preserving the original Lander's coordinate system
// (320x256) for game logic.
//
// Logical coordinates (used by game logic) are scaled by 4x when rendering
// to physical coordinates. This allows smooth rendering of lines and shapes
// rather than chunky pixel blocks.
//
// The original game uses a 256-color palette (8-bit indexed color), but
// we store RGBA for simplicity and SDL compatibility.
//
// =============================================================================

struct Color {
    uint8_t r, g, b, a;

    constexpr Color() : r(0), g(0), b(0), a(255) {}
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    // Predefined colors
    static constexpr Color black()   { return Color(0, 0, 0); }
    static constexpr Color white()   { return Color(255, 255, 255); }
    static constexpr Color red()     { return Color(255, 0, 0); }
    static constexpr Color green()   { return Color(0, 255, 0); }
    static constexpr Color blue()    { return Color(0, 0, 255); }
    static constexpr Color yellow()  { return Color(255, 255, 0); }
    static constexpr Color cyan()    { return Color(0, 255, 255); }
    static constexpr Color magenta() { return Color(255, 0, 255); }
};

class ScreenBuffer {
public:
    // Logical dimensions (original game coordinates) - always fixed
    static constexpr int LOGICAL_WIDTH = ORIGINAL_WIDTH;    // 320
    static constexpr int LOGICAL_HEIGHT = ORIGINAL_HEIGHT;  // 256

    // Maximum physical dimensions (buffer is always allocated at max size)
    static constexpr int MAX_PHYSICAL_WIDTH = SCREEN_WIDTH;     // 1280
    static constexpr int MAX_PHYSICAL_HEIGHT = SCREEN_HEIGHT;   // 1024

    // For backward compatibility - these now return current values
    static int PHYSICAL_WIDTH() { return DisplayConfig::getPhysicalWidth(); }
    static int PHYSICAL_HEIGHT() { return DisplayConfig::getPhysicalHeight(); }
    static int PIXEL_SCALE() { return DisplayConfig::scale; }

    ScreenBuffer();
    ~ScreenBuffer();

    // Prevent copying (large buffer)
    ScreenBuffer(const ScreenBuffer&) = delete;
    ScreenBuffer& operator=(const ScreenBuffer&) = delete;

    // Clear the entire buffer to a color
    void clear(Color color = Color::black());

    // Plot a pixel at logical coordinates (scaled to physical)
    // Coordinates are in original game space (0-319, 0-255)
    // Plots a single physical pixel at the scaled position
    void plotPixel(int x, int y, Color color);

    // Plot a pixel directly at physical coordinates (0-1279, 0-1023)
    // Use this for smooth rendering of primitives
    void plotPhysicalPixel(int px, int py, Color color);

    // Draw a horizontal line at physical coordinates
    // x1 and x2 are inclusive endpoints, automatically clipped to screen
    void drawHorizontalLine(int x1, int x2, int y, Color color);

    // Draw a filled triangle at physical coordinates
    // Uses scanline rasterization matching the original Lander algorithm
    void drawTriangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color);

    // Get pixel at physical coordinates (for testing)
    Color getPhysicalPixel(int px, int py) const;

    // Check if logical coordinates are in bounds
    bool inBounds(int x, int y) const {
        return x >= 0 && x < LOGICAL_WIDTH && y >= 0 && y < LOGICAL_HEIGHT;
    }

    // Check if physical coordinates are in bounds (uses current resolution)
    bool inPhysicalBounds(int px, int py) const {
        return px >= 0 && px < PHYSICAL_WIDTH() && py >= 0 && py < PHYSICAL_HEIGHT();
    }

    // Convert logical to physical coordinates (uses current scale)
    static int toPhysicalX(int x) { return x * PIXEL_SCALE(); }
    static int toPhysicalY(int y) { return y * PIXEL_SCALE(); }

    // Direct access to physical buffer (for SDL texture updates)
    const uint8_t* getData() const { return buffer; }
    uint8_t* getData() { return buffer; }

    // Buffer size in bytes (RGBA = 4 bytes per pixel) - always max size
    static constexpr size_t getBufferSize() {
        return MAX_PHYSICAL_WIDTH * MAX_PHYSICAL_HEIGHT * 4;
    }

    // Current buffer size for current resolution
    static size_t getCurrentBufferSize() {
        return PHYSICAL_WIDTH() * PHYSICAL_HEIGHT() * 4;
    }

    // Physical buffer pitch (bytes per row) - always max width for consistent layout
    static constexpr int getPitch() {
        return MAX_PHYSICAL_WIDTH * 4;
    }

    // Current pitch for current resolution
    static int getCurrentPitch() {
        return PHYSICAL_WIDTH() * 4;
    }

    // Save buffer to PNG file
    bool savePNG(const char* filename) const;

    // Draw a single character at logical coordinates using BBC Micro font
    // Returns the width of the character drawn (for text positioning)
    // Scale multiplies the 8x8 font size (scale=1 gives 8x8 pixels in logical coords)
    int drawChar(int x, int y, char c, Color color, int scale = 1);

    // Draw a string at logical coordinates
    // Returns the x position after the last character
    int drawText(int x, int y, const char* text, Color color, int scale = 1);

    // Draw an integer as text at logical coordinates
    // Returns the x position after the last digit
    int drawInt(int x, int y, int value, Color color, int scale = 1);

private:
    // Convert physical coordinates to buffer offset (uses max width for stride)
    static size_t physicalToOffset(int px, int py) {
        return (py * MAX_PHYSICAL_WIDTH + px) * 4;
    }

    // RGBA buffer (always allocated at max physical resolution)
    uint8_t* buffer;
};

#endif // LANDER_SCREEN_H
