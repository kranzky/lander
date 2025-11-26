#include <cstdio>
#include <cstdlib>
#include "../src/screen.h"

// =============================================================================
// Simple Test Framework
// =============================================================================

static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    std::printf("  %s... ", #name); \
    testsRun++; \
    try { \
        test_##name(); \
        testsPassed++; \
        std::printf("PASSED\n"); \
    } catch (...) { \
        testsFailed++; \
        std::printf("FAILED (exception)\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        std::printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", \
                    #cond, __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("FAILED\n    Expected %s == %s\n    Got %d vs %d\n    at %s:%d\n", \
                    #a, #b, (int)(a), (int)(b), __FILE__, __LINE__); \
        testsFailed++; \
        testsRun++; \
        return; \
    } \
} while(0)

// =============================================================================
// Color Tests
// =============================================================================

TEST(color_default_constructor) {
    Color c;
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    ASSERT_EQ(c.a, 255);
}

TEST(color_rgb_constructor) {
    Color c(100, 150, 200);
    ASSERT_EQ(c.r, 100);
    ASSERT_EQ(c.g, 150);
    ASSERT_EQ(c.b, 200);
    ASSERT_EQ(c.a, 255);
}

TEST(color_rgba_constructor) {
    Color c(100, 150, 200, 128);
    ASSERT_EQ(c.r, 100);
    ASSERT_EQ(c.g, 150);
    ASSERT_EQ(c.b, 200);
    ASSERT_EQ(c.a, 128);
}

TEST(color_predefined) {
    Color black = Color::black();
    ASSERT_EQ(black.r, 0);
    ASSERT_EQ(black.g, 0);
    ASSERT_EQ(black.b, 0);

    Color white = Color::white();
    ASSERT_EQ(white.r, 255);
    ASSERT_EQ(white.g, 255);
    ASSERT_EQ(white.b, 255);

    Color red = Color::red();
    ASSERT_EQ(red.r, 255);
    ASSERT_EQ(red.g, 0);
    ASSERT_EQ(red.b, 0);
}

// =============================================================================
// ScreenBuffer Constants Tests
// =============================================================================

TEST(screen_constants) {
    ASSERT_EQ(ScreenBuffer::LOGICAL_WIDTH, 320);
    ASSERT_EQ(ScreenBuffer::LOGICAL_HEIGHT, 256);
    ASSERT_EQ(ScreenBuffer::PHYSICAL_WIDTH, 1280);
    ASSERT_EQ(ScreenBuffer::PHYSICAL_HEIGHT, 1024);
    ASSERT_EQ(ScreenBuffer::PIXEL_SCALE, 4);
}

TEST(screen_buffer_size) {
    // 1280 * 1024 * 4 bytes (RGBA) = 5,242,880 bytes
    ASSERT_EQ(ScreenBuffer::getBufferSize(), 1280 * 1024 * 4);
}

TEST(screen_pitch) {
    // 1280 * 4 bytes per row
    ASSERT_EQ(ScreenBuffer::getPitch(), 1280 * 4);
}

TEST(screen_coordinate_conversion) {
    // Logical to physical conversion
    ASSERT_EQ(ScreenBuffer::toPhysicalX(0), 0);
    ASSERT_EQ(ScreenBuffer::toPhysicalY(0), 0);
    ASSERT_EQ(ScreenBuffer::toPhysicalX(1), 4);
    ASSERT_EQ(ScreenBuffer::toPhysicalY(1), 4);
    ASSERT_EQ(ScreenBuffer::toPhysicalX(319), 1276);
    ASSERT_EQ(ScreenBuffer::toPhysicalY(255), 1020);
}

// =============================================================================
// ScreenBuffer Bounds Tests
// =============================================================================

TEST(screen_inbounds_logical) {
    ScreenBuffer screen;

    // Valid logical corners
    ASSERT(screen.inBounds(0, 0));
    ASSERT(screen.inBounds(319, 0));
    ASSERT(screen.inBounds(0, 255));
    ASSERT(screen.inBounds(319, 255));

    // Center
    ASSERT(screen.inBounds(160, 128));

    // Out of bounds
    ASSERT(!screen.inBounds(-1, 0));
    ASSERT(!screen.inBounds(0, -1));
    ASSERT(!screen.inBounds(320, 0));
    ASSERT(!screen.inBounds(0, 256));
}

TEST(screen_inbounds_physical) {
    ScreenBuffer screen;

    // Valid physical corners
    ASSERT(screen.inPhysicalBounds(0, 0));
    ASSERT(screen.inPhysicalBounds(1279, 0));
    ASSERT(screen.inPhysicalBounds(0, 1023));
    ASSERT(screen.inPhysicalBounds(1279, 1023));

    // Out of bounds
    ASSERT(!screen.inPhysicalBounds(-1, 0));
    ASSERT(!screen.inPhysicalBounds(0, -1));
    ASSERT(!screen.inPhysicalBounds(1280, 0));
    ASSERT(!screen.inPhysicalBounds(0, 1024));
}

// =============================================================================
// ScreenBuffer Pixel Tests
// =============================================================================

TEST(screen_clear_black) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Check several physical pixels are black with full alpha
    Color c = screen.getPhysicalPixel(0, 0);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    ASSERT_EQ(c.a, 255);

    c = screen.getPhysicalPixel(640, 512);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
    ASSERT_EQ(c.a, 255);
}

