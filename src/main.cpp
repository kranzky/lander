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
#include "clipping.h"
#include "settings.h"

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
    void update(int mouseRelX, int mouseRelY, uint32_t mouseButtons);
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
    int stateTimer = 0;  // Timer for explosion animation
    Vec3 explosionPos;   // Position where explosion occurred (for future particle effects)
    bool waitingForKeypress = false;  // True when showing "GAME OVER" message

    // Debug mode: keyboard controls, no physics
    bool debugMode = false;

    // FPS overlay toggle (default on for debug builds, off for release)
#ifdef NDEBUG
    bool showFPS = false;  // Release build: off by default
#else
    bool showFPS = true;   // Debug build: on by default
#endif

    // Sound system
    SoundSystem sound;
    int thrustHeldFrames = 0;  // How long thrust has been held (for filter effect)

    // Score (used for rock spawning threshold)
    // Original Lander starts with 500 points
    int score = 500;

    // Target FPS selection (index into FPS_OPTIONS array)
    int fpsIndex = DEFAULT_FPS_INDEX;  // Default to 15fps

    // Sound enabled toggle (on by default)
    bool soundEnabled = true;

    // Fullscreen toggle
    bool fullscreen = false;

    // Helper methods
    void maybeSpawnRock();  // Check if we should spawn a falling rock
    void triggerCrash();
    void respawnPlayer();
    void resetGame();
    void updateResolution();  // Recreate texture for new resolution
    void saveCurrentSettings();  // Save settings to file

    void drawFPS();
    void drawScoreBar();
    void drawGameOver();
    void drawDigit(int x, int y, int digit, Color color);
    void drawMinus(int x, int y, Color color);
    int drawNumber(int x, int y, int value, Color color);

    // High score tracking (initial high score is 500 in original Lander)
    int highScore = 500;
};

