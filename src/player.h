#ifndef LANDER_PLAYER_H
#define LANDER_PLAYER_H

#include "fixed.h"
#include "math3d.h"
#include "polar_coords.h"
#include "constants.h"

// =============================================================================
// Player State and Input
// =============================================================================
//
// Based on the original Lander player variables (Lander.arm lines 309-331):
//   - xPlayer, yPlayer, zPlayer: Ship position
//   - xVelocity, yVelocity, zVelocity: Ship velocity
//   - xExhaust, yExhaust, zExhaust: Exhaust vector direction
//
// Input is read from mouse (Lander.arm lines 1743-1770):
//   - Mouse position controls ship orientation (converted to polar in Task 14)
//   - Mouse buttons control thrust:
//     - Right button (bit 0): Fire bullets
//     - Middle button (bit 1): Hover (medium thrust)
//     - Left button (bit 2): Full thrust
//
// =============================================================================

// Mouse button flags (matching original Lander bit positions)
namespace MouseButton {
    constexpr uint8_t FIRE = 0x01;       // Right button - fire bullets
    constexpr uint8_t HOVER = 0x02;      // Middle button - hover/medium thrust
    constexpr uint8_t THRUST = 0x04;     // Left button - full thrust
}

// Input state captured each frame
struct InputState {
    // Raw mouse position (screen coordinates)
    int mouseX = 0;
    int mouseY = 0;

    // Mouse position relative to center, scaled to ±512 range
    // (matching original Lander's coordinate system)
    int mouseRelX = 0;     // -512 to +511
    int mouseRelY = 0;     // -512 to +512

    // Mouse button state (bits as per MouseButton namespace)
    uint8_t buttons = 0;

    // Convenience accessors
    bool isFiring() const { return (buttons & MouseButton::FIRE) != 0; }
    bool isHovering() const { return (buttons & MouseButton::HOVER) != 0; }
    bool isThrusting() const { return (buttons & MouseButton::THRUST) != 0; }

    // Get fuel burn rate based on buttons (0, 2, or 4)
    // Bit 0 (fire) is ignored for fuel - only bits 1 and 2 matter
    uint8_t getFuelBurnRate() const { return buttons & 0x06; }
};

class Player {
public:
    Player();

    // Initialize player at starting position (launchpad)
    void reset();

    // Update input state from SDL (absolute screen coordinates)
    void updateInput(int mouseX, int mouseY, uint32_t sdlButtonState);

    // Update input state from relative mouse coordinates (already in ±range format)
    void updateInputRelative(int relX, int relY, uint32_t sdlButtonState);

    // Apply keyboard-based movement (for testing/debugging)
    void applyDebugMovement(bool left, bool right, bool forward, bool back,
                           bool up, bool down, Fixed speed);

    // Update ship orientation from current mouse input
    // Converts mouse position to polar coordinates, then smoothly interpolates
    // shipDirection and shipPitch toward target values, and computes rotation matrix
    void updateOrientation();

    // Position accessors
    const Vec3& getPosition() const { return position; }
    Fixed getX() const { return position.x; }
    Fixed getY() const { return position.y; }
    Fixed getZ() const { return position.z; }

    void setPosition(const Vec3& pos) { position = pos; }
    void setPosition(Fixed x, Fixed y, Fixed z) {
        position.x = x;
        position.y = y;
        position.z = z;
    }

    // Velocity accessors
    const Vec3& getVelocity() const { return velocity; }
    void setVelocity(const Vec3& vel) { velocity = vel; }
    void setVelocity(Fixed vx, Fixed vy, Fixed vz) {
        velocity.x = vx;
        velocity.y = vy;
        velocity.z = vz;
    }

    // Exhaust direction (points along exhaust plume)
    const Vec3& getExhaustDirection() const { return exhaustDirection; }
    void setExhaustDirection(const Vec3& dir) { exhaustDirection = dir; }

    // Ship orientation angles (32-bit angle format: 0x80000000 = 180 degrees)
    int32_t getShipDirection() const { return shipDirection; }
    int32_t getShipPitch() const { return shipPitch; }
    void setShipDirection(int32_t dir) { shipDirection = dir; }
    void setShipPitch(int32_t pitch) { shipPitch = pitch; }

    // Ship rotation matrix (computed from direction and pitch)
    const Mat3x3& getRotationMatrix() const { return rotationMatrix; }

    // Input state
    const InputState& getInput() const { return input; }

    // Fuel management
    int getFuelLevel() const { return fuelLevel; }
    void setFuelLevel(int level) { fuelLevel = level; }
    bool hasFuel() const { return fuelLevel > 0; }
    void burnFuel(int amount);

private:
    // Position in world space (8.24 fixed-point)
    Vec3 position;

    // Velocity (8.24 fixed-point, units per frame)
    Vec3 velocity;

    // Exhaust direction vector (for particle spawning)
    Vec3 exhaustDirection;

    // Ship orientation angles (32-bit angle format)
    // shipDirection = yaw (rotation around Y axis) - controlled by polar angle from mouse X
    // shipPitch = pitch (tilt forward/back) - controlled by polar distance from mouse
    int32_t shipDirection;
    int32_t shipPitch;

    // Rotation matrix computed from shipDirection and shipPitch
    Mat3x3 rotationMatrix;

    // Current input state
    InputState input;

    // Fuel level (decreases when thrusting)
    int fuelLevel;
};

// =============================================================================
// Player Constants
// =============================================================================

namespace PlayerConstants {
    // Starting position - above the launchpad at world origin
    // Launchpad is at world (0-8, 0-8), so start player at center of launchpad
    // Player at (4, -2, 4), camera will be 5 tiles behind at (4, -2, -1)
    constexpr Fixed START_X = Fixed::fromRaw(0x04000000);      // 4 tiles (center of launchpad)
    constexpr Fixed START_Y = Fixed::fromRaw(static_cast<int32_t>(0xFE000000));  // -2 tiles (above ground)
    constexpr Fixed START_Z = Fixed::fromRaw(0x04000000);      // 4 tiles (center of launchpad)

    // Initial fuel level
    constexpr int INITIAL_FUEL = 65536;

    // Debug movement speed (tiles per frame)
    constexpr Fixed DEBUG_MOVE_SPEED = Fixed::fromRaw(0x00199999);  // ~0.1 tiles
}

#endif // LANDER_PLAYER_H