TEST(screen_clear_color) {
    ScreenBuffer screen;
    Color testColor(100, 150, 200, 255);
    screen.clear(testColor);

    Color c = screen.getPhysicalPixel(0, 0);
    ASSERT_EQ(c.r, 100);
    ASSERT_EQ(c.g, 150);
    ASSERT_EQ(c.b, 200);
    ASSERT_EQ(c.a, 255);

    c = screen.getPhysicalPixel(1279, 1023);
    ASSERT_EQ(c.r, 100);
    ASSERT_EQ(c.g, 150);
    ASSERT_EQ(c.b, 200);
}

TEST(screen_plot_physical_pixel) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Plot a red pixel at physical coordinates
    screen.plotPhysicalPixel(100, 50, Color::red());

    Color c = screen.getPhysicalPixel(100, 50);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);

    // Neighboring pixels should still be black
    Color neighbor = screen.getPhysicalPixel(99, 50);
    ASSERT_EQ(neighbor.r, 0);
    neighbor = screen.getPhysicalPixel(101, 50);
    ASSERT_EQ(neighbor.r, 0);
}

TEST(screen_plot_logical_pixel) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Plot at logical (10, 20) should appear at physical (40, 80)
    screen.plotPixel(10, 20, Color::green());

    // Check the scaled physical position
    Color c = screen.getPhysicalPixel(40, 80);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 0);

    // Adjacent physical pixels should be black (no 4x4 block)
    Color neighbor = screen.getPhysicalPixel(41, 80);
    ASSERT_EQ(neighbor.g, 0);
    neighbor = screen.getPhysicalPixel(40, 81);
    ASSERT_EQ(neighbor.g, 0);
}

TEST(screen_plot_corners) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Plot at logical corners
    screen.plotPixel(0, 0, Color::red());
    screen.plotPixel(319, 0, Color::green());
    screen.plotPixel(0, 255, Color::blue());
    screen.plotPixel(319, 255, Color::yellow());

    // Verify at physical positions
    Color tl = screen.getPhysicalPixel(0, 0);
    ASSERT_EQ(tl.r, 255);
    ASSERT_EQ(tl.g, 0);

    Color tr = screen.getPhysicalPixel(1276, 0);
    ASSERT_EQ(tr.g, 255);
    ASSERT_EQ(tr.r, 0);

    Color bl = screen.getPhysicalPixel(0, 1020);
    ASSERT_EQ(bl.b, 255);

    Color br = screen.getPhysicalPixel(1276, 1020);
    ASSERT_EQ(br.r, 255);
    ASSERT_EQ(br.g, 255);
}

