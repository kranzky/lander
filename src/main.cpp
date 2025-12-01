#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include "constants.h"
#include "screen.h"
#include "palette.h"
#include "landscape_renderer.h"
#include "landscape.h"
#include "camera.h"
#include "player.h"
#include "fixed.h"
#include "object_renderer.h"
#include "object3d.h"
#include "particles.h"
#include "object_map.h"
#include "sound.h"

// =============================================================================
// Lander - C++/SDL Port
// Original game by David Braben (1987) for Acorn Archimedes
// =============================================================================

// Game states
enum class GameState {
    PLAYING,      // Normal gameplay
    EXPLODING,    // Crash animation playing (ship hidden)
    GAME_OVER     // No lives remaining
};

// Game constants
namespace GameConfig {
    constexpr int INITIAL_LIVES = 3;
    constexpr int EXPLOSION_DURATION = 60;  // Frames for explosion animation (~0.5 sec at 120fps)
    constexpr int GAME_OVER_DELAY = 180;    // Frames before game restarts (~1.5 sec at 120fps)
}

class Game {
public:
    Game() = default;
    ~Game() { shutdown(); }

    bool init();
    void run();
    void shutdown();

    // Screenshot mode: render one frame and save to PNG
    void setScreenshotMode(const char* filename) {
        screenshotMode = true;
        screenshotFilename = filename;
    }

private:
    void handleEvents();
    void update();
    void render();
    void drawTestPattern();
    void bufferShip();

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    ScreenBuffer screen;
    LandscapeRenderer landscapeRenderer;
    Camera camera;
    Player player;
    bool running = false;
    Uint32 lastFrameTime = 0;

    // Screenshot mode
    bool screenshotMode = false;
    const char* screenshotFilename = nullptr;

    // FPS counter
    Uint32 fpsLastTime = 0;
    int fpsFrameCount = 0;
    int fpsDisplay = 0;

    // Accumulated mouse position (simulates absolute positioning from relative movement)
    // Decays toward center each frame for spring-like return behavior
    int accumulatedMouseX = 0;
    int accumulatedMouseY = 0;

    // Landing state (start as LANDED on launchpad)
    LandingState landingState = LandingState::LANDED;
    int crashRecoveryTimer = 0;  // Frames before CRASHED can reset to FLYING

    // Game state
    GameState gameState = GameState::PLAYING;
    int lives = GameConfig::INITIAL_LIVES;
    int stateTimer = 0;  // Timer for explosion/game over animations
    Vec3 explosionPos;   // Position where explosion occurred (for future particle effects)

    // Debug mode: keyboard controls, no physics
    bool debugMode = false;

    // Sound system
    SoundSystem sound;
    int thrustHeldFrames = 0;  // How long thrust has been held (for filter effect)

    // Helper methods
    void triggerCrash();
    void respawnPlayer();
    void resetGame();

    void drawFPS();
    void drawDigit(int x, int y, int digit, Color color);
    void drawMinus(int x, int y, Color color);
    int drawNumber(int x, int y, int value, Color color);
};

bool Game::init() {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    // Create window
    window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH,
        SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s", SDL_GetError());
        return false;
    }

    // Create renderer
    renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s", SDL_GetError());
        return false;
    }

    // Set logical size for DPI scaling
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

    // Create streaming texture for the screen buffer
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    );

    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }

    running = true;
    lastFrameTime = SDL_GetTicks();

    // Player initializes itself with default position
    // Camera follows the player with a fixed offset (5 tiles behind on Z)
    camera.followTarget(player.getPosition(), false);

    // Enable relative mouse mode to capture the cursor
    SDL_SetRelativeMouseMode(SDL_TRUE);

    // Initialize the object map with random objects
    placeObjectsOnMap();

    // Initialize sound system
    if (!sound.init()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound system failed to initialize - continuing without audio");
    }

    // Report status
    int drawW, drawH;
    SDL_GL_GetDrawableSize(window, &drawW, &drawH);
    SDL_Log("Lander initialized: %dx%d logical, %dx%d drawable @ %d FPS",
            SCREEN_WIDTH, SCREEN_HEIGHT, drawW, drawH, TARGET_FPS);


    return true;
}

