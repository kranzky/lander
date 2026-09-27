// =============================================================================
// Projection Tests
// =============================================================================
//
// Tests for the 3D projection system, verifying:
// 1. Basic perspective projection formula
// 2. Screen coordinate scaling
// 3. Visibility and on-screen detection
// 4. Edge cases (behind camera, at camera, far away)
// 5. Visual output verification
//
// =============================================================================

#include <cstdio>
#include <cmath>
#include "projection.h"
#include "screen.h"
#include "camera.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %s... ", name); \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        printf("PASS\n"); \
    } while(0)

#define FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while(0)

// =============================================================================
// Basic Projection Tests
// =============================================================================

void test_center_projection() {
    TEST("Point at center projects to screen center");

    // A point directly in front of the camera at z=1 tile should project
    // to the screen center
    Fixed x = Fixed::fromInt(0);
    Fixed y = Fixed::fromInt(0);
    Fixed z = Fixed::fromInt(1);  // 1 tile away

    ProjectedVertex result = projectVertex(x, y, z);

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X(),
           "X should be at center");
    ASSERT(result.screenY == ProjectionConstants::CENTER_Y(),
           "Y should be at center");

    PASS();
}

// Expected physical screen offset from the centre for a point at (a, z) tiles
static int expectedOffset(double a, double z) {
    int logical = static_cast<int>(a * ProjectionConstants::FOCAL_LENGTH / z);
    return logical * ProjectionConstants::SCALE();
}

void test_right_offset() {
    TEST("Point to the right projects right of center");

    // x=1, z=16 tiles: 256/16 = 16 logical pixels right of center
    ProjectedVertex result = projectVertex(Fixed::fromInt(1), Fixed::fromInt(0), Fixed::fromInt(16));

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() + expectedOffset(1, 16),
           "X should be 16 logical pixels right of center");

    PASS();
}

void test_left_offset() {
    TEST("Point to the left projects left of center");

    ProjectedVertex result = projectVertex(Fixed::fromInt(-1), Fixed::fromInt(0), Fixed::fromInt(16));

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() - expectedOffset(1, 16),
           "X should be 16 logical pixels left of center");

    PASS();
}

void test_down_offset() {
    TEST("Point below projects below center");

    // Positive Y = down in original coords
    ProjectedVertex result = projectVertex(Fixed::fromInt(0), Fixed::fromInt(1), Fixed::fromInt(16));

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenY == ProjectionConstants::CENTER_Y() + expectedOffset(1, 16),
           "Y should be 16 logical pixels below center");

    PASS();
}

void test_perspective_scaling() {
    TEST("Points further away appear smaller (perspective)");

    Fixed x = Fixed::fromInt(2);
    Fixed y = Fixed::fromInt(0);

    ProjectedVertex near = projectVertex(x, y, Fixed::fromInt(16));
    ProjectedVertex far = projectVertex(x, y, Fixed::fromInt(32));

    ASSERT(near.visible && far.visible, "Both should be visible");

    int nearOffset = near.screenX - ProjectionConstants::CENTER_X();
    int farOffset = far.screenX - ProjectionConstants::CENTER_X();

    ASSERT(nearOffset == expectedOffset(2, 16), "Near point offset");
    ASSERT(farOffset == expectedOffset(2, 32), "Far point offset");
    ASSERT(nearOffset == 2 * farOffset, "Twice as far should be half the offset");

    PASS();
}

// =============================================================================
// Visibility Tests
// =============================================================================

void test_behind_camera() {
    TEST("Points behind camera are not visible");

    // Negative z = behind camera
    Fixed x = Fixed::fromInt(0);
    Fixed y = Fixed::fromInt(0);
    Fixed z = Fixed::fromInt(-1);

    ProjectedVertex result = projectVertex(x, y, z);

    ASSERT(!result.visible, "Should not be visible");

    PASS();
}

void test_at_camera() {
    TEST("Points at camera (z=0) are not visible");

    Fixed x = Fixed::fromInt(0);
    Fixed y = Fixed::fromInt(0);
    Fixed z = Fixed::fromInt(0);

    ProjectedVertex result = projectVertex(x, y, z);

    ASSERT(!result.visible, "Should not be visible (would divide by zero)");

    PASS();
}

void test_very_close() {
    TEST("Very close points are visible");

    // Point at z = 0.001 tiles (small but positive)
    Fixed x = Fixed::fromInt(0);
    Fixed y = Fixed::fromInt(0);
    Fixed z = Fixed::fromRaw(0x00004000);  // About 1/4096 of a tile

    ProjectedVertex result = projectVertex(x, y, z);

    ASSERT(result.visible, "Very close points should be visible");

    PASS();
}

// =============================================================================
// On-Screen Tests
// =============================================================================

