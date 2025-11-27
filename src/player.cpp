#include "player.h"

// =============================================================================
// Player Implementation
// =============================================================================

Player::Player()
    : position()
    , velocity()
    , exhaustDirection()
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
