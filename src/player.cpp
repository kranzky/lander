#include "player.h"
#include "landscape.h"
#include "object3d.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>

// =============================================================================
// Player Implementation
// =============================================================================

Player::Player()
    : position()
    , velocity()
    , exhaustDirection()
    , shipDirection(0)
    , shipPitch(1)  // Slightly pitched up for take-off (matching original)
    , rotationMatrix(Mat3x3::identity())
    , input()
    , fuelLevel(PlayerConstants::INITIAL_FUEL)
{
    reset();
}

void Player::reset() {
    // Set starting position
    position.x = PlayerConstants::START_X;
    position.y = PlayerConstants::START_Y;
    position.z = PlayerConstants::START_Z;

    // Zero velocity
    velocity.x = Fixed::fromInt(0);
    velocity.y = Fixed::fromInt(0);
    velocity.z = Fixed::fromInt(0);

    // Initial exhaust direction (pointing down, i.e., positive Y in world space)
    // This will be updated by the rotation matrix once ship physics are implemented
    exhaustDirection.x = Fixed::fromInt(0);
    exhaustDirection.y = Fixed::fromInt(1);
    exhaustDirection.z = Fixed::fromInt(0);

    // Reset fuel
    fuelLevel = PlayerConstants::INITIAL_FUEL;

    // As in the original: facing right, very slightly pitched up for take-off,
    // then turned by the mouse from the first frame
    shipPitch = 1;
    shipDirection = 0;
    rotationMatrix = calculateRotationMatrix(shipPitch, shipDirection);

    // Clear input
    input = InputState();
}

void Player::updateInput(int pointerX, int pointerY, uint32_t sdlButtonState) {
    // As MoveAndDrawPlayer (Lander.arm lines 1768-1781): cap x at 1023 (so the
    // right-hand 256 units of pointer travel do nothing) and centre it, then
    // flip y so positive is down. y isn't capped: once shifted up by 22 bits
    // it wraps every 1024 units, so past +512 it reads as -511 and the heading
    // target flips while the pitch falls again, continuing the ship's rotation
    // round the loop, just as in the original.
    input.mouseX = std::min(pointerX, 1023) - 512;
    input.mouseY = 512 - pointerY;

    // Convert SDL button state to original Lander format
    input.buttons = 0;
    if (sdlButtonState & SDL_BUTTON_RMASK) {
        input.buttons |= MouseButton::FIRE;
    }
    if (sdlButtonState & SDL_BUTTON_MMASK) {
        input.buttons |= MouseButton::HOVER;
    }
    if (sdlButtonState & SDL_BUTTON_LMASK) {
        input.buttons |= MouseButton::THRUST;
    }
}

void Player::burnFuel(int amount) {
    fuelLevel -= amount;
    if (fuelLevel < 0) {
        fuelLevel = 0;
    }
}

// =============================================================================
// Ship Orientation Update
// =============================================================================
//
// Port of the ship orientation code from MoveAndDrawPlayer (Lander.arm lines
// 1768-1867):
//
// 1. Convert the mouse (x, y) to polar coordinates: the angle sets the ship's
//    direction and the distance its pitch (full pitch at 512 from the centre)
// 2. Move the direction and pitch towards those targets, damping the controls
// 3. Calculate the rotation matrix from the new angles
//
// =============================================================================

namespace {
    // The original's orientation damping, once per 15fps frame: move half the
    // gap to the target, with the gap capped at &30000000. That's a constant
    // &18000000 per frame for big gaps, then halving once within the cap.
    constexpr double GAP_CAP = 0x30000000;

    // To apply that 1/8 of a frame at a time, measure a gap on a scale where
    // each original frame adds exactly 1: halvings count up from the cap, and
    // each capped &18000000 move beyond it counts as one more
    double framesFromCap(double gap)
    {
        double linearFrames = std::max(std::ceil((gap - GAP_CAP) / (GAP_CAP / 2)), 0.0);
        return std::log2(GAP_CAP / (gap - linearFrames * GAP_CAP / 2)) - linearFrames;
    }

