#ifndef LANDER_PROJECTION_H
#define LANDER_PROJECTION_H

#include "fixed.h"
#include "math3d.h"
#include "screen.h"

// =============================================================================
// 3D Projection
// =============================================================================
//
// Projects 3D world coordinates onto 2D screen coordinates using perspective
// projection. Based on the original Lander projection algorithm:
//
//   screen_x = 160 + x / z
//   screen_y = 64 + y / z
//
// The original uses (160, 64) as the center point for 320x256 resolution.
// We scale this to (640, 256) for our 1280x1024 resolution, with a 4x scale
// factor applied to the projection result.
//
// The original also clips vertices that are:
// - Behind the camera (z <= 0)
// - Beyond maximum distance (z >= 0x80000000)
// - At viewing angles greater than 45 degrees (|x| > z or |y| > z)
//
// =============================================================================

// Projection result containing screen coordinates and visibility flags
struct ProjectedVertex {
    int screenX;        // Screen X coordinate in physical pixels
    int screenY;        // Screen Y coordinate in physical pixels
    bool visible;       // True if the vertex is in front of camera
    bool onScreen;      // True if the projected point is within screen bounds
};

// Projection constants matching original Lander
namespace ProjectionConstants {
    // Original screen center (320x256 logical)
    // Note: The original uses (160, 64) because the view looks down at the
    // landscape with the horizon near the top of the play area
    constexpr int ORIGINAL_CENTER_X = 160;
    constexpr int ORIGINAL_CENTER_Y = 64;

    // Scaled screen center - computed at runtime based on current scale
    inline int CENTER_X() { return ORIGINAL_CENTER_X * ScreenBuffer::PIXEL_SCALE(); }
    inline int CENTER_Y() { return ORIGINAL_CENTER_Y * ScreenBuffer::PIXEL_SCALE(); }

    // Scale factor for projection (matches original's effective focal length)
    inline int SCALE() { return ScreenBuffer::PIXEL_SCALE(); }

    // Screen bounds for on-screen check (physical pixels)
    constexpr int SCREEN_LEFT = 0;
    constexpr int SCREEN_TOP = 0;
    inline int SCREEN_RIGHT() { return ScreenBuffer::PHYSICAL_WIDTH() - 1; }
    inline int SCREEN_BOTTOM() { return ScreenBuffer::PHYSICAL_HEIGHT() - 1; }
}

// =============================================================================
// Projection Functions
// =============================================================================

// Project a 3D point onto the screen
// Input: Camera-relative coordinates (x, y, z) in 8.24 fixed-point where:
//   - x is left/right (positive = right)
//   - y is up/down (positive = down, matching original's inverted Y)
//   - z is depth (positive = into screen, towards horizon)
// Returns: Projected screen coordinates and visibility flags
ProjectedVertex projectVertex(Fixed x, Fixed y, Fixed z);

// Project a Vec3 onto the screen (convenience wrapper)
ProjectedVertex projectVertex(const Vec3& v);

#endif // LANDER_PROJECTION_H