void Game::shutdown() {
    sound.shutdown();
    if (texture) {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_Quit();
}

void Game::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_d) {
                    debugMode = !debugMode;
                } else if (event.key.keysym.sym == SDLK_1) {
                    GameConstants::landscapeScale = 1;
                } else if (event.key.keysym.sym == SDLK_2) {
                    GameConstants::landscapeScale = 2;
                } else if (event.key.keysym.sym == SDLK_3) {
                    GameConstants::landscapeScale = 4;
                } else if (event.key.keysym.sym == SDLK_4) {
                    GameConstants::landscapeScale = 8;
                }
                break;
        }
    }
}

void Game::triggerCrash() {
    // Store explosion position for particle effects
    explosionPos = player.getPosition();

    // Spawn explosion particles (50 clusters = 200 particles for ship crash)
    spawnExplosionParticles(explosionPos, 50);

    // Play death sound (reduced volume to match spatial sounds)
    sound.play(SoundId::DEAD, 0.5f);

    // Stop any engine sounds
    sound.stopSound(SoundId::THRUST);
    sound.stopSound(SoundId::HOVER);

    // Transition to exploding state
    gameState = GameState::EXPLODING;
    stateTimer = GameConfig::EXPLOSION_DURATION;

    // Decrement lives
    lives--;
}

void Game::respawnPlayer() {
    // Reset player to launchpad
    player.reset();

    // Reset landing state (start as LANDED on launchpad)
    landingState = LandingState::LANDED;
    crashRecoveryTimer = 0;

    // Reset mouse accumulation so ship starts level
    accumulatedMouseX = 0;
    accumulatedMouseY = 0;

    // Back to playing
    gameState = GameState::PLAYING;
}

void Game::resetGame() {
    // Full game reset
    lives = GameConfig::INITIAL_LIVES;
    player.reset();
    landingState = LandingState::LANDED;  // Start on launchpad
    crashRecoveryTimer = 0;
    accumulatedMouseX = 0;
    accumulatedMouseY = 0;
    gameState = GameState::PLAYING;
    stateTimer = 0;
}