bool Game::init() {
    // Load saved settings
    GameSettings settings = loadSettings();
    DisplayConfig::scale = settings.scale;
    fpsIndex = settings.fpsIndex;
    fullscreen = settings.fullscreen;
    ClippingConfig::enabled = settings.smoothClipping;
    soundEnabled = settings.soundEnabled;
    GameConstants::landscapeScale = settings.landscapeScale;

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
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
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

    // Set logical size for DPI scaling (uses current resolution)
    int initWidth = DisplayConfig::getPhysicalWidth();
    int initHeight = DisplayConfig::getPhysicalHeight();
    SDL_RenderSetLogicalSize(renderer, initWidth, initHeight);

    // Create streaming texture for the screen buffer (at current resolution)
    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        initWidth,
        initHeight
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

    // Initialize sound system (uses loaded settings)
    if (!sound.init()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Sound system failed to initialize - continuing without audio");
    }
    sound.setEnabled(soundEnabled);

    // Apply fullscreen if loaded from settings
    if (fullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    // Report status
    int drawW, drawH;
    SDL_GL_GetDrawableSize(window, &drawW, &drawH);
    SDL_Log("Lander initialized: %dx%d render, %dx%d drawable @ %d FPS",
            initWidth, initHeight, drawW, drawH, FPS_OPTIONS[fpsIndex]);


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

            case SDL_WINDOWEVENT:
                // Track fullscreen state when changed via OS controls (e.g., macOS green button)
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    Uint32 flags = SDL_GetWindowFlags(window);
                    bool wasFullscreen = fullscreen;
                    fullscreen = (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
                    if (fullscreen != wasFullscreen) {
                        saveCurrentSettings();
                    }
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    // Release mouse when window loses focus
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                // Recapture mouse when clicking on window (after Cmd+Tab etc.)
                if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
                break;

            case SDL_KEYDOWN:
                // Ignore all keys when mouse is not captured
                if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
                    break;
                }

                // If waiting for keypress after game over, any key restarts
                if (waitingForKeypress && event.key.keysym.sym != SDLK_ESCAPE) {
                    waitingForKeypress = false;
                    resetGame();
                    break;
                }

                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (event.key.keysym.sym == SDLK_F11 ||
                           (event.key.keysym.sym == SDLK_RETURN &&
                            (event.key.keysym.mod & KMOD_ALT))) {
                    // Toggle fullscreen (F11 or Alt+Enter)
                    fullscreen = !fullscreen;
                    if (fullscreen) {
                        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                    } else {
                        SDL_SetWindowFullscreen(window, 0);
                    }
                    saveCurrentSettings();
                } else if (event.key.keysym.sym == SDLK_d) {
                    debugMode = !debugMode;
                } else if (event.key.keysym.sym == SDLK_TAB) {
                    showFPS = !showFPS;
                } else if (event.key.keysym.sym == SDLK_1) {
                    // Cycle through landscape scales: 1 -> 2 -> 4 -> 8 -> 1
                    int scale = GameConstants::landscapeScale;
                    if (scale == 1) scale = 2;
                    else if (scale == 2) scale = 4;
                    else if (scale == 4) scale = 8;
                    else scale = 1;
                    GameConstants::landscapeScale = scale;
                    saveCurrentSettings();
                } else if (event.key.keysym.sym == SDLK_2) {
                    // Cycle through target FPS: 15 -> 30 -> 60 -> 120 -> 15
                    fpsIndex = (fpsIndex + 1) % FPS_OPTION_COUNT;
                    saveCurrentSettings();
                } else if (event.key.keysym.sym == SDLK_3) {
                    // Cycle through display resolutions: 1x -> 2x -> 4x -> 1x
                    if (DisplayConfig::scale == 1) DisplayConfig::scale = 2;
                    else if (DisplayConfig::scale == 2) DisplayConfig::scale = 4;
                    else DisplayConfig::scale = 1;
                    updateResolution();
                    saveCurrentSettings();
                } else if (event.key.keysym.sym == SDLK_4) {
                    // Toggle smooth edge clipping
                    ClippingConfig::enabled = !ClippingConfig::enabled;
                    saveCurrentSettings();
                } else if (event.key.keysym.sym == SDLK_5) {
                    // Toggle sound on/off
                    soundEnabled = !soundEnabled;
                    sound.setEnabled(soundEnabled);
                    saveCurrentSettings();
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
    score = 500;  // Reset score to initial value (matching original Lander)
    player.reset();
    landingState = LandingState::LANDED;  // Start on launchpad
    crashRecoveryTimer = 0;
    accumulatedMouseX = 0;
    accumulatedMouseY = 0;
    gameState = GameState::PLAYING;
    stateTimer = 0;

    // Restore all destroyed objects (in place, without re-rolling RNG)
    objectMap.restoreDestroyedObjects();
}

void Game::updateResolution() {
    // Recreate texture for new resolution
    if (texture) {
        SDL_DestroyTexture(texture);
    }

    int width = DisplayConfig::getPhysicalWidth();
    int height = DisplayConfig::getPhysicalHeight();

    texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );

    // Update logical size for proper scaling
    SDL_RenderSetLogicalSize(renderer, width, height);

    SDL_Log("Resolution changed to %dx%d (scale %d)", width, height, DisplayConfig::scale);
}

void Game::saveCurrentSettings() {
    GameSettings settings;
    settings.scale = DisplayConfig::scale;
    settings.fpsIndex = fpsIndex;
    settings.fullscreen = fullscreen;
    settings.smoothClipping = ClippingConfig::enabled;
    settings.soundEnabled = soundEnabled;
    settings.landscapeScale = GameConstants::landscapeScale;
    saveSettings(settings);
}

// =============================================================================
// Rock Spawning
// =============================================================================
//
// Port of DropRocksFromTheSky from Lander.arm (lines 4578-4625).
//
// When score >= 800:
// - Generate random number 0-16383
// - If random < (score - 800), spawn a rock
// - Higher scores = more frequent rocks
//
// Rocks spawn 32 tiles above and 6 tiles in front of camera position.
//
// =============================================================================

void Game::maybeSpawnRock() {
    // Only spawn rocks when score >= 800
    if (score < 800) return;

    // Only spawn during normal gameplay
    if (gameState != GameState::PLAYING) return;

    // Frame rate scaling: original ran at 15fps, we run at 120fps
    // Only check every 8th frame to match original spawn rate
    static int frameCounter = 0;
    frameCounter++;
    if ((frameCounter & 7) != 0) return;

    // Random chance based on score
    // Original: random 0-16383, spawn if random < (score - 800)
    // This makes rocks more frequent at higher scores
    uint32_t rand = static_cast<uint32_t>(std::rand()) & 0x3FFF;  // 0 to 16383
    int threshold = score - 800;

    if (static_cast<int>(rand) >= threshold) {
        return;  // Don't spawn this frame
    }

    // Spawn rock in a circle of radius 30 tiles around the player
    // Generate random angle and distance within circle
    constexpr int32_t SPAWN_RADIUS = 30;
    int angle = std::rand() % 360;
    int distance = std::rand() % (SPAWN_RADIUS + 1);

    // Convert polar to cartesian (approximate using lookup or simple math)
    // Use simple approximation: sin/cos from angle
    float radians = angle * 3.14159f / 180.0f;
    int32_t offsetX = static_cast<int32_t>(distance * cosf(radians));
    int32_t offsetZ = static_cast<int32_t>(distance * sinf(radians));

    Vec3 playerPos = player.getPosition();
    constexpr int32_t ROCK_HEIGHT_ABOVE_PLAYER = 32 * 0x01000000;  // 32 tiles above player

    Vec3 rockPos;
    rockPos.x = Fixed::fromRaw(playerPos.x.raw + offsetX * GameConstants::TILE_SIZE.raw);
    rockPos.y = Fixed::fromRaw(playerPos.y.raw - ROCK_HEIGHT_ABOVE_PLAYER);  // Above player (negative Y = up)
    rockPos.z = Fixed::fromRaw(playerPos.z.raw + offsetZ * GameConstants::TILE_SIZE.raw);

    spawnRock(rockPos);
}

void Game::update(int mouseRelX, int mouseRelY, uint32_t mouseButtons) {
    // Update particles every frame (including during explosions)
    particleSystem.update();

    // Try to spawn falling rocks (score >= 800)
    maybeSpawnRock();

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
        // Each destroyed object adds 20 to score (matching original Lander)
        score += events.objectDestroyed * 20;
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
    if (events.rockExploded > 0) {
        float vol = calcSpatialVolume(events.rockExplodedPos);
        if (vol > 0.0f) sound.play(SoundId::BOOM, vol);
    }

    // Check rock-player collision (before game state changes)
    if (gameState == GameState::PLAYING && !debugMode) {
        if (checkRockPlayerCollision(player.getPosition(), camera.getPosition())) {
            // Rock hit player - trigger crash
            triggerCrash();
            return;  // Skip rest of update
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
                // Game over - wait for keypress
                gameState = GameState::GAME_OVER;
                waitingForKeypress = true;
            }
        }
        // Camera stays at explosion position during explosion
        camera.followTarget(explosionPos, false);
        return;
    }

    if (gameState == GameState::GAME_OVER) {
        // Wait for keypress to restart (handled in handleEvents)
        // Just keep the camera at explosion position
        camera.followTarget(explosionPos, false);
        return;
    }

    // Normal gameplay (GameState::PLAYING)

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
    accumulatedMouseX += mouseRelX * 2;
    accumulatedMouseY += mouseRelY * 2;

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
        // Burn fuel based on thrust level (bit 0 = fire, bit 1 = hover, bit 2 = full thrust)
        // Only bits 1 and 2 burn fuel (firing doesn't use fuel)
        // Original burns fuelBurnRate per frame at 15fps
        // At 120fps, we burn every 8th frame to match original rate
        static int fuelBurnCounter = 0;
        fuelBurnCounter++;
        if ((fuelBurnCounter & 7) == 0) {
            int burnRate = input.getFuelBurnRate();  // Returns buttons & 0x06
            player.burnFuel(burnRate);
        }

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
        // Firing a bullet costs 1 point (matching original Lander)
        if (score > 0) score--;
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
    Color black = Color::black();
    int scale = DisplayConfig::scale;
    int y = 248;  // Bottom row in logical coords (256 - 8)

    // Draw black background bar across the bottom (at current resolution)
    int physWidth = DisplayConfig::getPhysicalWidth();
    for (int row = 0; row < 8 * scale; row++) {
        screen.drawHorizontalLine(0, physWidth - 1, y * scale + row, black);
    }

    // Bottom left: Landscape size (e.g. "12x10")
    // TILES_X includes corners, so visible tiles = TILES_X - 1 x TILES_Z - 1
    int visibleX = TILES_X - 1;
    int visibleZ = TILES_Z - 1;
    int x = screen.drawInt(0, y, visibleX, white);
    x = screen.drawText(x, y, "x", white);
    screen.drawInt(x, y, visibleZ, white);

    // Bottom center: Target FPS / Actual FPS (e.g. "60/58")
    int targetFPS = FPS_OPTIONS[fpsIndex];
    int centerX = 160 - 24;  // Approximate center for "NNN/NNN"
    x = screen.drawInt(centerX, y, targetFPS, white);
    x = screen.drawText(x, y, "/", white);
    screen.drawInt(x, y, fpsDisplay, white);

    // Bottom right: Display resolution (e.g. "1280x1024")
    // Calculate position from right edge
    // "1280x1024" = 9 chars = 72 pixels, so start at 320 - 72 = 248
    int resWidth = DisplayConfig::getPhysicalWidth();
    int resHeight = DisplayConfig::getPhysicalHeight();
    x = screen.drawInt(248, y, resWidth, white);
    x = screen.drawText(x, y, "x", white);
    screen.drawInt(x, y, resHeight, white);
}


