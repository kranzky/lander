#include "player.h"
#include "landscape.h"
#include "object3d.h"

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

    // Reset orientation (matching original: direction=0, pitch=1 for slight upward tilt)
    shipDirection = 0;
    shipPitch = 1;
    rotationMatrix = calculateRotationMatrix(shipPitch, shipDirection);

    // Clear input
    input = InputState();
}

void Player::updateInput(int mouseX, int mouseY, uint32_t sdlButtonState) {
    // Store raw mouse position
    input.mouseX = mouseX;
    input.mouseY = mouseY;

    // Convert to relative coordinates centered on screen
    // Original Lander uses 1024x1024 mouse range centered at (512, 512)
    // SDL gives us pixel coordinates, so we need to map from screen size

    // Map screen coordinates to -512 to +511 range
    // Screen center is at (SCREEN_WIDTH/2, SCREEN_HEIGHT/2)
    constexpr int HALF_WIDTH = 320 / 2;   // 160
    constexpr int HALF_HEIGHT = 256 / 2;  // 128

    // Calculate offset from center
    int offsetX = mouseX - HALF_WIDTH;
    int offsetY = mouseY - HALF_HEIGHT;

    // Scale to -512 to +512 range
    // The original uses a larger range to allow more precise control
    // We scale based on screen dimensions to get similar feel
    input.mouseRelX = (offsetX * 512) / HALF_WIDTH;
    input.mouseRelY = (offsetY * 512) / HALF_HEIGHT;

    // Clamp to valid range
    if (input.mouseRelX < -512) input.mouseRelX = -512;
    if (input.mouseRelX > 511) input.mouseRelX = 511;
    if (input.mouseRelY < -512) input.mouseRelY = -512;
    if (input.mouseRelY > 512) input.mouseRelY = 512;

    // Convert SDL button state to original Lander format
    // SDL: SDL_BUTTON_LEFT=1, SDL_BUTTON_MIDDLE=2, SDL_BUTTON_RIGHT=3
    // Original: Right=bit0, Middle=bit1, Left=bit2
    input.buttons = 0;

    // SDL_BUTTON(x) creates a mask, SDL_BUTTON_LMASK etc are the masks
    if (sdlButtonState & 0x04) {  // SDL_BUTTON_RIGHT (button 3)
        input.buttons |= MouseButton::FIRE;
    }
    if (sdlButtonState & 0x02) {  // SDL_BUTTON_MIDDLE (button 2)
        input.buttons |= MouseButton::HOVER;
    }
    if (sdlButtonState & 0x01) {  // SDL_BUTTON_LEFT (button 1)
        input.buttons |= MouseButton::THRUST;
    }
}

void Player::updateInputRelative(int relX, int relY, uint32_t sdlButtonState) {
    // Store accumulated position as raw coordinates
    input.mouseX = relX;
    input.mouseY = relY;

    // Use directly as relative coordinates (already in appropriate range)
    input.mouseRelX = relX;
    input.mouseRelY = relY;

    // Convert SDL button state to original Lander format
    input.buttons = 0;
    if (sdlButtonState & 0x04) {  // SDL_BUTTON_RIGHT (button 3)
        input.buttons |= MouseButton::FIRE;
    }
    if (sdlButtonState & 0x02) {  // SDL_BUTTON_MIDDLE (button 2)
        input.buttons |= MouseButton::HOVER;
    }
    if (sdlButtonState & 0x01) {  // SDL_BUTTON_LEFT (button 1)
        input.buttons |= MouseButton::THRUST;
    }
}

