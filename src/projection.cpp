#include "projection.h"

// =============================================================================
// 3D Projection Implementation
// =============================================================================
//
// This implements perspective projection matching the original Lander.
//
// The original ProjectVertexOntoScreen routine (Lander.arm lines 7161-7370):
// 1. Checks if z >= 0x80000000 (too far/behind) - returns not visible
// 2. Uses dynamic scaling to maximize precision for division
// 3. Performs x/z and y/z using shift-and-subtract division
// 4. Returns screen coordinates: (160 + x/z, 64 + y/z)
//
// Our implementation uses 64-bit intermediates in Fixed division, which
// gives us more than enough precision without the dynamic scaling trick.
//
// =============================================================================

ProjectedVertex projectVertex(Fixed x, Fixed y, Fixed z) {
    ProjectedVertex result;
    result.screenX = 0;
    result.screenY = 0;
    result.visible = false;
    result.onScreen = false;

    // Check if vertex is behind camera or at camera (z <= 0)
    // The original checks if z + 0x80000000 produces a carry, which means
    // z >= 0x80000000 (too far) or z is negative (behind camera)
    if (z.raw <= 0) {
        return result;
    }

    // The original also rejects vertices that are too far away
    // In practice, this is handled by the sign check above for our purposes

    // Vertex is in front of camera
    result.visible = true;

    // Calculate perspective division: x/z and y/z
    // The result is in 8.24 fixed-point, representing pixel offset
    // A result of 1.0 (0x01000000) means 1 pixel offset in original coords
    Fixed xDivZ = x / z;
    Fixed yDivZ = y / z;

    // Convert to screen coordinates
    // Original: screen_x = 160 + x/z, screen_y = 64 + y/z
    // We scale by 4x for our higher resolution
    // The division result's integer part gives us the pixel offset
    int offsetX = xDivZ.toInt();
    int offsetY = yDivZ.toInt();

    // Apply scale and center offset
    result.screenX = ProjectionConstants::CENTER_X + (offsetX * ProjectionConstants::SCALE);
    result.screenY = ProjectionConstants::CENTER_Y + (offsetY * ProjectionConstants::SCALE);

    // Check if on screen
    result.onScreen = (result.screenX >= ProjectionConstants::SCREEN_LEFT &&
                       result.screenX <= ProjectionConstants::SCREEN_RIGHT &&
                       result.screenY >= ProjectionConstants::SCREEN_TOP &&
                       result.screenY <= ProjectionConstants::SCREEN_BOTTOM);

    return result;
}

ProjectedVertex projectVertex(const Vec3& v) {
    return projectVertex(v.x, v.y, v.z);
}
