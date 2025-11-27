#ifndef LANDER_CAMERA_H
#define LANDER_CAMERA_H

#include "fixed.h"
#include "math3d.h"
#include "constants.h"

// =============================================================================
// Camera System
// =============================================================================
//
// The camera follows the player with a fixed offset. Based on the original
// Lander camera system (Lander.arm lines 2064-2087):
//
//   xCamera = xPlayer              (follows horizontally)
//   yCamera = max(0, yPlayer)      (clamped - doesn't rise above 0)
//   zCamera = zPlayer + CAMERA_PLAYER_Z  (5 tiles behind player)
//
// The camera position is also snapped to tile coordinates for landscape
// rendering (xCameraTile, yCameraTile, zCameraTile).
//
// =============================================================================

class Camera {
public:
    Camera();

    // Update camera to follow a target position (typically player position)
    // Set clampHeight=false to allow unlimited height (for debugging)
    void followTarget(const Vec3& targetPosition, bool clampHeight = true);

    // Set camera position directly (for initialization or debugging)
    void setPosition(const Vec3& pos);
    void setPosition(Fixed x, Fixed y, Fixed z);

    // Get camera position
    const Vec3& getPosition() const { return position; }
    Fixed getX() const { return position.x; }
    Fixed getY() const { return position.y; }
    Fixed getZ() const { return position.z; }

    // Get tile-snapped camera position (for landscape rendering)
    Fixed getXTile() const { return xTile; }
    Fixed getYTile() const { return yTile; }
    Fixed getZTile() const { return zTile; }

    // Get fractional offset within current tile
    Fixed getXFraction() const { return position.x - xTile; }
    Fixed getYFraction() const { return position.y - yTile; }
    Fixed getZFraction() const { return position.z - zTile; }

    // Transform world coordinates to camera-relative coordinates
    Vec3 worldToCamera(const Vec3& worldPos) const;
    void worldToCamera(Fixed worldX, Fixed worldY, Fixed worldZ,
                       Fixed& outX, Fixed& outY, Fixed& outZ) const;

private:
    // Update tile-snapped positions from current position
    void updateTilePositions();

    Vec3 position;      // Camera position in world space

    // Tile-snapped camera position (for landscape rendering)
    // These are the position rounded down to the nearest tile
    Fixed xTile;
    Fixed yTile;
    Fixed zTile;
};

// =============================================================================
// Camera Constants
// =============================================================================

namespace CameraConstants {
    // Distance behind player along z-axis (5 tiles as in original)
    constexpr Fixed CAMERA_PLAYER_Z = Fixed::fromRaw(5 * GameConstants::TILE_SIZE.raw);

    // Height offset - camera is at same altitude as player (no Y offset)
    constexpr Fixed CAMERA_Y_OFFSET = Fixed::fromRaw(0);  // No offset

    // Maximum camera Y value (clamped to not rise above 0)
    // In the original coordinate system, lower Y = higher altitude
    constexpr Fixed MAX_CAMERA_Y = Fixed::fromInt(0);
}

#endif // LANDER_CAMERA_H