void Game::update() {
    // Update particles every frame (including during explosions)
    particleSystem.update();

    // Process particle events for sounds with spatial audio
    // Volume is based on distance from player (full volume within 2 tiles, fades to 0 at 10 tiles)
    ParticleEvents& events = getParticleEvents();
    Vec3 playerPos = player.getPosition();

    auto calcSpatialVolume = [&playerPos](const Vec3& eventPos) -> float {
        // Calculate squared distance in tiles (using X and Z, ignore Y)
        float dx = Fixed::fromRaw(eventPos.x.raw - playerPos.x.raw).toFloat();
        float dz = Fixed::fromRaw(eventPos.z.raw - playerPos.z.raw).toFloat();
        float distSq = dx * dx + dz * dz;

        // Full volume within 1 tile, then inverse square falloff
        // At distance 10, we want 30% volume: 0.3 = 1 / (1 + (100-1)/k) => k ≈ 42.4
        if (distSq <= 1.0f) return 1.0f;
        float vol = 1.0f / (1.0f + (distSq - 1.0f) / 42.4f);
        return (vol < 0.05f) ? 0.0f : vol;
    };

    if (events.objectDestroyed > 0) {
        float vol = calcSpatialVolume(events.objectDestroyedPos);
        if (vol > 0.0f) sound.play(SoundId::BOOM, vol);
    }
    if (events.bulletHitGround > 0) {
        float vol = calcSpatialVolume(events.bulletHitGroundPos);
        if (vol > 0.0f) sound.play(SoundId::SHOOT_IMPACT, vol);
    }
    if (events.bulletHitWater > 0) {
        // Only play if previous splash finished
        if (!sound.isPlaying(SoundId::SPLASH)) {
            float vol = calcSpatialVolume(events.bulletHitWaterPos);
            if (vol > 0.0f) sound.play(SoundId::SPLASH, vol);
        }
    }
    if (events.exhaustHitWater > 0) {
        // Play water sound once (not looped) when exhaust creates water splash
        if (!sound.isPlaying(SoundId::WATER)) {
            float vol = calcSpatialVolume(events.exhaustHitWaterPos);
            if (vol > 0.0f) sound.play(SoundId::WATER, vol);
        }
    }

    // Handle game state transitions
    if (gameState == GameState::EXPLODING) {
        // During explosion, just count down timer
        stateTimer--;
        if (stateTimer <= 0) {
            if (lives > 0) {
                // Respawn player on launchpad
                respawnPlayer();
            } else {
                // Game over
                gameState = GameState::GAME_OVER;
                stateTimer = GameConfig::GAME_OVER_DELAY;
            }
        }
        // Camera stays at explosion position during explosion
        camera.followTarget(explosionPos, false);
        return;
    }

    if (gameState == GameState::GAME_OVER) {
        // During game over, count down then restart
        stateTimer--;
        if (stateTimer <= 0) {
            resetGame();
        }
        return;
    }

    // Normal gameplay (GameState::PLAYING)

    // Get relative mouse movement (cursor is captured)
    int relX, relY;
    uint32_t mouseButtons = SDL_GetRelativeMouseState(&relX, &relY);

    // Debug mode: keyboard controls for position, mouse for rotation only
    if (debugMode) {
        const Uint8* keys = SDL_GetKeyboardState(nullptr);
        Vec3 pos = player.getPosition();
        constexpr int32_t MOVE_SPEED = 0x00100000;  // ~0.0625 tiles per frame

        // A/Z for height (Y axis - positive is down)
        if (keys[SDL_SCANCODE_A]) pos.y = Fixed::fromRaw(pos.y.raw - MOVE_SPEED);
        if (keys[SDL_SCANCODE_Z]) pos.y = Fixed::fromRaw(pos.y.raw + MOVE_SPEED);

        // Arrow keys for X/Z movement
        if (keys[SDL_SCANCODE_LEFT])  pos.x = Fixed::fromRaw(pos.x.raw - MOVE_SPEED);
        if (keys[SDL_SCANCODE_RIGHT]) pos.x = Fixed::fromRaw(pos.x.raw + MOVE_SPEED);
        if (keys[SDL_SCANCODE_UP])    pos.z = Fixed::fromRaw(pos.z.raw + MOVE_SPEED);
        if (keys[SDL_SCANCODE_DOWN])  pos.z = Fixed::fromRaw(pos.z.raw - MOVE_SPEED);

        player.setPosition(pos);
    }

    // Accumulate mouse position with scaling for sensitivity
    // No decay - ship maintains orientation until player moves mouse (like original)
    accumulatedMouseX += relX * 2;
    accumulatedMouseY += relY * 2;

    // Clamp to valid range (±512)
    if (accumulatedMouseX < -512) accumulatedMouseX = -512;
    if (accumulatedMouseX > 512) accumulatedMouseX = 512;
    if (accumulatedMouseY < -512) accumulatedMouseY = -512;
    if (accumulatedMouseY > 512) accumulatedMouseY = 512;

    // Pass accumulated position directly as relative coordinates
    // The values represent offset from center in the ±512 range that polar coords expect
    player.updateInputRelative(accumulatedMouseX, accumulatedMouseY, mouseButtons);

    // Update ship orientation based on mouse position
    player.updateOrientation();

    // Update ship physics (gravity, thrust, friction) - skip in debug mode
    bool hitTerrain = debugMode ? false : player.updatePhysics();

    // Check ship-object collision (from original Lander.arm lines 2095-2150)
    // Original logic:
    //   lowestSafeAlt = terrainAltitude - UNDERCARRIAGE_Y
    //   heightAboveLowest = lowestSafeAlt - playerY
    //   if heightAboveLowest >= SAFE_HEIGHT, skip collision (safe)
    // So collision happens when: (terrainY - UNDERCARRIAGE_Y) - playerY < SAFE_HEIGHT
    bool hitObject = false;
    if (!debugMode && !hitTerrain) {
        Vec3 pos = player.getPosition();
        Fixed terrainY = getLandscapeAltitude(pos.x, pos.z);
        // Calculate height above the lowest safe point (terrain minus undercarriage offset)
        Fixed lowestSafeAlt = Fixed::fromRaw(terrainY.raw - GameConstants::UNDERCARRIAGE_Y.raw);
        Fixed heightAboveLowest = Fixed::fromRaw(lowestSafeAlt.raw - pos.y.raw);

        if (heightAboveLowest.raw < GameConstants::SAFE_HEIGHT.raw) {
            // Player is low enough to potentially hit objects
            int tileX = pos.x.toInt() & 0xFF;
            int tileZ = pos.z.toInt() & 0xFF;
            uint8_t objectType = objectMap.getObjectAt(tileX, tileZ);

            // Collide with non-destroyed objects (types 0-11)
            if (objectType != ObjectType::NONE && objectType < 12) {
                hitObject = true;
                // Destroy the object
                uint8_t destroyedType = ObjectMap::getDestroyedType(objectType);
                objectMap.setObjectAt(tileX, tileZ, destroyedType);
            }
        }
    }

    // Spawn exhaust particles when engine is active (thrust button AND below altitude limit)
    const InputState& input = player.getInput();
    bool engineActive = player.isEngineActive();
    bool fullThrust = input.isThrusting();
    bool hoverThrust = input.isHovering() && !fullThrust;

    if (engineActive) {
        // Spawn from exhaust port (center of yellow triangle on ship underside)
        spawnExhaustParticles(player.getExhaustSpawnPoint(), player.getVelocity(),
                              player.getExhaustDirection(), fullThrust);

        // Track how long thrust has been held
        thrustHeldFrames++;

        // Calculate filter cutoff and pitch shift over ~5 seconds
        // At 120fps, 600 frames = 5 seconds
        float t = std::min(thrustHeldFrames, 600) / 600.0f;
        float filterCutoff = 1.0f - (0.7f * t);  // 1.0 -> 0.3
        float pitch = 1.0f - (0.15f * t);         // 1.0 -> 0.85 (slight pitch drop)

        // Handle thrust sounds (looped, reduced volume to match spatial sounds)
        if (fullThrust) {
            // Full thrust - play thrust sound, stop hover
            sound.stopSound(SoundId::HOVER);
            if (!sound.isPlaying(SoundId::THRUST)) {
                sound.playLoop(SoundId::THRUST, 0.5f);
            }
            sound.setLoopFilter(SoundId::THRUST, filterCutoff);
            sound.setLoopPitch(SoundId::THRUST, pitch);
        } else if (hoverThrust) {
            // Hover - play hover sound (pitched down thrust), stop thrust
            sound.stopSound(SoundId::THRUST);
            if (!sound.isPlaying(SoundId::HOVER)) {
                sound.playLoop(SoundId::HOVER, 0.5f);
            }
            sound.setLoopFilter(SoundId::HOVER, filterCutoff);
            sound.setLoopPitch(SoundId::HOVER, pitch);
        }
    } else {
        // Engine not active - stop all engine sounds and reset filter timer
        sound.stopSound(SoundId::THRUST);
        sound.stopSound(SoundId::HOVER);
        thrustHeldFrames = 0;
    }

    // Spawn bullet particle when firing (right button)
    // Fire every 8th frame to match original 15fps rate (120/8 = 15 bullets/sec)
    static int bulletFrameCounter = 0;
    bulletFrameCounter++;
    if (input.isFiring() && (bulletFrameCounter & 7) == 0) {
        // Gun direction is the nose vector from the rotation matrix
        Vec3 gunDir = player.getRotationMatrix().nose();
        // Spawn from nose (midpoint of vertices 0 and 1)
        spawnBulletParticle(player.getBulletSpawnPoint(), player.getVelocity(), gunDir);
        // Play shoot sound (reduced volume to match spatial sounds)
        sound.play(SoundId::SHOOT, 0.5f);
    }

    // Check for object collision - immediate crash, no landing possible
    if (hitObject) {
        landingState = LandingState::CRASHED;
        triggerCrash();
    }

    // Check for landing when terrain collision detected OR already landed
    // Skip in debug mode - no crashes
    if (!debugMode && !hitObject && (hitTerrain || landingState == LandingState::LANDED)) {
        // If already landed and player is thrusting upward, allow takeoff
        // Check if velocity is negative (upward) - this means thrust overcame gravity
        Vec3 vel = player.getVelocity();
        bool thrustingUp = vel.y.raw < 0;

        if (landingState == LandingState::LANDED && thrustingUp) {
            // Player is taking off - transition to flying
            landingState = LandingState::FLYING;
        } else if (landingState == LandingState::CRASHED) {
            // Already in crashed state - trigger the crash sequence
            triggerCrash();
        } else {
            // Check for landing/refueling
            LandingState newState = player.checkLanding();

            if (newState == LandingState::CRASHED) {
                // Crashed! Trigger crash sequence
                landingState = newState;
                triggerCrash();
            } else {
                landingState = newState;
            }

            // If not landed, stop downward movement to prevent bouncing through terrain
            if (landingState != LandingState::LANDED) {
                if (vel.y.raw > 0) {
                    vel.y = Fixed::fromInt(0);
                    player.setVelocity(vel);
                }
            }
        }
    }

    // Update camera to follow player (no height clamping for debugging)
    camera.followTarget(player.getPosition(), false);
}

