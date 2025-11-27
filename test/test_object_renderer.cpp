#include "object_renderer.h"
#include "object3d.h"
#include "math3d.h"
#include "screen.h"
#include <cstdio>
#include <cstdlib>

// Test helper
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(condition, name) do { \
    if (condition) { \
        printf("  PASS: %s\n", name); \
        testsPassed++; \
    } else { \
        printf("  FAIL: %s\n", name); \
        testsFailed++; \
    } \
} while(0)

void testLightingCalculation() {
    printf("Lighting Tests:\n");

    // Test 1: Normal pointing straight up (negative Y) should be bright
    Vec3 upNormal;
    upNormal.x = Fixed::fromInt(0);
    upNormal.y = Fixed::fromRaw(-0x70000000);  // Strongly negative Y
    upNormal.z = Fixed::fromInt(0);

    Color upColor = calculateLitColor(0x888, upNormal);  // Gray base
    TEST(upColor.r > 136, "Upward normal increases brightness");

    // Test 2: Normal pointing straight down (positive Y) should be dark
    Vec3 downNormal;
    downNormal.x = Fixed::fromInt(0);
    downNormal.y = Fixed::fromRaw(0x70000000);  // Strongly positive Y
    downNormal.z = Fixed::fromInt(0);

    Color downColor = calculateLitColor(0x888, downNormal);
    TEST(downColor.r <= 136, "Downward normal doesn't increase brightness");

    // Test 3: Red base color should stay red
    Color redLit = calculateLitColor(0xF00, upNormal);
    TEST(redLit.r > redLit.g && redLit.r > redLit.b, "Red base stays red");

    // Test 4: Green base color should stay green
    Color greenLit = calculateLitColor(0x0F0, upNormal);
    TEST(greenLit.g > greenLit.r && greenLit.g > greenLit.b, "Green base stays green");
}

void testObjectRendering() {
    printf("\nObject Rendering Tests:\n");

    // Create screen buffer
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Position: 5 tiles in front of camera (positive Z), centered
    Vec3 position;
    position.x = Fixed::fromInt(0);
    position.y = Fixed::fromInt(0);
    position.z = Fixed::fromRaw(0x05000000);  // 5 tiles

    // Identity rotation (no rotation)
    Mat3x3 rotation = Mat3x3::identity();

    // Draw the ship - this should complete without crashing
    drawObject(shipBlueprint, position, rotation, screen);

    // Basic sanity check - drawing completed
    TEST(true, "Ship rendering completed without crash");

    // Save a screenshot for visual verification
    screen.savePNG("test_ship_render.png");
    printf("  Saved test_ship_render.png for visual verification\n");
}

void testRotatedShip() {
    printf("\nRotated Ship Test:\n");

    // Create screen buffer
    ScreenBuffer screen;
    screen.clear(Color::black());

    // Position: 5 tiles in front of camera
    Vec3 position;
    position.x = Fixed::fromInt(0);
    position.y = Fixed::fromInt(0);
    position.z = Fixed::fromRaw(0x05000000);

    // Calculate rotation matrix with some pitch and yaw
    int32_t pitch = 0x10000000;    // ~22.5 degrees pitch
    int32_t direction = 0x20000000; // ~45 degrees yaw
    Mat3x3 rotation = calculateRotationMatrix(pitch, direction);

    // Draw the rotated ship
    drawObject(shipBlueprint, position, rotation, screen);

    // Basic sanity check - drawing completed
    TEST(true, "Rotated ship rendering completed without crash");

    // Save a screenshot for visual verification
    screen.savePNG("test_ship_rotated.png");
    printf("  Saved test_ship_rotated.png for visual verification\n");
}

int main() {
    printf("=== Object Renderer Tests ===\n\n");

    testLightingCalculation();
    testObjectRendering();
    testRotatedShip();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", testsPassed);
    printf("Failed: %d\n", testsFailed);

    return testsFailed > 0 ? 1 : 0;
}