void Game::drawScoreBar() {
    // Score bar at top of screen, matching original Lander layout:
    // Original is 40 columns (320 pixels / 8 pixels per char)
    // Row 0: Title "Lander (C) D.J.Braben 1987"
    // Row 1: Score at left, lives at column 30, high score at column 35
    // Below row 1: Fuel bar (3 pixels tall)
    //
    // Characters are 8 pixels wide at scale 1

    Color white = Color::white();

    // Update high score if current score exceeds it
    if (score > highScore) {
        highScore = score;
    }

    constexpr int CHAR_WIDTH = 8;  // 8 pixels per character at scale 1

    // Row 0: Title (column 0)
    screen.drawText(0, 0, "Lander Demo/Practice (C) D.J.Braben 1987", white);

    // Row 1: Score at left (column 0)
    int y = 8;  // Row 1 = 8 pixels down
    screen.drawInt(0, y, score, white);

    // Lives at column 30 (240 pixels from left)
    screen.drawInt(30 * CHAR_WIDTH, y, lives, white);

    // High score at column 35 (280 pixels from left)
    screen.drawInt(35 * CHAR_WIDTH, y, highScore, white);

    // Fuel bar below the text (starts at y=16, 3 logical pixels tall)
    // Original: bar length = fuelLevel / 16, max fuel = 0x1400 = 5120
    // So max bar length = 5120 / 16 = 320 pixels (full screen width)
    Color fuelColor = GameColors::fuelBar();
    int fuelBarLength = player.getFuelLevel() / 16;
    if (fuelBarLength > 320) fuelBarLength = 320;
    if (fuelBarLength > 0) {
        // Draw 3 logical pixel rows for the fuel bar
        int fuelY = 16;  // Just below row 1 (logical y=16)
        int scale = DisplayConfig::scale;
        // Each logical row is 'scale' physical pixels tall
        for (int logicalRow = 0; logicalRow < 3; logicalRow++) {
            int physicalY = (fuelY + logicalRow) * scale;
            for (int subRow = 0; subRow < scale; subRow++) {
                screen.drawHorizontalLine(0, fuelBarLength * scale - 1,
                                          physicalY + subRow, fuelColor);
            }
        }
    }
}

