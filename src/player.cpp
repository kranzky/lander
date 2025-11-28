#include "player.h"
#include "landscape.h"

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

    // Check terrain collision - get altitude at player's (x, z) position
    Fixed terrainY = getLandscapeAltitude(position.x, position.z);

    // Check if player is below terrain (positive Y = down, so player.y > terrain.y means below)
    bool hitTerrain = false;
    if (position.y.raw > terrainY.raw) {
        position.y = terrainY;
        // Stop downward velocity when hitting terrain
        if (velocity.y.raw > 0) {
            velocity.y = Fixed::fromInt(0);
        }
        hitTerrain = true;
    }

    return hitTerrain;
}
