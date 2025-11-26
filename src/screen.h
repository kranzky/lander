#ifndef LANDER_SCREEN_H
#define LANDER_SCREEN_H

#include <cstdint>
#include <cstring>
#include "constants.h"

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
    // Logical dimensions (original game coordinates)
    static constexpr int LOGICAL_WIDTH = ORIGINAL_WIDTH;    // 320
    static constexpr int LOGICAL_HEIGHT = ORIGINAL_HEIGHT;  // 256

    // Physical dimensions (actual framebuffer)
    static constexpr int PHYSICAL_WIDTH = SCREEN_WIDTH;     // 1280
    static constexpr int PHYSICAL_HEIGHT = SCREEN_HEIGHT;   // 1024

    // Scale factor
    static constexpr int PIXEL_SCALE = SCALE;               // 4

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

    // Get pixel at physical coordinates (for testing)
    Color getPhysicalPixel(int px, int py) const;

    // Check if logical coordinates are in bounds
    bool inBounds(int x, int y) const {
        return x >= 0 && x < LOGICAL_WIDTH && y >= 0 && y < LOGICAL_HEIGHT;
    }

    // Check if physical coordinates are in bounds
    bool inPhysicalBounds(int px, int py) const {
        return px >= 0 && px < PHYSICAL_WIDTH && py >= 0 && py < PHYSICAL_HEIGHT;
    }

    // Convert logical to physical coordinates
    static constexpr int toPhysicalX(int x) { return x * PIXEL_SCALE; }
    static constexpr int toPhysicalY(int y) { return y * PIXEL_SCALE; }

    // Direct access to physical buffer (for SDL texture updates)
    const uint8_t* getData() const { return buffer; }
    uint8_t* getData() { return buffer; }

    // Buffer size in bytes (RGBA = 4 bytes per pixel)
    static constexpr size_t getBufferSize() {
        return PHYSICAL_WIDTH * PHYSICAL_HEIGHT * 4;
    }

    // Physical buffer pitch (bytes per row)
    static constexpr int getPitch() {
        return PHYSICAL_WIDTH * 4;
    }

    // Save buffer to PNG file
    bool savePNG(const char* filename) const;

private:
    // Convert physical coordinates to buffer offset
    static constexpr size_t physicalToOffset(int px, int py) {
        return (py * PHYSICAL_WIDTH + px) * 4;
    }

    // RGBA buffer (physical resolution)
    uint8_t* buffer;
};

#endif // LANDER_SCREEN_H
