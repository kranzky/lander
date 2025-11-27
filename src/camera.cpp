#include "camera.h"

// =============================================================================
// Camera Implementation
// =============================================================================

Camera::Camera()
    : position()
    , xTile()
    , yTile()
    , zTile()
{
}

void Camera::followTarget(const Vec3& targetPosition, bool clampHeight) {
    // Camera follows target with fixed offset
    // Based on original Lander.arm lines 2064-2087

    // X: follow target directly
    position.x = targetPosition.x;

    // Y: optionally clamp to not rise above 0 (prevents camera from rising with ship)
    // In original coordinate system, lower Y = higher altitude
    if (clampHeight && targetPosition.y.raw < 0) {
        position.y = CameraConstants::MAX_CAMERA_Y;
    } else {
        position.y = targetPosition.y;
    }

    // Z: offset behind target by CAMERA_PLAYER_Z (5 tiles)
    // "Behind" means lower Z value (camera looks toward positive Z)
    position.z = targetPosition.z - CameraConstants::CAMERA_PLAYER_Z;

    // Clamp camera Z to stay negative (prevents projection issues when Z >= 0)
    // The projection math breaks down when camera Z approaches or exceeds 0
    // because landscape tiles end up at or behind the camera
    if (position.z.raw >= 0) {
        position.z = Fixed::fromRaw(-1);  // Just below 0
    }

    // Update tile-snapped positions
    updateTilePositions();
}

void Camera::setPosition(const Vec3& pos) {
    position = pos;
    updateTilePositions();
}

void Camera::setPosition(Fixed x, Fixed y, Fixed z) {
    position.x = x;
    position.y = y;
    position.z = z;
    updateTilePositions();
}

void Camera::updateTilePositions() {
    // Snap to tile boundary by masking off lower 24 bits
    // This keeps only the integer part (upper 8 bits of 8.24 format)
    // From original Lander.arm lines 786-816

    constexpr int32_t TILE_MASK = 0xFF000000;

    xTile = Fixed::fromRaw(position.x.raw & TILE_MASK);
    yTile = Fixed::fromRaw(position.y.raw & TILE_MASK);
    zTile = Fixed::fromRaw(position.z.raw & TILE_MASK);
}

Vec3 Camera::worldToCamera(const Vec3& worldPos) const {
    return worldPos - position;
}

void Camera::worldToCamera(Fixed worldX, Fixed worldY, Fixed worldZ,
                           Fixed& outX, Fixed& outY, Fixed& outZ) const {
    outX = worldX - position.x;
    outY = worldY - position.y;
    outZ = worldZ - position.z;
}