void Game::drawGameOver() {
    // Draw "GAME OVER - press a key to start again" centered on screen
    // with black background rectangle behind the text

    const char* text = "GAME OVER - press a key to start again";

    constexpr int CHAR_WIDTH = 8;
    constexpr int CHAR_HEIGHT = 8;

    // Calculate text width
    int len = 0;
    for (const char* p = text; *p; p++) len++;
    int textWidth = len * CHAR_WIDTH;

    // Center position (screen is 320x256 logical)
    int x = (320 - textWidth) / 2;
    int y = 128 - CHAR_HEIGHT / 2;  // Center vertically

    // Draw black background rectangle (with some padding)
    int bgX = x - 8;
    int bgY = y - 4;
    int bgW = textWidth + 16;
    int bgH = CHAR_HEIGHT + 8;

    // Draw black rectangle at physical coordinates (using current scale)
    int scale = DisplayConfig::scale;
    Color black = Color::black();
    for (int row = 0; row < bgH * scale; row++) {
        screen.drawHorizontalLine(bgX * scale, (bgX + bgW) * scale - 1,
                                  (bgY * scale) + row, black);
    }

    // Draw the text
    Color white = Color::white();
    screen.drawText(x, y, text, white);
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

    // Buffer falling rocks (they're rendered as 3D objects, not sprites)
    bufferRocks(camera);

    // Buffer the player's ship for depth-sorted rendering
    bufferShip();

    // Buffer particles in front of ship (so they're drawn after ship)
    bufferParticlesInFront(camera, shipDepthZ);

    // Render the landscape, flushing object buffers after each row for correct Z-ordering
    // This draws landscape tiles, buffered objects (including ship), and particles in depth order
    landscapeRenderer.render(screen, camera);

    // Draw score bar at top of screen
    drawScoreBar();

    // Draw game over message if waiting for keypress
    if (gameState == GameState::GAME_OVER && waitingForKeypress) {
        drawGameOver();
    }

    // Draw FPS overlay (toggled with Tab key)
    if (showFPS) {
        drawFPS();
    }
}

