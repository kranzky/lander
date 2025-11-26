#include <cstdio>
#include <cstdlib>
#include "../src/palette.h"

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
// VIDC Color Conversion Tests
// =============================================================================

TEST(vidc_black) {
    // VIDC 0x00 should be black
    Color c = vidc256ToColor(0x00);
    ASSERT_EQ(c.r, 0);
    ASSERT_EQ(c.g, 0);
    ASSERT_EQ(c.b, 0);
}

TEST(vidc_white) {
    // VIDC 0xFF should be white
    Color c = vidc256ToColor(0xFF);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 255);
    ASSERT_EQ(c.b, 255);
}

TEST(vidc_pure_red) {
    // Pure red: R=15, G=0, B=0
    // Bits: R3=1 (bit4), R2=1 (bit2), tint=3 (bits0-1)
    // VIDC = 0001 0111 = 0x17
    uint8_t vidc = buildVidcColor(15, 0, 0);
    Color c = vidc256ToColor(vidc);
    ASSERT_EQ(c.r, 255);
    ASSERT_EQ(c.g, 51);   // tint bits affect all channels
    ASSERT_EQ(c.b, 51);
}

TEST(vidc_pure_green) {
    // Pure green: R=0, G=15, B=0
    uint8_t vidc = buildVidcColor(0, 15, 0);
    Color c = vidc256ToColor(vidc);
    ASSERT_EQ(c.g, 255);
}

TEST(vidc_pure_blue) {
    // Pure blue: R=0, G=0, B=15
    uint8_t vidc = buildVidcColor(0, 0, 15);
    Color c = vidc256ToColor(vidc);
    ASSERT_EQ(c.b, 255);
}

TEST(vidc_fuel_bar_color) {
    // Fuel bar is &37 from original
    Color c = GameColors::fuelBar();
    // &37 = 0011 0111 binary
    // Bit 7 (B3) = 0, Bit 6 (G3) = 0, Bit 5 (G2) = 1, Bit 4 (R3) = 1
    // Bit 3 (B2) = 0, Bit 2 (R2) = 1, Bit 1 (tint) = 1, Bit 0 (tint) = 1
    // R = tint(3) | r2(4) | r3(8) = 15
    // G = tint(3) | g2(4) = 7
    // B = tint(3) = 3
    ASSERT_EQ(c.r, 15 * 17);  // 255
    ASSERT_EQ(c.g, 7 * 17);   // 119
    ASSERT_EQ(c.b, 3 * 17);   // 51
}

TEST(vidc_grey_levels) {
    // Test grey levels (equal R, G, B)
    for (int level = 0; level <= 15; level++) {
        Color c = GameColors::smokeGrey(level);
        // All channels should be equal
        ASSERT_EQ(c.r, c.g);
        ASSERT_EQ(c.g, c.b);
        // Value should be level * 17
        ASSERT_EQ(c.r, level * 17);
    }
}

TEST(build_vidc_roundtrip) {
    // Building and decoding should give consistent results
    // Note: Due to the VIDC format, not all 4096 RGB combinations
    // can be represented, but building and decoding should be consistent
    for (int r = 0; r < 16; r += 4) {
        for (int g = 0; g < 16; g += 4) {
            for (int b = 0; b < 16; b += 4) {
                uint8_t vidc = buildVidcColor(r, g, b);
                Color c = vidc256ToColor(vidc);
                // The decoded color should be valid (non-zero for non-black)
                if (r > 0 || g > 0 || b > 0) {
                    ASSERT(c.r > 0 || c.g > 0 || c.b > 0);
                }
            }
        }
    }
}

// =============================================================================
// Landscape Color Tests
// =============================================================================

TEST(landscape_land_color) {
    // Basic land at middle distance
    Color c = getLandscapeTileColor(0x08, 5, 0, TileType::Land);
    // Should be greenish (green channel dominant)
    ASSERT(c.g >= c.r);
    ASSERT(c.g > c.b);
}

TEST(landscape_sea_color) {
    // Sea should be blue
    Color c = getLandscapeTileColor(0, 5, 0, TileType::Sea);
    ASSERT(c.b > c.r);
    ASSERT(c.b > c.g);
}