TEST(screen_plot_out_of_bounds) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // These should not crash or modify the buffer
    screen.plotPixel(-1, 0, Color::red());
    screen.plotPixel(0, -1, Color::red());
    screen.plotPixel(320, 0, Color::red());
    screen.plotPixel(0, 256, Color::red());

    screen.plotPhysicalPixel(-1, 0, Color::red());
    screen.plotPhysicalPixel(1280, 0, Color::red());
    screen.plotPhysicalPixel(0, 1024, Color::red());

    // Buffer should still be all black at corners
    ASSERT_EQ(screen.getPhysicalPixel(0, 0).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(1279, 1023).r, 0);
}

TEST(screen_get_out_of_bounds) {
    ScreenBuffer screen;
    screen.clear(Color::white());

    // Out of bounds should return black
    Color c = screen.getPhysicalPixel(-1, 0);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);

    c = screen.getPhysicalPixel(1280, 0);
    ASSERT_EQ(c.r, 0);

    c = screen.getPhysicalPixel(0, 1024);
    ASSERT_EQ(c.r, 0);
}

// =============================================================================
// ScreenBuffer Single Pixel (No Block) Tests
// =============================================================================

TEST(screen_single_pixel_not_block) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Plot at logical (50, 50) = physical (200, 200)
    screen.plotPixel(50, 50, Color::red());

    // Only the single physical pixel at (200, 200) should be set
    Color c = screen.getPhysicalPixel(200, 200);
    ASSERT_EQ(c.r, 255);

    // All surrounding pixels should be black
    ASSERT_EQ(screen.getPhysicalPixel(199, 200).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(201, 200).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(200, 199).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(200, 201).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(201, 201).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(202, 202).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(203, 203).r, 0);
}

TEST(screen_adjacent_logical_pixels) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Plot two adjacent logical pixels
    screen.plotPixel(10, 10, Color::red());
    screen.plotPixel(11, 10, Color::green());

    // Physical (40, 40) should be red
    Color c = screen.getPhysicalPixel(40, 40);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 0);

    // Physical (44, 40) should be green (11 * 4 = 44)
    c = screen.getPhysicalPixel(44, 40);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 255);

    // Physical pixels between them (41, 42, 43) should be black
    ASSERT_EQ(screen.getPhysicalPixel(41, 40).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(42, 40).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(43, 40).r, 0);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::printf("Screen Buffer Unit Tests\n");
    std::printf("========================\n\n");

    std::printf("Color tests:\n");
    RUN_TEST(color_default_constructor);
    RUN_TEST(color_rgb_constructor);
    RUN_TEST(color_rgba_constructor);
    RUN_TEST(color_predefined);

    std::printf("\nScreenBuffer constants tests:\n");
    RUN_TEST(screen_constants);
    RUN_TEST(screen_buffer_size);
    RUN_TEST(screen_pitch);
    RUN_TEST(screen_coordinate_conversion);

    std::printf("\nScreenBuffer bounds tests:\n");
    RUN_TEST(screen_inbounds_logical);
    RUN_TEST(screen_inbounds_physical);

    std::printf("\nScreenBuffer pixel tests:\n");
    RUN_TEST(screen_clear_black);
    RUN_TEST(screen_clear_color);
    RUN_TEST(screen_plot_physical_pixel);
    RUN_TEST(screen_plot_logical_pixel);
    RUN_TEST(screen_plot_corners);
    RUN_TEST(screen_plot_out_of_bounds);
    RUN_TEST(screen_get_out_of_bounds);

    std::printf("\nScreenBuffer single pixel tests:\n");
    RUN_TEST(screen_single_pixel_not_block);
    RUN_TEST(screen_adjacent_logical_pixels);

    std::printf("\n========================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