    double gapAtFramesFromCap(double frames)
    {
        double linearFrames = std::max(std::ceil(-frames), 0.0);
        return GAP_CAP * std::exp2(-(frames + linearFrames)) + linearFrames * GAP_CAP / 2;
    }

    // Move an angle 1/8 of an original frame towards its target, so 8 physics
    // steps land exactly where one original frame would. The gap wraps, so
    // directions turn the short way round.
    int32_t approach(int32_t current, int32_t target)
    {
        int32_t gap = static_cast<int32_t>(static_cast<uint32_t>(current) - static_cast<uint32_t>(target));
        if (gap == 0) {
            return target;
        }

        double size = std::fabs(static_cast<double>(gap));
        double remaining = gapAtFramesFromCap(framesFromCap(size) + 1.0 / 8);
        int32_t newGap = static_cast<int32_t>(std::copysign(std::trunc(remaining), gap));
        return static_cast<int32_t>(static_cast<uint32_t>(target) + static_cast<uint32_t>(newGap));
    }
}

void Player::updateOrientation() {
    // Scale the mouse coordinates up as far as possible (-512 << 22 is
    // 0x80000000, as is +512 << 22), as the original does before the polar
    // conversion
    int32_t scaledX = static_cast<int32_t>(static_cast<uint32_t>(input.mouseX) << 22);
    int32_t scaledY = static_cast<int32_t>(static_cast<uint32_t>(input.mouseY) << 22);

    PolarCoordinates polar = getMouseInPolarCoordinates(scaledX, scaledY);

    // Cap the distance at &3FFFFFFF (reached 512 from the centre), then double
    // it, so pitch runs from 0 to &7FFFFFFE
    int32_t targetPitch = std::min(polar.distance, 0x3FFFFFFF) * 2;

    shipDirection = approach(shipDirection, polar.angle);
    shipPitch = approach(shipPitch, targetPitch);

    // Note: CalculateRotationMatrix takes (angleA=pitch, angleB=direction)
    rotationMatrix = calculateRotationMatrix(shipPitch, shipDirection);
}

// =============================================================================
// Ship Physics Update
// =============================================================================
//
// Port of the physics code from MoveAndDrawPlayer Part 2 (Lander.arm lines 1930-2048)
//
// Physics simulation (per 120Hz step; see the constants in player.h):
// 1. Apply friction to velocity
// 2. Apply thrust from engines (based on button state)
// 3. Apply velocity to position
// 4. Apply hover thrust (with slight delay for inertia feel)
// 5. Apply gravity to Y velocity
// 6. Clamp position to sea level floor
//
// The exhaust/thrust vector is the "roof" row of the rotation matrix,
// which points through the ship's floor (direction of thrust plume).
//
// =============================================================================

