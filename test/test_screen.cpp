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
    // These are now functions that return current scale, default is 4x
    ASSERT_EQ(ScreenBuffer::PHYSICAL_WIDTH(), 1280);
    ASSERT_EQ(ScreenBuffer::PHYSICAL_HEIGHT(), 1024);
    ASSERT_EQ(ScreenBuffer::PIXEL_SCALE(), 4);
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
// Triangle Rasterization Tests
// =============================================================================

// Helper to check if a pixel is set (non-black)
static bool pixelSet(const ScreenBuffer& screen, int x, int y) {
    Color c = screen.getPhysicalPixel(x, y);
    return c.r != 0 || c.g != 0 || c.b != 0;
}

TEST(triangle_basic) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw a simple right triangle
    screen.drawTriangle(100, 100, 150, 100, 100, 150, Color::red());

    // Check that corners are filled
    ASSERT(pixelSet(screen, 100, 100));  // Top-left
    ASSERT(pixelSet(screen, 149, 100));  // Top-right (near)
    ASSERT(pixelSet(screen, 100, 149));  // Bottom-left (near)

    // Check that a point clearly outside is not filled
    ASSERT(!pixelSet(screen, 150, 150));  // Outside bottom-right
}

TEST(triangle_flat_top) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Flat-top triangle (two vertices at same y)
    screen.drawTriangle(100, 100, 200, 100, 150, 200, Color::green());

    // Check that top edge is filled
    ASSERT(pixelSet(screen, 100, 100));
    ASSERT(pixelSet(screen, 150, 100));
    ASSERT(pixelSet(screen, 199, 100));

    // Check bottom vertex
    ASSERT(pixelSet(screen, 150, 199));

    // Check center is filled
    ASSERT(pixelSet(screen, 150, 150));
}

TEST(triangle_flat_bottom) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Flat-bottom triangle (two vertices at same y)
    screen.drawTriangle(150, 100, 100, 200, 200, 200, Color::blue());

    // Check top vertex
    ASSERT(pixelSet(screen, 150, 100));

    // Check bottom edge is filled
    ASSERT(pixelSet(screen, 100, 199));
    ASSERT(pixelSet(screen, 150, 199));
    ASSERT(pixelSet(screen, 199, 199));

    // Check center is filled
    ASSERT(pixelSet(screen, 150, 150));
}

TEST(triangle_vertex_order_independence) {
    ScreenBuffer screen1, screen2, screen3;
    screen1.clear(Color::black());
    screen2.clear(Color::black());
    screen3.clear(Color::black());

    // Draw the same triangle with different vertex orders
    screen1.drawTriangle(100, 100, 200, 150, 150, 200, Color::red());
    screen2.drawTriangle(200, 150, 150, 200, 100, 100, Color::red());
    screen3.drawTriangle(150, 200, 100, 100, 200, 150, Color::red());

    // All should produce the same result - check several points
    int testPoints[][2] = {{100, 100}, {150, 150}, {175, 150}, {125, 175}};

    for (auto& pt : testPoints) {
        bool s1 = pixelSet(screen1, pt[0], pt[1]);
        bool s2 = pixelSet(screen2, pt[0], pt[1]);
        bool s3 = pixelSet(screen3, pt[0], pt[1]);
        ASSERT(s1 == s2);
        ASSERT(s2 == s3);
    }
}

TEST(triangle_degenerate_horizontal) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // All three points on same horizontal line
    screen.drawTriangle(100, 200, 200, 200, 150, 200, Color::yellow());

    // Should draw a horizontal line
    ASSERT(pixelSet(screen, 100, 200));
    ASSERT(pixelSet(screen, 150, 200));
    ASSERT(pixelSet(screen, 200, 200));

    // No pixels above or below
    ASSERT(!pixelSet(screen, 150, 199));
    ASSERT(!pixelSet(screen, 150, 201));
}

TEST(triangle_degenerate_point) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // All three points at same location
    screen.drawTriangle(150, 150, 150, 150, 150, 150, Color::cyan());

    // Should draw at least one pixel at that point
    ASSERT(pixelSet(screen, 150, 150));
}