void Player::applyDebugMovement(bool left, bool right, bool forward, bool back,
                                bool up, bool down, Fixed speed) {
    // Simple translation for debugging/testing
    // This bypasses normal physics for camera testing

    if (left) {
        position.x = Fixed::fromRaw(position.x.raw - speed.raw);
    }
    if (right) {
        position.x = Fixed::fromRaw(position.x.raw + speed.raw);
    }
    if (forward) {
        position.z = Fixed::fromRaw(position.z.raw + speed.raw);
    }
    if (back) {
        position.z = Fixed::fromRaw(position.z.raw - speed.raw);
    }
    if (up) {
        // Negative Y = higher altitude
        position.y = Fixed::fromRaw(position.y.raw - speed.raw);
    }
    if (down) {
        // Positive Y = lower altitude
        position.y = Fixed::fromRaw(position.y.raw + speed.raw);
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
// This is a direct port of the ship orientation code from MoveAndDrawPlayer
// (Lander.arm lines 1805-1867). The algorithm:
//
// 1. Convert mouse (x, y) to polar coordinates (angle, distance)
// 2. Smooth the transition from current angles to target angles:
//    - delta = current - target
//    - Cap delta to ±0x30000000 to prevent jerky movements
//    - new = current - delta/2 (halving gives smooth interpolation)
// 3. Calculate the rotation matrix from the smoothed angles
//
// The polar distance is scaled up (LSL #1) to range 0 to 0x7FFFFFFE
// so that moving the mouse to the edge produces full pitch.
//
// =============================================================================

void Player::updateOrientation() {
    // Convert mouse position to polar coordinates
    // The polar conversion expects input shifted << 22, so scale our ±512 range
    int32_t scaledX = input.mouseRelX << 22;
    int32_t scaledY = input.mouseRelY << 22;

    PolarCoordinates polar = getMouseInPolarCoordinates(scaledX, scaledY);

    // Extract target angle and distance
    int32_t targetAngle = polar.angle;
    int32_t targetDistance = polar.distance;

    // Scale distance to range 0 to 0x7FFFFFFE as in original (lines 1800-1803)
    // The original clamps distance to 0x40000000 then shifts left by 1
    if (static_cast<uint32_t>(targetDistance) >= 0x40000000u) {
        targetDistance = 0x40000000 - 1;
    }
    targetDistance <<= 1;

    // Calculate delta from current direction to target angle (lines 1809-1824)
    int32_t deltaDirection = shipDirection - targetAngle;

    // Cap deltaDirection to ±0x30000000 to prevent jerky movement
    if (deltaDirection >= 0) {
        if (deltaDirection > 0x30000000) {
            deltaDirection = 0x30000000;
        }
    } else {
        if (deltaDirection < -0x30000000) {
            deltaDirection = -0x30000000;
        }
    }

    // Calculate delta from current pitch to target distance (lines 1828-1843)
    int32_t deltaPitch = shipPitch - targetDistance;

    // Cap deltaPitch to ±0x30000000
    if (deltaPitch > 0) {
        if (deltaPitch > 0x30000000) {
            deltaPitch = 0x30000000;
        }
    } else {
        if (deltaPitch < -0x30000000) {
            deltaPitch = -0x30000000;
        }
    }

    // Update angles by halving the delta (smooth interpolation) (lines 1855-1865)
    // newPitch = pitch - deltaPitch/2
    // newDirection = direction - deltaDirection/2
    shipPitch = shipPitch - (deltaPitch >> 1);
    shipDirection = shipDirection - (deltaDirection >> 1);

    // Calculate the rotation matrix from the updated angles (line 1867)
    // Note: CalculateRotationMatrix takes (angleA=pitch, angleB=direction)
    rotationMatrix = calculateRotationMatrix(shipPitch, shipDirection);
}

// =============================================================================
// Ship Physics Update
// =============================================================================
//
// Port of the physics code from MoveAndDrawPlayer Part 2 (Lander.arm lines 1930-2048)
//
// Physics simulation:
// 1. Apply friction to velocity (multiply by 63/64)
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

    // Check if full thrust (left mouse button)
    bool fullThrust = input.isThrusting();

    // Apply friction: velocity *= 63/64 (subtract velocity >> 6)
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
    if (input.isHovering()) {
        velocity.x = Fixed::fromRaw(velocity.x.raw - (exhaustX.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
        velocity.y = Fixed::fromRaw(velocity.y.raw - (exhaustY.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
        velocity.z = Fixed::fromRaw(velocity.z.raw - (exhaustZ.raw >> PlayerConstants::HOVER_THRUST_SHIFT));
    }

    // Apply gravity (positive Y is down in Lander coordinate system)
    velocity.y = Fixed::fromRaw(velocity.y.raw + PlayerConstants::GRAVITY);

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
    fuelLevel += PlayerConstants::REFUEL_RATE;
    if (fuelLevel > PlayerConstants::MAX_FUEL) {
        fuelLevel = PlayerConstants::MAX_FUEL;
    }

    return LandingState::LANDED;
}