bool Player::updatePhysics() {
    // Get the exhaust/thrust vector from the rotation matrix
    // This is the "roof" vector - pointing through the ship's floor
    // In the rotation matrix, this is the second column (Y axis of ship frame)
    const Vec3& roofVec = rotationMatrix.roof();
    Fixed exhaustX = roofVec.x;
    Fixed exhaustY = roofVec.y;
    Fixed exhaustZ = roofVec.z;

    // Check if full thrust (left mouse button) and hover (middle button)
    bool fullThrust = input.isThrusting();
    bool hover = input.isHovering();

    // Check if we have fuel - no fuel means no thrust
    if (fuelLevel <= 0) {
        fullThrust = false;
        hover = false;
    }

    // Check if above the altitude where engines work (52 tiles up)
    // If too high, cut the engines - ship will fall back down due to gravity
    // In our coordinate system, negative Y is up, so check if Y < HIGHEST_ALTITUDE
    if (position.y.raw < PlayerConstants::HIGHEST_ALTITUDE.raw) {
        fullThrust = false;
        hover = false;
    }

    // Apply friction (63/64 per original frame)
    velocity.x = Fixed::fromRaw(velocity.x.raw - (velocity.x.raw >> PlayerConstants::FRICTION_SHIFT));
    velocity.y = Fixed::fromRaw(velocity.y.raw - (velocity.y.raw >> PlayerConstants::FRICTION_SHIFT));
    velocity.z = Fixed::fromRaw(velocity.z.raw - (velocity.z.raw >> PlayerConstants::FRICTION_SHIFT));

    // Apply full thrust if left button pressed
    // Thrust is SUBTRACTED because exhaust points down, thrust pushes up
    if (fullThrust) {
        velocity.x = Fixed::fromRaw(velocity.x.raw - (exhaustX.raw >> PlayerConstants::FULL_THRUST_SHIFT));
        velocity.y = Fixed::fromRaw(velocity.y.raw - (exhaustY.raw >> PlayerConstants::FULL_THRUST_SHIFT));
        velocity.z = Fixed::fromRaw(velocity.z.raw - (exhaustZ.raw >> PlayerConstants::FULL_THRUST_SHIFT));
    }

    // Apply velocity to position
    position.x = Fixed::fromRaw(position.x.raw + velocity.x.raw);
    position.y = Fixed::fromRaw(position.y.raw + velocity.y.raw);
    position.z = Fixed::fromRaw(position.z.raw + velocity.z.raw);

    // Apply hover thrust if middle button pressed (applied after position update for inertia feel)
    if (hover) {
        velocity.x = Fixed::fromRaw(velocity.x.raw - (exhaustX.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
        velocity.y = Fixed::fromRaw(velocity.y.raw - (exhaustY.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
        velocity.z = Fixed::fromRaw(velocity.z.raw - (exhaustZ.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
    }

    // Apply gravity (positive Y is down in Lander coordinate system)
    velocity.y = Fixed::fromRaw(velocity.y.raw + PlayerConstants::GRAVITY);

    // Clamp altitude to prevent fixed-point overflow
    // The 8.24 format wraps at -128 tiles (0x80000000), which causes visual glitches
    // We clamp to -120 tiles to leave some margin
    constexpr int32_t MAX_ALTITUDE = -120 * GameConstants::TILE_SIZE.raw;  // -120 tiles
    if (position.y.raw < MAX_ALTITUDE) {
        position.y = Fixed::fromRaw(MAX_ALTITUDE);
        // Also zero upward velocity to prevent bouncing against the ceiling
        if (velocity.y.raw < 0) {
            velocity.y = Fixed::fromInt(0);
        }
    }

    // Update exhaust direction for particle spawning (later tasks)
    exhaustDirection.x = exhaustX;
    exhaustDirection.y = exhaustY;
    exhaustDirection.z = exhaustZ;

    // Check terrain collision for all ship vertices
    // This prevents the ship from sinking into the terrain when tilted
    // We transform each vertex by the rotation matrix, add the ship position,
    // and check against terrain altitude at that world position.
    // Track the maximum penetration depth to push the ship up appropriately.

    bool hitTerrain = false;
    int32_t maxPenetration = 0;  // How far below terrain the deepest vertex is

    for (uint32_t i = 0; i < shipBlueprint.vertexCount; i++) {
        const ObjectVertex& vertex = shipBlueprint.vertices[i];

        // Get vertex in local coordinates
        Vec3 localVert;
        localVert.x = Fixed::fromRaw(vertex.x);
        localVert.y = Fixed::fromRaw(vertex.y);
        localVert.z = Fixed::fromRaw(vertex.z);

        // Transform by rotation matrix
        Vec3 rotatedVert = rotationMatrix * localVert;

        // Calculate world position of this vertex
        Fixed worldX = Fixed::fromRaw(position.x.raw + rotatedVert.x.raw);
        Fixed worldY = Fixed::fromRaw(position.y.raw + rotatedVert.y.raw);
        Fixed worldZ = Fixed::fromRaw(position.z.raw + rotatedVert.z.raw);

        // Get terrain altitude at this vertex's (x, z) position
        Fixed terrainY = getLandscapeAltitude(worldX, worldZ);

        // Check if vertex is below terrain (positive Y = down)
        int32_t penetration = worldY.raw - terrainY.raw;
        if (penetration > 0) {
            hitTerrain = true;
            if (penetration > maxPenetration) {
                maxPenetration = penetration;
            }
        }
    }

    // If any vertex hit terrain, push the ship up by the maximum penetration amount
    // NOTE: We do NOT zero velocity here - that's handled by checkLanding() if
    // landing is successful, or by crash handling otherwise.
    // This allows checkLanding() to see the actual velocity before collision.
    if (hitTerrain) {
        position.y = Fixed::fromRaw(position.y.raw - maxPenetration);
    }

    return hitTerrain;
}

// =============================================================================
// Update Exhaust Direction
// =============================================================================
//
// Updates the exhaust direction from the current rotation matrix.
// Used in debug mode when physics are skipped but we still want particles
// to spawn in the correct direction.
//
// =============================================================================

void Player::updateExhaustDirection() {
    const Vec3& roofVec = rotationMatrix.roof();
    exhaustDirection.x = roofVec.x;
    exhaustDirection.y = roofVec.y;
    exhaustDirection.z = roofVec.z;
}

// =============================================================================
// Launchpad Landing Check
// =============================================================================
//
// Port of LandOnLaunchpad (Lander.arm lines 2492-2564)
//
// This should be called when terrain collision is detected. It checks:
// 1. Is the ship over the launchpad? (0 <= x < LAUNCHPAD_SIZE && 0 <= z < LAUNCHPAD_SIZE)
// 2. Is the ship moving slowly enough? (|vx| + |vy| + |vz| < LANDING_SPEED)
//
// If both conditions are met, the ship has landed safely:
// - Set Y position to LAUNCHPAD_Y (sitting on the pad)
// - Zero velocity
// - Refuel incrementally
//
// If the ship is NOT over the launchpad, it's a crash.
// If the ship is going too fast, it's still flying (will bounce/crash on next check).
//
// =============================================================================

LandingState Player::checkLanding() {
    // Check if ship is over the launchpad
    // Launchpad is at (0,0) to (LAUNCHPAD_SIZE, LAUNCHPAD_SIZE)
    if (position.x.raw < 0 ||
        position.x.raw >= PlayerConstants::LAUNCHPAD_SIZE.raw ||
        position.z.raw < 0 ||
        position.z.raw >= PlayerConstants::LAUNCHPAD_SIZE.raw) {
        // Not over launchpad - this is a crash
        return LandingState::CRASHED;
    }

    // Check ship orientation - must be mostly upright to land safely
    // The "roof" vector points through the ship's floor (exhaust direction)
    // For a level ship, roof.y should be close to 1.0 (pointing down)
    // If roof.y < 0.5 (roughly 60 degrees tilt), the ship is too tilted
    const Vec3& roofVec = rotationMatrix.roof();
    if (roofVec.y.raw < 0x00800000) {  // 0.5 in 8.24 format
        // Ship is tilted too much or upside down - crash
        return LandingState::CRASHED;
    }

    // Over the launchpad and upright - check velocity
    // Calculate total velocity magnitude: |vx| + |vy| + |vz|
    // Note: We subtract gravity from Y velocity because gravity was already applied
    // this frame before collision detection. A ship hovering stationary will have
    // vy = GRAVITY, not vy = 0, so we compensate for this.
    int32_t absVelX = velocity.x.raw >= 0 ? velocity.x.raw : -velocity.x.raw;
    int32_t adjustedVelY = velocity.y.raw - PlayerConstants::GRAVITY;  // Remove this frame's gravity
    int32_t absVelY = adjustedVelY >= 0 ? adjustedVelY : -adjustedVelY;
    int32_t absVelZ = velocity.z.raw >= 0 ? velocity.z.raw : -velocity.z.raw;
    int32_t totalVelocity = absVelX + absVelY + absVelZ;

    if (totalVelocity >= PlayerConstants::LANDING_SPEED) {
        // Too fast to land safely - this is a crash
        return LandingState::CRASHED;
    }

    // Safe landing! Set position to landed position
    position.y = PlayerConstants::LAUNCHPAD_Y;

    // Zero velocity
    velocity.x = Fixed::fromInt(0);
    velocity.y = Fixed::fromInt(0);
    velocity.z = Fixed::fromInt(0);

    // Refuel (add fuel up to maximum)
    // Original adds 0x20 per frame at 15fps
    // At 120fps, we add every 8th frame to match original rate
    refuelTicks++;
    if ((refuelTicks & 7) == 0) {
        fuelLevel += PlayerConstants::REFUEL_RATE;
        if (fuelLevel > PlayerConstants::MAX_FUEL) {
            fuelLevel = PlayerConstants::MAX_FUEL;
        }
    }

    return LandingState::LANDED;
}

// =============================================================================
// Engine State Check
// =============================================================================
//
// Returns true if the engine is currently active (producing thrust/exhaust).
// Engine is active when:
// 1. A thrust button is pressed (full thrust or hover)
// 2. The ship is below the altitude limit (52 tiles up)
//
// =============================================================================

bool Player::isEngineActive() const {
    // Check if any thrust button is pressed
    bool buttonPressed = input.isThrusting() || input.isHovering();
    if (!buttonPressed) {
        return false;
    }

    // Check if we have fuel
    if (fuelLevel <= 0) {
        return false;
    }

    // Check if below the altitude limit where engines work
    // In our coordinate system, negative Y is up, so check if Y >= HIGHEST_ALTITUDE
    // (HIGHEST_ALTITUDE is a large negative value, -52 tiles)
    bool belowAltitudeLimit = position.y.raw >= PlayerConstants::HIGHEST_ALTITUDE.raw;

    return belowAltitudeLimit;
}

// =============================================================================
// Particle Spawn Point Calculations
// =============================================================================
//
// These functions calculate world-space spawn points for particles by
// transforming ship model vertices through the rotation matrix.
//
// Ship model vertex coordinates (from object3d.cpp):
//   Vertex 0 (nose edge): (0x01000000, 0x00500000, 0x00800000)
//   Vertex 1 (nose edge): (0x01000000, 0x00500000, 0xFF800000)
//   Vertex 6 (exhaust port): (0x00555555, 0x00500000, 0x00400000)
//   Vertex 7 (exhaust port): (0x00555555, 0x00500000, 0xFFC00000)
//   Vertex 8 (exhaust port): (0xFFCCCCCD, 0x00500000, 0x00000000)
//
// =============================================================================

Vec3 Player::getBulletSpawnPoint() const {
    // The nose of the ship is formed by the two vertices at the front of the
    // light green "window" triangle (Face 0: vertices 0, 1, 5 with color 0x080).
    // Vertex 5 is the roof/top point, while vertices 0 and 1 form the nose edge.
    //
    // Vertex 0: (0x01000000, 0x00500000, 0x00800000)  - right wing front
    // Vertex 1: (0x01000000, 0x00500000, 0xFF800000)  - right wing back
    //
    // Midpoint = ((v0 + v1) / 2):
    //   X: (0x01000000 + 0x01000000) / 2 = 0x01000000
    //   Y: (0x00500000 + 0x00500000) / 2 = 0x00500000
    //   Z: (0x00800000 + 0xFF800000) / 2 = 0x00000000

    Vec3 noseLocal;
    noseLocal.x = Fixed::fromRaw(0x01000000);  // 1.0 (front of ship)
    noseLocal.y = Fixed::fromRaw(0x00500000);  // 0.31 (slightly below center)
    noseLocal.z = Fixed::fromRaw(0x00000000);  // 0 (centered)

    // Transform by rotation matrix to get world-relative offset
    Vec3 noseRotated = rotationMatrix * noseLocal;

    // Add to ship position to get world spawn point
    Vec3 spawnPoint;
    spawnPoint.x = Fixed::fromRaw(position.x.raw + noseRotated.x.raw);
    spawnPoint.y = Fixed::fromRaw(position.y.raw + noseRotated.y.raw);
    spawnPoint.z = Fixed::fromRaw(position.z.raw + noseRotated.z.raw);

    return spawnPoint;
}

// Simple random number generator for exhaust spawn point variation
static uint32_t exhaustSpawnSeed = 0x87654321;

static uint32_t exhaustSpawnRandom() {
    exhaustSpawnSeed = exhaustSpawnSeed * 1103515245 + 12345;
    return exhaustSpawnSeed;
}

Vec3 Player::getExhaustSpawnPoint() const {
    // Exhaust port is the yellow triangle formed by vertices 6, 7, 8
    // Spawn from a random point on this triangle using barycentric coordinates
    //
    // Vertex 6: (0x00555555, 0x00500000, 0x00400000)
    // Vertex 7: (0x00555555, 0x00500000, 0xFFC00000)
    // Vertex 8: (0xFFCCCCCD, 0x00500000, 0x00000000)

    // Triangle vertices in local model space
    constexpr int32_t v6x = 0x00555555, v6y = 0x00500000, v6z = 0x00400000;
    constexpr int32_t v7x = 0x00555555, v7y = 0x00500000, v7z = static_cast<int32_t>(0xFFC00000);
    constexpr int32_t v8x = static_cast<int32_t>(0xFFCCCCCD), v8y = 0x00500000, v8z = 0x00000000;

    // Generate random barycentric coordinates (u, v) where u + v <= 1
    // Using the method: if u + v > 1, reflect to (1-u, 1-v)
    uint32_t randU = exhaustSpawnRandom() & 0xFFFF;  // 0 to 65535
    uint32_t randV = exhaustSpawnRandom() & 0xFFFF;

    // Normalize to 0.0-1.0 range (as 16.16 fixed point for interpolation)
    int32_t u = randU;      // 0 to 65535 represents 0.0 to ~1.0
    int32_t v = randV;

    // If u + v > 1 (65536), reflect
    if (u + v > 65536) {
        u = 65536 - u;
        v = 65536 - v;
    }

    int32_t w = 65536 - u - v;  // Third barycentric coordinate

    // Bias towards center by shrinking the triangle to half size
    // Blend each coordinate towards 1/3 (the centroid)
    // new_coord = 1/3 + 0.5 * (coord - 1/3) = 0.5 * coord + 1/6
    // In 16.16 fixed: 1/3 = 21845, 1/6 = 10923
    constexpr int32_t ONE_SIXTH = 10923;
    u = (u >> 1) + ONE_SIXTH;
    v = (v >> 1) + ONE_SIXTH;
    w = (w >> 1) + ONE_SIXTH;

    // Interpolate: P = w*v6 + u*v7 + v*v8 (all coords divided by 65536)
    Vec3 exhaustLocal;
    exhaustLocal.x = Fixed::fromRaw((int32_t)(((int64_t)w * v6x + (int64_t)u * v7x + (int64_t)v * v8x) >> 16));
    exhaustLocal.y = Fixed::fromRaw((int32_t)(((int64_t)w * v6y + (int64_t)u * v7y + (int64_t)v * v8y) >> 16));
    exhaustLocal.z = Fixed::fromRaw((int32_t)(((int64_t)w * v6z + (int64_t)u * v7z + (int64_t)v * v8z) >> 16));

    // Transform by rotation matrix to get world-relative offset
    Vec3 exhaustRotated = rotationMatrix * exhaustLocal;

    // Add to ship position to get world spawn point
    Vec3 spawnPoint;
    spawnPoint.x = Fixed::fromRaw(position.x.raw + exhaustRotated.x.raw);
    spawnPoint.y = Fixed::fromRaw(position.y.raw + exhaustRotated.y.raw);
    spawnPoint.z = Fixed::fromRaw(position.z.raw + exhaustRotated.z.raw);

    return spawnPoint;
}
