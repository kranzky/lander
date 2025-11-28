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

// Landing state enum
enum class LandingState {
    FLYING,         // Normal flight
    LANDED,         // Successfully landed on launchpad
    CRASHED         // Hit terrain too fast or outside launchpad
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

    // Update physics: apply gravity, thrust, friction, and update position
    // Returns true if the ship hit terrain
    bool updatePhysics();

    // Check for landing on launchpad after terrain collision detected
    // Returns the resulting landing state
    LandingState checkLanding();

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
    // Starting position - on the launchpad at world origin
    // Launchpad is at world (0-8, 0-8), so start player at center of launchpad
    // Player starts on the pad (at LAUNCHPAD_Y), camera will be 5 tiles behind
    constexpr Fixed START_X = Fixed::fromRaw(0x04000000);      // 4 tiles (center of launchpad)
    constexpr Fixed START_Z = Fixed::fromRaw(0x04000000);      // 4 tiles (center of launchpad)
    // START_Y is defined after LAUNCHPAD_Y below

    // Initial fuel level
    constexpr int INITIAL_FUEL = 65536;

    // Debug movement speed (tiles per frame)
    constexpr Fixed DEBUG_MOVE_SPEED = Fixed::fromRaw(0x00199999);  // ~0.1 tiles

    // ==========================================================================
    // Physics Constants (from original Lander.arm)
    // ==========================================================================
    // Original ran at ~15fps; we run at ~120fps (8x faster)
    // All per-frame values are scaled down by 8 (>> 3) to compensate
    // ==========================================================================

    // Gravity: Original 0x30000 per frame at 15fps
    // At 120fps: 0x30000 >> 3 = 0x6000
    constexpr int32_t GRAVITY = 0x6000;

    // Friction: Original velocity *= 63/64 per frame (subtract velocity >> 6)
    // At 8x frame rate, we need much less friction per frame
    // Use >> 9 (subtract 1/512 per frame) for similar decay over time
    constexpr int FRICTION_SHIFT = 9;  // Divide by 512

    // Thrust: Original exhaust >> 11 on 31-bit values at 15fps
    // We use 24-bit Fixed (+7 shift) and 8x frame rate (+3 shift)
    // Net: 11 - 7 + 3 = 7
    constexpr int FULL_THRUST_SHIFT = 7;  // Divide by 128

    // Hover: Original exhaust >> 13 on 31-bit values at 15fps
    // Net: 13 - 7 + 3 = 9
    constexpr int HOVER_THRUST_SHIFT = 9;  // Divide by 512

    // Sea level: Y position floor (landscape water level)
    // Original: SEA_LEVEL = &05500000 (5.3125 tiles)
    // In our coordinate system, positive Y is down, so this is the max Y value
    constexpr Fixed SEA_LEVEL = Fixed::fromRaw(0x05500000);

    // Highest altitude for engines to work (52 tiles above origin)
    // Original: HIGHEST_ALTITUDE = TILE_SIZE * 52
    // In our coordinate system, negative Y is up, so this is -52 tiles
    // Engines cut out above this altitude (player falls back down)
    constexpr Fixed HIGHEST_ALTITUDE = Fixed::fromRaw(static_cast<int32_t>(0xCC000000));  // -52 tiles

    // ==========================================================================
    // Landing Constants (from original Lander.arm)
    // ==========================================================================

    // Launchpad size: 8 tiles (tiles 0-7 in both X and Z)
    constexpr Fixed LAUNCHPAD_SIZE = Fixed::fromRaw(0x08000000);  // 8 tiles

    // Launchpad altitude (Y coordinate of the launchpad surface)
    constexpr Fixed LAUNCHPAD_ALTITUDE = Fixed::fromRaw(0x03500000);  // 3.3125 tiles

    // Undercarriage offset (how far below ship center the landing gear is)
    constexpr Fixed UNDERCARRIAGE_Y = Fixed::fromRaw(0x00640000);  // ~0.39 tiles

    // Y position of ship when landed (launchpad altitude minus undercarriage)
    constexpr Fixed LAUNCHPAD_Y = Fixed::fromRaw(
        LAUNCHPAD_ALTITUDE.raw - UNDERCARRIAGE_Y.raw);

    // Starting Y position - on the launchpad (same as LAUNCHPAD_Y)
    constexpr Fixed START_Y = LAUNCHPAD_Y;

    // Maximum safe landing velocity (sum of |vx| + |vy| + |vz|)
    // Tuned for gameplay feel - allows gentle landings but crashes on hard impacts
    // 0x00100000 = ~1024 in debug display units (raw >> 10)
    constexpr int32_t LANDING_SPEED = 0x00100000;

    // Fuel refuel rate while landed (added per frame)
    // Original: 0x20 per frame at 15fps
    // At 120fps: same per frame gives 8x faster refueling (feels better)
    constexpr int REFUEL_RATE = 0x20;

    // Maximum fuel level
    constexpr int MAX_FUEL = 0x1400;
}

#endif // LANDER_PLAYER_H