// Simple 3x5 pixel font for digits 0-9
static const uint8_t DIGIT_FONT[10][5] = {
    {0b111, 0b101, 0b101, 0b101, 0b111},  // 0
    {0b010, 0b110, 0b010, 0b010, 0b111},  // 1
    {0b111, 0b001, 0b111, 0b100, 0b111},  // 2
    {0b111, 0b001, 0b111, 0b001, 0b111},  // 3
    {0b101, 0b101, 0b111, 0b001, 0b001},  // 4
    {0b111, 0b100, 0b111, 0b001, 0b111},  // 5
    {0b111, 0b100, 0b111, 0b101, 0b111},  // 6
    {0b111, 0b001, 0b001, 0b001, 0b001},  // 7
    {0b111, 0b101, 0b111, 0b101, 0b111},  // 8
    {0b111, 0b101, 0b111, 0b001, 0b111},  // 9
};

void Game::drawDigit(int x, int y, int digit, Color color) {
    if (digit < 0 || digit > 9) return;
    const int scale = 2;  // 2x scale for visibility
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (DIGIT_FONT[digit][row] & (0b100 >> col)) {
                // Draw scaled pixel
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        screen.plotPhysicalPixel(x + col * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
}

// Draw a minus sign
void Game::drawMinus(int x, int y, Color color) {
    const int scale = 2;
    for (int sx = 0; sx < 3 * scale; sx++) {
        for (int sy = 0; sy < scale; sy++) {
            screen.plotPhysicalPixel(x + sx, y + 4 + sy, color);
        }
    }
}

// Draw a signed integer, returns x position after last digit
int Game::drawNumber(int x, int y, int value, Color color) {
    int digitWidth = 8;

    if (value < 0) {
        drawMinus(x, y, color);
        x += digitWidth;
        value = -value;
    }

    // Handle zero specially
    if (value == 0) {
        drawDigit(x, y, 0, color);
        return x + digitWidth;
    }

    // Count digits and draw from left to right
    int temp = value;
    int numDigits = 0;
    while (temp > 0) {
        numDigits++;
        temp /= 10;
    }

    // Calculate divisor for leftmost digit
    int divisor = 1;
    for (int i = 1; i < numDigits; i++) {
        divisor *= 10;
    }

    // Draw each digit
    while (divisor > 0) {
        int digit = (value / divisor) % 10;
        drawDigit(x, y, digit, color);
        x += digitWidth;
        divisor /= 10;
    }

    return x;
}

void Game::drawFPS() {
    // Update FPS counter
    fpsFrameCount++;
    Uint32 currentTime = SDL_GetTicks();
    if (currentTime - fpsLastTime >= 1000) {
        fpsDisplay = fpsFrameCount;
        fpsFrameCount = 0;
        fpsLastTime = currentTime;
    }

    Color white = Color::white();
    Color yellow = Color(255, 255, 0);
    Color cyan = Color(0, 255, 255);
    int x = 8;
    int y = 8;

    // Draw FPS
    drawNumber(x, y, fpsDisplay, white);

    // Draw player position on second line (in yellow)
    int playerX = player.getX().toInt();
    int playerY = player.getY().toInt();
    int playerZ = player.getZ().toInt();

    y += 14;
    x = 8;
    x = drawNumber(x, y, playerX, yellow);
    x += 4;
    x = drawNumber(x, y, playerY, yellow);
    x += 4;
    drawNumber(x, y, playerZ, yellow);

    // Draw camera position on third line (in cyan)
    int camX = camera.getX().toInt();
    int camY = camera.getY().toInt();
    int camZ = camera.getZ().toInt();

    y += 14;
    x = 8;
    x = drawNumber(x, y, camX, cyan);
    x += 4;
    x = drawNumber(x, y, camY, cyan);
    x += 4;
    drawNumber(x, y, camZ, cyan);

    // Draw mouse state on fourth line (relative coords and buttons)
    const InputState& input = player.getInput();
    y += 14;
    x = 8;
    x = drawNumber(x, y, input.mouseRelX, white);
    x += 4;
    x = drawNumber(x, y, input.mouseRelY, white);
    x += 8;
    // Show button state as number (0-7)
    x = drawNumber(x, y, input.buttons, white);

    // Draw landing state indicator
    x += 16;
    Color stateColor;
    int stateValue;
    switch (landingState) {
        case LandingState::FLYING:
            stateColor = white;
            stateValue = 0;
            break;
        case LandingState::LANDED:
            stateColor = Color(0, 255, 0);  // Green
            stateValue = 1;
            break;
        case LandingState::CRASHED:
            stateColor = Color(255, 0, 0);  // Red
            stateValue = 2;
            break;
    }
    drawNumber(x, y, stateValue, stateColor);

    // Draw lives on fifth line
    y += 14;
    x = 8;
    Color livesColor = lives > 1 ? Color(0, 255, 0) : Color(255, 0, 0);  // Green if >1, red if 1 or 0
    x = drawNumber(x, y, lives, livesColor);

    // Draw game state indicator
    x += 16;
    Color gameStateColor;
    int gameStateValue;
    switch (gameState) {
        case GameState::PLAYING:
            gameStateColor = white;
            gameStateValue = 0;
            break;
        case GameState::EXPLODING:
            gameStateColor = Color(255, 128, 0);  // Orange
            gameStateValue = 1;
            break;
        case GameState::GAME_OVER:
            gameStateColor = Color(255, 0, 0);  // Red
            gameStateValue = 2;
            break;
    }
    drawNumber(x, y, gameStateValue, gameStateColor);

    // Draw particle count on sixth line
    y += 14;
    x = 8;
    Color magenta = Color(255, 0, 255);
    drawNumber(x, y, particleSystem.getParticleCount(), magenta);
}

void Game::bufferShip() {
    // Don't buffer the ship during explosion or game over
    if (gameState != GameState::PLAYING) {
        return;
    }

    // In the original Lander, the ship is always drawn at a fixed screen position:
    // X = 0 (centered horizontally)
    // Y = player's Y relative to camera (for altitude)
    // Z = LANDSCAPE_Z_MID (15 tiles in front of camera)
    //
    // This keeps the ship at a consistent visual size and position on screen
    // while the landscape scrolls beneath it.

    Vec3 shipScreenPos;
    shipScreenPos.x = Fixed::fromInt(0);  // Centered horizontally

    // Y position: use player's actual Y relative to camera
    // This allows the ship to move up/down visually as altitude changes
    Vec3 actualRelPos = camera.worldToCamera(player.getPosition());
    shipScreenPos.y = actualRelPos.y;

    // Z position: fixed distance in front of camera (15 tiles like original)
    // LANDSCAPE_Z_MID = LANDSCAPE_Z - CAMERA_PLAYER_Z = 20 - 5 = 15 tiles
    shipScreenPos.z = Fixed::fromInt(15);

    // Calculate which row the ship belongs to for depth sorting
    // The ship's world Z determines its row in the landscape grid
    // Row mapping: row = camTileZ + TILES_Z - 1 - worldZInt
    int camTileZ = camera.getZTile().toInt();
    int playerTileZ = player.getZ().toInt();
    int row = camTileZ + TILES_Z - 1 - playerTileZ;

    // Clamp to valid row range
    if (row < 0) row = 0;
    if (row >= TILES_Z) row = TILES_Z - 1;

    // Buffer the ship's shadow first (so it appears under the ship)
    Vec3 cameraWorldPos;
    cameraWorldPos.x = camera.getX();
    cameraWorldPos.y = camera.getY();
    cameraWorldPos.z = camera.getZ();
    bufferObjectShadow(shipBlueprint, shipScreenPos, player.getRotationMatrix(),
                       player.getPosition(), cameraWorldPos, row);

    // Buffer the ship using the object renderer
    bufferObject(shipBlueprint, shipScreenPos, player.getRotationMatrix(), row);
}

void Game::drawTestPattern() {
    // Clear to black
    screen.clear(Color::black());

    // Buffer objects first (they get drawn during landscape rendering for proper depth sorting)
    landscapeRenderer.renderObjects(screen, camera);

    // Ship's visual depth (15 tiles from camera, matching bufferShip)
    Fixed shipDepthZ = Fixed::fromInt(15);

    // Buffer particles behind ship first (so they're drawn before ship)
    bufferParticlesBehind(camera, shipDepthZ);

    // Buffer the player's ship for depth-sorted rendering
    bufferShip();

    // Buffer particles in front of ship (so they're drawn after ship)
    bufferParticlesInFront(camera, shipDepthZ);

    // Render the landscape, flushing object buffers after each row for correct Z-ordering
    // This draws landscape tiles, buffered objects (including ship), and particles in depth order
    landscapeRenderer.render(screen, camera);

    // Draw FPS overlay
    drawFPS();
}

void Game::render() {
    // Draw test pattern (will be replaced with actual game rendering)
    drawTestPattern();

    // Update texture with screen buffer contents
    SDL_UpdateTexture(texture, nullptr, screen.getData(), ScreenBuffer::getPitch());

    // Clear and draw texture
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void Game::run() {
    // Screenshot mode: render one frame and exit
    if (screenshotMode) {
        drawTestPattern();
        if (screen.savePNG(screenshotFilename)) {
            SDL_Log("Screenshot saved to: %s", screenshotFilename);
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to save screenshot");
        }
        return;
    }

    // Normal game loop
    while (running) {
        Uint32 frameStart = SDL_GetTicks();

        handleEvents();
        update();
        render();

        // Frame rate limiting (disabled for benchmarking)
        // Uint32 frameTime = SDL_GetTicks() - frameStart;
        // if (frameTime < FRAME_TIME_MS) {
        //     SDL_Delay(FRAME_TIME_MS - frameTime);
        // }
    }
}

int main(int argc, char* argv[]) {
    Game game;

    // Parse command line arguments
    const char* screenshotFile = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshotFile = argv[++i];
        }
    }

    if (!game.init()) {
        return EXIT_FAILURE;
    }

    if (screenshotFile) {
        game.setScreenshotMode(screenshotFile);
    }

    game.run();

    return EXIT_SUCCESS;
}
