#include "projection.h"
#include <algorithm>

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
    ProjectedVertex result{};

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

    // Perspective division, scaled by the focal length (64-bit to avoid overflow)
    using ProjectionConstants::FOCAL_LENGTH;
    int64_t scaledX = static_cast<int64_t>(x.raw) * FOCAL_LENGTH;
    int64_t scaledY = static_cast<int64_t>(y.raw) * FOCAL_LENGTH;

    // Divide by z (in raw format) to get logical pixels. Clamp so vertices very
    // close to the camera can't overflow int once scaled; the rasterizer clips
    // anything that far off screen anyway.
    constexpr int64_t MAX_OFFSET = 1 << 20;
    int offsetX = static_cast<int>(std::clamp<int64_t>(scaledX / z.raw, -MAX_OFFSET, MAX_OFFSET));
    int offsetY = static_cast<int>(std::clamp<int64_t>(scaledY / z.raw, -MAX_OFFSET, MAX_OFFSET));

    // Apply resolution scale and center offset
    result.screenX = ProjectionConstants::CENTER_X() + (offsetX * ProjectionConstants::SCALE());
    result.screenY = ProjectionConstants::CENTER_Y() + (offsetY * ProjectionConstants::SCALE());

    // Check if on screen
    result.onScreen = (result.screenX >= ProjectionConstants::SCREEN_LEFT &&
                       result.screenX <= ProjectionConstants::SCREEN_RIGHT() &&
                       result.screenY >= ProjectionConstants::SCREEN_TOP &&
                       result.screenY <= ProjectionConstants::SCREEN_BOTTOM());

    return result;
}

ProjectedVertex projectVertex(const Vec3& v) {
    return projectVertex(v.x, v.y, v.z);
}
