#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include "constants.h"
#include "screen.h"

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
    bool running = false;
    Uint32 lastFrameTime = 0;

    // Screenshot mode
    bool screenshotMode = false;
    const char* screenshotFilename = nullptr;
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
    // Game logic will go here
}

void Game::drawTestPattern() {
    // Clear to black
    screen.clear(Color::black());

    // Draw corner markers at logical coordinates (scaled to physical)
    // These demonstrate that logical (0,0) maps to physical (0,0), etc.

    // Top-left corner - Red (logical 0,0 = physical 0,0)
    for (int i = 0; i < 20; i++) {
        screen.plotPhysicalPixel(i, 0, Color::red());
        screen.plotPhysicalPixel(0, i, Color::red());
    }

    // Top-right corner - Green (logical 319 = physical 1276)
    int right = ScreenBuffer::PHYSICAL_WIDTH - 1;
    for (int i = 0; i < 20; i++) {
        screen.plotPhysicalPixel(right - i, 0, Color::green());
        screen.plotPhysicalPixel(right, i, Color::green());
    }

    // Bottom-left corner - Blue (logical 255 = physical 1020)
    int bottom = ScreenBuffer::PHYSICAL_HEIGHT - 1;
    for (int i = 0; i < 20; i++) {
        screen.plotPhysicalPixel(i, bottom, Color::blue());
        screen.plotPhysicalPixel(0, bottom - i, Color::blue());
    }

    // Bottom-right corner - Yellow
    for (int i = 0; i < 20; i++) {
        screen.plotPhysicalPixel(right - i, bottom, Color::yellow());
        screen.plotPhysicalPixel(right, bottom - i, Color::yellow());
    }

    // Draw a smooth diagonal line in physical coordinates
    // From logical (10, 10) to (100, 100) = physical (40, 40) to (400, 400)
    int x0 = ScreenBuffer::toPhysicalX(10);
    int y0 = ScreenBuffer::toPhysicalY(10);
    int x1 = ScreenBuffer::toPhysicalX(100);
    int y1 = ScreenBuffer::toPhysicalY(100);
    for (int i = 0; i <= x1 - x0; i++) {
        screen.plotPhysicalPixel(x0 + i, y0 + i, Color::white());
    }

    // Draw a filled rectangle at screen center - Cyan
    // Logical center (160, 128) = physical (640, 512)
    int pcx = ScreenBuffer::toPhysicalX(ORIGINAL_WIDTH / 2);
    int pcy = ScreenBuffer::toPhysicalY(ORIGINAL_HEIGHT / 2);
    int halfW = 80;  // 20 logical * 4
    int halfH = 40;  // 10 logical * 4
    for (int py = pcy - halfH; py <= pcy + halfH; py++) {
        for (int px = pcx - halfW; px <= pcx + halfW; px++) {
            screen.plotPhysicalPixel(px, py, Color::cyan());
        }
    }

    // Draw rectangle border - Magenta
    int borderW = halfW + 4;
    int borderH = halfH + 4;
    for (int px = pcx - borderW; px <= pcx + borderW; px++) {
        screen.plotPhysicalPixel(px, pcy - borderH, Color::magenta());
        screen.plotPhysicalPixel(px, pcy + borderH, Color::magenta());
    }
    for (int py = pcy - borderH; py <= pcy + borderH; py++) {
        screen.plotPhysicalPixel(pcx - borderW, py, Color::magenta());
        screen.plotPhysicalPixel(pcx + borderW, py, Color::magenta());
    }

    // Draw some single logical pixels to show the grid alignment
    // These will be tiny single pixels at each logical position
    for (int lx = 20; lx <= 60; lx += 10) {
        screen.plotPixel(lx, 200, Color::white());
    }
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