TEST(landscape_launchpad_color) {
    // Launchpad should be grey (equal R, G, B)
    Color c = getLandscapeTileColor(0, 5, 0, TileType::Launchpad);
    ASSERT_EQ(c.r, c.g);
    ASSERT_EQ(c.g, c.b);
}

TEST(landscape_distance_brightness) {
    // Tiles nearer (higher row) should be brighter
    Color far_tile = getLandscapeTileColor(0x08, 1, 0, TileType::Land);
    Color near_tile = getLandscapeTileColor(0x08, 10, 0, TileType::Land);

    // Near tile should be brighter (higher RGB values)
    ASSERT(near_tile.r >= far_tile.r);
    ASSERT(near_tile.g >= far_tile.g);
}

TEST(landscape_slope_brightness) {
    // Left-facing tiles (positive slope) should be brighter
    Color flat_tile = getLandscapeTileColor(0x08, 5, 0, TileType::Land);
    Color slope_tile = getLandscapeTileColor(0x08, 5, 0x00400000, TileType::Land);

    // Sloped tile should be brighter
    ASSERT(slope_tile.g >= flat_tile.g);
}

TEST(landscape_altitude_variation) {
    // Different altitudes should give different colors
    Color low = getLandscapeTileColor(0x00, 5, 0, TileType::Land);
    Color high = getLandscapeTileColor(0x0C, 5, 0, TileType::Land);

    // Colors should be different due to altitude bits affecting R/G
    ASSERT(low.r != high.r || low.g != high.g);
}

// =============================================================================
// Object Color Tests
// =============================================================================

TEST(object_color_ship_nose) {
    // Ship nose color from original: &080
    Color c = objectColorToRGB(0x080, 0);
    // &080 = R=0, G=8, B=0 (dark green)
    ASSERT(c.g > c.r);
    ASSERT(c.g > c.b);
}

TEST(object_color_ship_body) {
    // Ship body color: &040
    Color c = objectColorToRGB(0x040, 0);
    // &040 = R=0, G=4, B=0 (darker green)
    ASSERT(c.g >= c.r);
}

TEST(object_color_ship_cockpit) {
    // Ship cockpit: &C80
    Color c = objectColorToRGB(0xC80, 0);
    // &C80 = R=12, G=8, B=0 (orange/red)
    ASSERT(c.r > c.b);
}

TEST(object_color_brightness) {
    // Adding brightness should increase values
    Color dark = objectColorToRGB(0x040, 0);
    Color bright = objectColorToRGB(0x040, 8);

    ASSERT(bright.r >= dark.r);
    ASSERT(bright.g >= dark.g);
    ASSERT(bright.b >= dark.b);
}

TEST(object_color_clamp) {
    // High brightness shouldn't overflow
    Color c = objectColorToRGB(0xFFF, 15);  // Max color + max brightness
    ASSERT(c.r <= 255);
    ASSERT(c.g <= 255);
    ASSERT(c.b <= 255);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::printf("Palette Unit Tests\n");
    std::printf("==================\n\n");

    std::printf("VIDC color conversion tests:\n");
    RUN_TEST(vidc_black);
    RUN_TEST(vidc_white);
    RUN_TEST(vidc_pure_red);
    RUN_TEST(vidc_pure_green);
    RUN_TEST(vidc_pure_blue);
    RUN_TEST(vidc_fuel_bar_color);
    RUN_TEST(vidc_grey_levels);
    RUN_TEST(build_vidc_roundtrip);

    std::printf("\nLandscape color tests:\n");
    RUN_TEST(landscape_land_color);
    RUN_TEST(landscape_sea_color);
    RUN_TEST(landscape_launchpad_color);
    RUN_TEST(landscape_distance_brightness);
    RUN_TEST(landscape_slope_brightness);
    RUN_TEST(landscape_altitude_variation);

    std::printf("\nObject color tests:\n");
    RUN_TEST(object_color_ship_nose);
    RUN_TEST(object_color_ship_body);
    RUN_TEST(object_color_ship_cockpit);
    RUN_TEST(object_color_brightness);
    RUN_TEST(object_color_clamp);

    std::printf("\n==================\n");
    std::printf("Tests: %d total, %d passed, %d failed\n",
                testsRun, testsPassed, testsFailed);

    return testsFailed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
