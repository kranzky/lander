#include <SDL.h>
#include <cstdlib>
#include <cstring>
#include "constants.h"
#include "screen.h"
#include "palette.h"

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

    const int W = ScreenBuffer::PHYSICAL_WIDTH;   // 1280
    const int H = ScreenBuffer::PHYSICAL_HEIGHT;  // 1024

    // =========================================================================
    // Triangle demonstration - draw various triangles to test rasterization
    // =========================================================================

    // Large triangles spread across the screen
    int centerX = W / 2;
    int centerY = H / 2;

    // Red: pointing up (top center)
    screen.drawTriangle(centerX - 150, 50, centerX + 150, 50, centerX, 250, Color::red());

    // Green: pointing down (bottom center)
    screen.drawTriangle(centerX - 150, H - 50, centerX + 150, H - 50, centerX, H - 250, Color::green());

    // Blue: pointing right (left side)
    screen.drawTriangle(50, centerY - 150, 50, centerY + 150, 250, centerY, Color::blue());

    // Yellow: pointing left (right side)
    screen.drawTriangle(W - 50, centerY - 150, W - 50, centerY + 150, W - 250, centerY, Color::yellow());

    // Cyan: large center triangle
    screen.drawTriangle(centerX, centerY - 200, centerX - 200, centerY + 150, centerX + 200, centerY + 150, Color::cyan());

    // Magenta: overlapping triangle (tests drawing order)
    screen.drawTriangle(centerX - 100, centerY, centerX + 100, centerY, centerX, centerY + 200, Color::magenta());

    // White: thin triangles in corners
    screen.drawTriangle(50, 50, 200, 50, 125, 150, Color::white());
    screen.drawTriangle(W - 200, 50, W - 50, 50, W - 125, 150, Color::white());
    screen.drawTriangle(50, H - 50, 200, H - 50, 125, H - 150, Color::white());
    screen.drawTriangle(W - 200, H - 50, W - 50, H - 50, W - 125, H - 150, Color::white());
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
