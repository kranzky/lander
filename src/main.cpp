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
    const int margin = 20;

    // =========================================================================
    // Full 256-color VIDC palette display (16x16 grid) - Left side
    // Uses drawHorizontalLine for efficient rectangle filling
    // =========================================================================
    int paletteW = (W / 2) - margin * 2;
    int paletteH = (H / 2) - margin * 2;
    int swatchW = paletteW / 16;
    int swatchH = paletteH / 16;

    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            uint8_t vidc = row * 16 + col;
            Color c = GameColors::fromVidc(vidc);

            int px = margin + col * swatchW;
            int py = margin + row * swatchH;

            // Fill rectangle using horizontal lines
            for (int dy = 0; dy < swatchH - 1; dy++) {
                screen.drawHorizontalLine(px, px + swatchW - 2, py + dy, c);
            }
        }
    }

    // =========================================================================
    // Landscape color gradient - Right side, top half
    // Shows 10 rows (distance) x 16 altitude variations
    // =========================================================================
    int landscapeStartX = W / 2 + margin;
    int landscapeStartY = margin;
    int landscapeW = W / 2 - margin * 2;
    int landscapeH = H / 2 - margin * 2;
    int tileW = landscapeW / 16;
    int tileH = landscapeH / 10;

    for (int row = 1; row <= 10; row++) {
        for (int alt = 0; alt < 16; alt++) {
            Color c = getLandscapeTileColor(alt, row, 0, TileType::Land);
            int px = landscapeStartX + alt * tileW;
            int py = landscapeStartY + (10 - row) * tileH;

            for (int dy = 0; dy < tileH - 1; dy++) {
                screen.drawHorizontalLine(px, px + tileW - 2, py + dy, c);
            }
        }
    }

    // =========================================================================
    // Bottom half - split into sections
    // =========================================================================
    int bottomY = H / 2 + margin;
    int bottomH = H / 2 - margin * 2;
    int sectionW = W / 4 - margin;

    // -------------------------------------------------------------------------
    // Section 1: Sea tiles (10 rows showing distance effect)
    // -------------------------------------------------------------------------
    int seaX = margin;
    int seaTileH = bottomH / 10;
    int seaTileW = sectionW;

    for (int row = 1; row <= 10; row++) {
        Color c = getLandscapeTileColor(0, row, 0, TileType::Sea);
        int py = bottomY + (10 - row) * seaTileH;

        for (int dy = 0; dy < seaTileH - 1; dy++) {
            screen.drawHorizontalLine(seaX, seaX + seaTileW - 2, py + dy, c);
        }
    }

    // -------------------------------------------------------------------------
    // Section 2: Launchpad tiles (10 rows showing distance effect)
    // -------------------------------------------------------------------------
    int launchpadX = margin + sectionW + margin;

    for (int row = 1; row <= 10; row++) {
        Color c = getLandscapeTileColor(0, row, 0, TileType::Launchpad);
        int py = bottomY + (10 - row) * seaTileH;

        for (int dy = 0; dy < seaTileH - 1; dy++) {
            screen.drawHorizontalLine(launchpadX, launchpadX + seaTileW - 2, py + dy, c);
        }
    }

    // -------------------------------------------------------------------------
    // Section 3: Object colors (ship faces with brightness levels)
    // -------------------------------------------------------------------------
    int objectX = margin + (sectionW + margin) * 2;
    uint16_t shipColors[] = {0x080, 0x040, 0x088, 0x044, 0xC80};
    int numColors = 5;
    int brightnessLevels = 10;
    int objSwatchW = sectionW / numColors;
    int objSwatchH = bottomH / brightnessLevels;

    for (int bright = 0; bright < brightnessLevels; bright++) {
        for (int i = 0; i < numColors; i++) {
            Color c = objectColorToRGB(shipColors[i], bright * 2);
            int px = objectX + i * objSwatchW;
            int py = bottomY + bright * objSwatchH;

            for (int dy = 0; dy < objSwatchH - 1; dy++) {
                screen.drawHorizontalLine(px, px + objSwatchW - 2, py + dy, c);
            }
        }
    }

    // -------------------------------------------------------------------------
    // Section 4: Smoke grey gradient and fuel bar
    // -------------------------------------------------------------------------
    int uiX = margin + (sectionW + margin) * 3;
    int greySwatchH = (bottomH - 60) / 16;

    // Grey gradient (16 levels)
    for (int level = 0; level < 16; level++) {
        Color c = GameColors::smokeGrey(level);
        int py = bottomY + level * greySwatchH;

        for (int dy = 0; dy < greySwatchH - 1; dy++) {
            screen.drawHorizontalLine(uiX, uiX + sectionW - 2, py + dy, c);
        }
    }

    // Fuel bar at bottom of section
    Color fuelColor = GameColors::fuelBar();
    int fuelY = bottomY + 16 * greySwatchH + 10;
    int fuelH = bottomH - 16 * greySwatchH - 20;

    for (int dy = 0; dy < fuelH; dy++) {
        screen.drawHorizontalLine(uiX, uiX + sectionW - 2, fuelY + dy, fuelColor);
    }

    // =========================================================================
    // Horizontal line demonstration - draw some colored lines across the screen
    // This specifically tests the drawHorizontalLine function
    // =========================================================================

    // Draw a series of colored lines at the very bottom of the screen
    int lineY = H - 10;
    screen.drawHorizontalLine(0, W/4 - 1, lineY, Color::red());
    screen.drawHorizontalLine(W/4, W/2 - 1, lineY, Color::green());
    screen.drawHorizontalLine(W/2, 3*W/4 - 1, lineY, Color::blue());
    screen.drawHorizontalLine(3*W/4, W - 1, lineY, Color::yellow());

    // Draw lines that test clipping (extend beyond screen bounds)
    screen.drawHorizontalLine(-100, 100, H - 6, Color::cyan());      // Clips left
    screen.drawHorizontalLine(W - 100, W + 100, H - 6, Color::magenta()); // Clips right

    // Draw a white line across the entire width
    screen.drawHorizontalLine(0, W - 1, H - 2, Color::white());
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