void test_on_screen_center() {
    TEST("Center point is on screen");

    ProjectedVertex result = projectVertex(
        Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(1));

    ASSERT(result.onScreen, "Center should be on screen");

    PASS();
}

void test_max_left_offset() {
    TEST("Points beyond the left edge are off screen");

    // x=-8, z=8: 256 logical pixels left of center, beyond the 160 available
    ProjectedVertex result = projectVertex(Fixed::fromInt(-8), Fixed::fromInt(0), Fixed::fromInt(8));

    ASSERT(result.visible, "Should be visible");
    ASSERT(!result.onScreen, "Should be off screen");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() - expectedOffset(8, 8),
           "Should be at expected position");

    PASS();
}

void test_max_right_offset() {
    TEST("Points beyond the right edge are off screen");

    ProjectedVertex result = projectVertex(Fixed::fromInt(8), Fixed::fromInt(0), Fixed::fromInt(8));

    ASSERT(result.visible, "Should be visible");
    ASSERT(!result.onScreen, "Should be off screen");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() + expectedOffset(8, 8),
           "Should be at expected position");

    PASS();
}

// =============================================================================
// Fixed-Point Precision Tests
// =============================================================================

void test_fractional_coordinates() {
    TEST("Fractional coordinates work correctly");

    // x=0.5, z=16: 8 logical pixels, so sub-tile offsets aren't lost
    Fixed x = Fixed::fromRaw(0x00800000);  // 0.5 in 8.24
    ProjectedVertex result = projectVertex(x, Fixed::fromInt(0), Fixed::fromInt(16));

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() + expectedOffset(0.5, 16),
           "Half-tile offset should give 8 logical pixels");

    PASS();
}

void test_tile_sized_offset() {
    TEST("TILE_SIZE unit gives correct projection");

    // x = 1/16 tile at z = 1 tile: 16 logical pixels
    Fixed x = Fixed::fromRaw(GameConstants::TILE_SIZE.raw / 16);
    ProjectedVertex result = projectVertex(x, Fixed::fromInt(0), GameConstants::TILE_SIZE);

    ASSERT(result.visible, "Should be visible");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() + expectedOffset(1.0 / 16, 1),
           "1/16 tile at 1 tile should give 16 logical pixels");

    PASS();
}

void test_game_coordinate_range() {
    TEST("Game coordinate ranges project correctly");

    // A corner 2.5 tiles right and 1 tile down, 5 tiles ahead
    Fixed x = Fixed::fromRaw(0x02800000);  // 2.5 tiles
    Fixed y = Fixed::fromRaw(0x01000000);  // 1 tile (altitude)
    Fixed z = Fixed::fromRaw(0x05000000);  // 5 tiles

    ProjectedVertex result = projectVertex(x, y, z);

    ASSERT(result.visible && result.onScreen, "Should be visible and on screen");
    ASSERT(result.screenX == ProjectionConstants::CENTER_X() + expectedOffset(2.5, 5),
           "X should be 128 logical pixels right of center");
    ASSERT(result.screenY == ProjectionConstants::CENTER_Y() + expectedOffset(1, 5),
           "Y should be 51 logical pixels below center");

    PASS();
}

// =============================================================================
// Vec3 Overload Test
// =============================================================================

void test_vec3_overload() {
    TEST("Vec3 overload matches component version");

    Fixed x = Fixed::fromInt(5);
    Fixed y = Fixed::fromInt(3);
    Fixed z = Fixed::fromInt(10);

    ProjectedVertex result1 = projectVertex(x, y, z);
    ProjectedVertex result2 = projectVertex(Vec3(x, y, z));

    ASSERT(result1.screenX == result2.screenX, "X should match");
    ASSERT(result1.screenY == result2.screenY, "Y should match");
    ASSERT(result1.visible == result2.visible, "Visible should match");
    ASSERT(result1.onScreen == result2.onScreen, "OnScreen should match");

    PASS();
}

// =============================================================================
// Camera Test
// =============================================================================

void test_camera_transform() {
    TEST("Camera worldToCamera transform works");

    Camera camera;
    camera.setPosition(
        Fixed::fromInt(10),
        Fixed::fromInt(5),
        Fixed::fromInt(20)
    );

    Vec3 worldPos(
        Fixed::fromInt(15),
        Fixed::fromInt(8),
        Fixed::fromInt(30)
    );

    Vec3 cameraRelative = camera.worldToCamera(worldPos);

    ASSERT(cameraRelative.x.toInt() == 5, "X offset should be 5");
    ASSERT(cameraRelative.y.toInt() == 3, "Y offset should be 3");
    ASSERT(cameraRelative.z.toInt() == 10, "Z offset should be 10");

    PASS();
}

// =============================================================================
// Visual Test - Generate test image
// =============================================================================