TEST(triangle_large) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Large triangle spanning significant portion of screen
    screen.drawTriangle(100, 100, 900, 100, 500, 800, Color::magenta());

    // Check vertices are filled
    ASSERT(pixelSet(screen, 100, 100));
    ASSERT(pixelSet(screen, 899, 100));
    ASSERT(pixelSet(screen, 500, 799));

    // Check center is filled
    ASSERT(pixelSet(screen, 500, 400));

    // Check outside is not filled
    ASSERT(!pixelSet(screen, 50, 400));
    ASSERT(!pixelSet(screen, 950, 400));
}

TEST(triangle_thin_tall) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Very thin, tall triangle
    screen.drawTriangle(500, 100, 510, 100, 505, 500, Color::red());

    // Should have filled pixels at all y levels
    ASSERT(pixelSet(screen, 505, 200));
    ASSERT(pixelSet(screen, 505, 300));
    ASSERT(pixelSet(screen, 505, 400));
}

TEST(triangle_thin_wide) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Very wide, short triangle
    screen.drawTriangle(100, 500, 900, 500, 500, 510, Color::green());

    // Should have filled pixels spanning horizontally
    ASSERT(pixelSet(screen, 200, 502));
    ASSERT(pixelSet(screen, 500, 505));
    ASSERT(pixelSet(screen, 700, 502));
}

TEST(triangle_clipping) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Triangle that extends off-screen
    // The drawHorizontalLine already clips, so this should work
    screen.drawTriangle(-100, 100, 200, 100, 50, 300, Color::blue());

    // Visible parts should be filled
    ASSERT(pixelSet(screen, 0, 100));  // Clipped left edge
    ASSERT(pixelSet(screen, 50, 200));

    // Check that no crash occurred and buffer is valid
    ASSERT(screen.inPhysicalBounds(0, 0));
}

// =============================================================================
// Horizontal Line Tests
// =============================================================================

TEST(hline_basic) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw a horizontal line from x=100 to x=110 at y=50
    screen.drawHorizontalLine(100, 110, 50, Color::red());

    // Check all pixels in the line
    for (int x = 100; x <= 110; x++) {
        Color c = screen.getPhysicalPixel(x, 50);
        ASSERT_EQ(c.r, 255);
        ASSERT_EQ(c.g, 0);
        ASSERT_EQ(c.b, 0);
    }

    // Check pixels just outside the line
    ASSERT_EQ(screen.getPhysicalPixel(99, 50).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(111, 50).r, 0);
}

TEST(hline_reversed_endpoints) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw with x2 < x1 (should still work)
    screen.drawHorizontalLine(110, 100, 50, Color::green());

    // Check all pixels in the line
    for (int x = 100; x <= 110; x++) {
        Color c = screen.getPhysicalPixel(x, 50);
        ASSERT_EQ(c.g, 255);
    }
}

TEST(hline_single_pixel) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw a single pixel line
    screen.drawHorizontalLine(200, 200, 100, Color::blue());

    Color c = screen.getPhysicalPixel(200, 100);
    ASSERT_EQ(c.b, 255);

    // Adjacent pixels should be black
    ASSERT_EQ(screen.getPhysicalPixel(199, 100).b, 0);
    ASSERT_EQ(screen.getPhysicalPixel(201, 100).b, 0);
}

TEST(hline_clip_left) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line that starts off-screen to the left
    screen.drawHorizontalLine(-50, 50, 100, Color::red());

    // Should be clipped to start at x=0
    ASSERT_EQ(screen.getPhysicalPixel(0, 100).r, 255);
    ASSERT_EQ(screen.getPhysicalPixel(50, 100).r, 255);
}

TEST(hline_clip_right) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line that extends off-screen to the right
    screen.drawHorizontalLine(1250, 1300, 100, Color::green());

    // Should be clipped to end at x=1279
    ASSERT_EQ(screen.getPhysicalPixel(1250, 100).g, 255);
    ASSERT_EQ(screen.getPhysicalPixel(1279, 100).g, 255);
}

