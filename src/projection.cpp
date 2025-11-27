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
    // The result is in 8.24 fixed-point format
    //
    // In the original Lander, the projection is: screen = center + (x/z) * scale
    // where the scale factor relates world units to screen pixels.
    //
    // With TILE_SIZE = 0x01000000 (1.0 in 8.24), a typical scene has:
    // - x offset of ~6 tiles from center (half of 12 tile width)
    // - z distance of ~10 tiles
    // - This gives x/z = 0.6 tiles
    //
    // To fill the screen (~160 pixels from center), we need:
    // - 0.6 tiles -> ~100 pixels
    // - So scale factor is about 160 pixels per tile
    //
    // In 8.24 format, division result for 6/10 = 0x00999999
    // We want this to become ~100 pixels
    // The integer part of 0x00999999 >> 24 = 0, so we need fractional scaling
    //
    // Solution: scale the numerator before dividing, or scale result after
    // We'll use a focal length multiplier that effectively scales x and y
    // before the division.
    //
    // Original uses approximately 256 (or 0x100) as focal length
    // So: screen_x = center + (x * 256) / z = center + x/z * 256

    // Focal length: how many pixels per unit distance at z=1
    // A value of 256 means at z=1 tile, x=1 tile maps to 256 pixels
    // This gives a reasonable field of view
    constexpr int FOCAL_LENGTH = 256;

    // Scale x and y by focal length before projection
    // Use 64-bit to avoid overflow
    int64_t scaledX = static_cast<int64_t>(x.raw) * FOCAL_LENGTH;
    int64_t scaledY = static_cast<int64_t>(y.raw) * FOCAL_LENGTH;

    // Divide by z (in raw format)
    // Result is in pixels (no longer in 8.24 format)
    int offsetX = static_cast<int>(scaledX / z.raw);
    int offsetY = static_cast<int>(scaledY / z.raw);

    // Apply our 4x resolution scale and center offset
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