void test_visual_output() {
    TEST("Visual projection test");

    ScreenBuffer screen;
    screen.clear(Color::black());

    // Draw a grid of points at different depths to verify perspective
    // Start from far (z=20 tiles) and come closer (z=2 tiles)

    Color depthColors[] = {
        Color(64, 64, 64),      // Far: dark gray
        Color(96, 96, 96),
        Color(128, 128, 128),
        Color(160, 160, 160),
        Color(192, 192, 192),
        Color(224, 224, 224),
        Color::white(),         // Near: white
    };

    // Draw converging lines to show perspective
    for (int depth = 0; depth < 7; depth++) {
        Fixed z = Fixed::fromInt(20 - depth * 3);  // 20, 17, 14, 11, 8, 5, 2 tiles

        // Draw points in a horizontal line at this depth
        for (int xOff = -6; xOff <= 6; xOff++) {
            Fixed x = Fixed::fromInt(xOff);
            Fixed y = Fixed::fromInt(0);

            ProjectedVertex v = projectVertex(x, y, z);
            if (v.visible && v.onScreen) {
                // Draw a small cross at each point
                for (int dy = -2; dy <= 2; dy++) {
                    screen.plotPhysicalPixel(v.screenX, v.screenY + dy, depthColors[depth]);
                }
                for (int dx = -2; dx <= 2; dx++) {
                    screen.plotPhysicalPixel(v.screenX + dx, v.screenY, depthColors[depth]);
                }
            }
        }
    }

    // Draw colored triangles at different depths to show perspective
    // Blue triangle far away
    {
        Vec3 v0(Fixed::fromInt(-3), Fixed::fromInt(-2), Fixed::fromInt(15));
        Vec3 v1(Fixed::fromInt(3), Fixed::fromInt(-2), Fixed::fromInt(15));
        Vec3 v2(Fixed::fromInt(0), Fixed::fromInt(2), Fixed::fromInt(15));

        ProjectedVertex p0 = projectVertex(v0);
        ProjectedVertex p1 = projectVertex(v1);
        ProjectedVertex p2 = projectVertex(v2);

        if (p0.visible && p1.visible && p2.visible) {
            screen.drawTriangle(p0.screenX, p0.screenY,
                              p1.screenX, p1.screenY,
                              p2.screenX, p2.screenY, Color::blue());
        }
    }

    // Green triangle medium distance
    {
        Vec3 v0(Fixed::fromInt(-4), Fixed::fromInt(-3), Fixed::fromInt(10));
        Vec3 v1(Fixed::fromInt(4), Fixed::fromInt(-3), Fixed::fromInt(10));
        Vec3 v2(Fixed::fromInt(0), Fixed::fromInt(3), Fixed::fromInt(10));

        ProjectedVertex p0 = projectVertex(v0);
        ProjectedVertex p1 = projectVertex(v1);
        ProjectedVertex p2 = projectVertex(v2);

        if (p0.visible && p1.visible && p2.visible) {
            screen.drawTriangle(p0.screenX, p0.screenY,
                              p1.screenX, p1.screenY,
                              p2.screenX, p2.screenY, Color::green());
        }
    }

    // Red triangle close
    {
        Vec3 v0(Fixed::fromInt(-5), Fixed::fromInt(-4), Fixed::fromInt(5));
        Vec3 v1(Fixed::fromInt(5), Fixed::fromInt(-4), Fixed::fromInt(5));
        Vec3 v2(Fixed::fromInt(0), Fixed::fromInt(4), Fixed::fromInt(5));

        ProjectedVertex p0 = projectVertex(v0);
        ProjectedVertex p1 = projectVertex(v1);
        ProjectedVertex p2 = projectVertex(v2);

        if (p0.visible && p1.visible && p2.visible) {
            screen.drawTriangle(p0.screenX, p0.screenY,
                              p1.screenX, p1.screenY,
                              p2.screenX, p2.screenY, Color::red());
        }
    }

    // Save the visual test
    if (screen.savePNG("projection_test.png")) {
        printf("(saved projection_test.png) ");
    }

    PASS();
}

// =============================================================================
// Main
// =============================================================================

int main() {
    printf("=== Projection Tests ===\n\n");

    printf("Basic projection:\n");
    test_center_projection();
    test_right_offset();
    test_left_offset();
    test_down_offset();
    test_perspective_scaling();

    printf("\nVisibility:\n");
    test_behind_camera();
    test_at_camera();
    test_very_close();

    printf("\nOn-screen detection:\n");
    test_on_screen_center();
    test_max_left_offset();
    test_max_right_offset();

    printf("\nFixed-point precision:\n");
    test_fractional_coordinates();
    test_tile_sized_offset();
    test_game_coordinate_range();

    printf("\nOverloads and utilities:\n");
    test_vec3_overload();
    test_camera_transform();

    printf("\nVisual tests:\n");
    test_visual_output();

    printf("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
}