TEST(hline_clip_both) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line that spans entire screen and beyond
    screen.drawHorizontalLine(-100, 1400, 200, Color::blue());

    // Should fill entire row
    ASSERT_EQ(screen.getPhysicalPixel(0, 200).b, 255);
    ASSERT_EQ(screen.getPhysicalPixel(640, 200).b, 255);
    ASSERT_EQ(screen.getPhysicalPixel(1279, 200).b, 255);
}

TEST(hline_entirely_off_left) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line entirely off-screen to the left
    screen.drawHorizontalLine(-100, -10, 100, Color::red());

    // Nothing should be drawn
    ASSERT_EQ(screen.getPhysicalPixel(0, 100).r, 0);
}

TEST(hline_entirely_off_right) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line entirely off-screen to the right
    screen.drawHorizontalLine(1300, 1400, 100, Color::red());

    // Nothing should be drawn
    ASSERT_EQ(screen.getPhysicalPixel(1279, 100).r, 0);
}

TEST(hline_off_top) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line off-screen above
    screen.drawHorizontalLine(100, 200, -10, Color::red());

    // Nothing should be drawn
    ASSERT_EQ(screen.getPhysicalPixel(100, 0).r, 0);
}

TEST(hline_off_bottom) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw line off-screen below
    screen.drawHorizontalLine(100, 200, 1050, Color::red());

    // Nothing should be drawn
    ASSERT_EQ(screen.getPhysicalPixel(100, 1023).r, 0);
}

TEST(hline_screen_edges) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw at top edge
    screen.drawHorizontalLine(0, 100, 0, Color::red());
    ASSERT_EQ(screen.getPhysicalPixel(50, 0).r, 255);

    // Draw at bottom edge
    screen.drawHorizontalLine(0, 100, 1023, Color::green());
    ASSERT_EQ(screen.getPhysicalPixel(50, 1023).g, 255);

    // Draw at left edge (single column)
    screen.drawHorizontalLine(0, 0, 500, Color::blue());
    ASSERT_EQ(screen.getPhysicalPixel(0, 500).b, 255);

    // Draw at right edge (single column)
    screen.drawHorizontalLine(1279, 1279, 500, Color::yellow());
    ASSERT_EQ(screen.getPhysicalPixel(1279, 500).r, 255);
    ASSERT_EQ(screen.getPhysicalPixel(1279, 500).g, 255);
}

TEST(hline_full_row) {
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw entire row
    screen.drawHorizontalLine(0, 1279, 512, Color::white());

    // Check several points
    ASSERT_EQ(screen.getPhysicalPixel(0, 512).r, 255);
    ASSERT_EQ(screen.getPhysicalPixel(640, 512).r, 255);
    ASSERT_EQ(screen.getPhysicalPixel(1279, 512).r, 255);

    // Check adjacent rows are still black
    ASSERT_EQ(screen.getPhysicalPixel(640, 511).r, 0);
    ASSERT_EQ(screen.getPhysicalPixel(640, 513).r, 0);
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

    std::printf("\nTriangle rasterization tests:\n");
    RUN_TEST(triangle_basic);
    RUN_TEST(triangle_flat_top);
    RUN_TEST(triangle_flat_bottom);
    RUN_TEST(triangle_vertex_order_independence);
    RUN_TEST(triangle_degenerate_horizontal);
    RUN_TEST(triangle_degenerate_point);
    RUN_TEST(triangle_large);
    RUN_TEST(triangle_thin_tall);
    RUN_TEST(triangle_thin_wide);
    RUN_TEST(triangle_clipping);

    std::printf("\nHorizontal line tests:\n");
    RUN_TEST(hline_basic);
    RUN_TEST(hline_reversed_endpoints);
    RUN_TEST(hline_single_pixel);
    RUN_TEST(hline_clip_left);
    RUN_TEST(hline_clip_right);
    RUN_TEST(hline_clip_both);
    RUN_TEST(hline_entirely_off_left);
    RUN_TEST(hline_entirely_off_right);
    RUN_TEST(hline_off_top);
    RUN_TEST(hline_off_bottom);
    RUN_TEST(hline_screen_edges);
    RUN_TEST(hline_full_row);

    std::printf("\n========================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
