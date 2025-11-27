#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include "constants.h"
#include "screen.h"
#include "palette.h"
#include "landscape_renderer.h"
#include "camera.h"
#include "fixed.h"

// =============================================================================
// Lander - C++/SDL Port
// Original game by David Braben (1987) for Acorn Archimedes
// =============================================================================

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

    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    ScreenBuffer screen;
    LandscapeRenderer landscapeRenderer;
    Camera camera;
    bool running = false;
    Uint32 lastFrameTime = 0;

    // Screenshot mode
    bool screenshotMode = false;
    const char* screenshotFilename = nullptr;

    // Simulated player position (for testing camera)
    Vec3 playerPosition;

    // FPS counter
    Uint32 fpsLastTime = 0;
    int fpsFrameCount = 0;
    int fpsDisplay = 0;

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

    // Initialize player position at center of landscape
    // Camera offset is 5 tiles behind, so player Z of -5 gives camera Z of -10
    playerPosition.x = Fixed::fromInt(6);   // Center of 12-tile width
    playerPosition.y = Fixed::fromInt(-2);  // Above terrain (negative Y = higher)
    playerPosition.z = Fixed::fromInt(-5);  // Camera will be at Z=-10

    // Camera follows the player with a fixed offset (5 tiles behind on Z)
    camera.followTarget(playerPosition, false);

    // Report status
    int drawW, drawH;
    SDL_GL_GetDrawableSize(window, &drawW, &drawH);
    SDL_Log("Lander initialized: %dx%d logical, %dx%d drawable @ %d FPS",
            SCREEN_WIDTH, SCREEN_HEIGHT, drawW, drawH, TARGET_FPS);

    return true;
}

void Game::shutdown() {
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
                }
                break;
        }
    }
}

void Game::update() {
    // Poll keyboard state for continuous movement
    const Uint8* keyState = SDL_GetKeyboardState(nullptr);

    // Movement speed: 0.1 tiles per frame (about 6 tiles per second at 60fps)
    constexpr int32_t MOVE_SPEED = 0x00199999;  // ~0.1 in 8.24 fixed-point

    // Cursor keys: translate camera in X/Z plane
    if (keyState[SDL_SCANCODE_LEFT]) {
        playerPosition.x = Fixed::fromRaw(playerPosition.x.raw - MOVE_SPEED);
    }
    if (keyState[SDL_SCANCODE_RIGHT]) {
        playerPosition.x = Fixed::fromRaw(playerPosition.x.raw + MOVE_SPEED);
    }
    if (keyState[SDL_SCANCODE_UP]) {
        playerPosition.z = Fixed::fromRaw(playerPosition.z.raw + MOVE_SPEED);
    }
    if (keyState[SDL_SCANCODE_DOWN]) {
        playerPosition.z = Fixed::fromRaw(playerPosition.z.raw - MOVE_SPEED);
    }

    // A/Z keys: adjust height
    if (keyState[SDL_SCANCODE_A]) {
        playerPosition.y = Fixed::fromRaw(playerPosition.y.raw - MOVE_SPEED);
    }
    if (keyState[SDL_SCANCODE_Z]) {
        playerPosition.y = Fixed::fromRaw(playerPosition.y.raw + MOVE_SPEED);
    }

    // Update camera to follow player (no height clamping for debugging)
    camera.followTarget(playerPosition, false);
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
    int x = 8;
    int y = 8;

    // Draw FPS
    drawNumber(x, y, fpsDisplay, white);

    // Draw camera position on second line
    // Convert fixed-point to integer tiles for display
    int camX = camera.getX().toInt();
    int camY = camera.getY().toInt();
    int camZ = camera.getZ().toInt();

    y += 14;  // Move to next line
    x = 8;
    x = drawNumber(x, y, camX, white);
    x += 4;  // Small gap
    x = drawNumber(x, y, camY, white);
    x += 4;
    drawNumber(x, y, camZ, white);
}

void Game::drawTestPattern() {
    // Clear to black
    screen.clear(Color::black());

    // Render the landscape using the camera
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

        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME_MS) {
            SDL_Delay(FRAME_TIME_MS - frameTime);
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