void Game::render() {
    // Draw test pattern (will be replaced with actual game rendering)
    drawTestPattern();

    // Update texture with screen buffer contents
    // Use max pitch since buffer stride is always max width
    SDL_UpdateTexture(texture, nullptr, screen.getData(), ScreenBuffer::getPitch());

    // Clear and draw texture (SDL scales to fill window via logical size)
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

        // Handle events once per frame (not per physics step)
        handleEvents();

        // Get mouse input once per frame (before physics loop)
        // SDL_GetRelativeMouseState returns accumulated movement since last call
        // Only use mouse input when mouse is captured
        int relX = 0, relY = 0;
        uint32_t mouseButtons = SDL_GetRelativeMouseState(&relX, &relY);
        if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
            relX = 0;
            relY = 0;
            mouseButtons = 0;
        }

        // Run physics multiple times per frame at lower FPS
        // This keeps physics consistent regardless of frame rate
        int physicsScale = PHYSICS_SCALE[fpsIndex];
        for (int i = 0; i < physicsScale; i++) {
            // On first iteration, pass the mouse delta; on subsequent ones, pass 0
            // This ensures mouse movement is only applied once per frame
            update(i == 0 ? relX : 0, i == 0 ? relY : 0, mouseButtons);
        }
        render();

        // Frame rate limiting based on target FPS
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        Uint32 targetFrameTime = FRAME_TIME_MS_LOOKUP[fpsIndex];
        if (frameTime < targetFrameTime) {
            SDL_Delay(targetFrameTime - frameTime);
        }
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
