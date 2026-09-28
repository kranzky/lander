// test_player.cpp
// Tests that ship controls and physics match the original's once-per-frame
// rules, spread over our 8 physics steps per original (15fps) frame

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <SDL.h>
#include "player.h"

static int testCount = 0;
static int passCount = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) do { \
    testCount++; \
    printf("  %s... ", #name); \
    test_##name(); \
    passCount++; \
    printf("PASS\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

constexpr int STEPS_PER_FRAME = 8;
constexpr double DEGREES_PER_UNIT = 360.0 / 4294967296.0;

// The original's orientation update for one frame (Lander.arm lines 1805-1865):
// move half the gap to the target, with the gap capped at &30000000
static int32_t originalApproach(int32_t current, int32_t target) {
    int32_t gap = static_cast<int32_t>(static_cast<uint32_t>(current) - static_cast<uint32_t>(target));
    if (gap >= 0) {
        gap = std::min(gap, 0x30000000);
    } else {
        gap = std::max(gap, -0x30000001);
    }
    return static_cast<int32_t>(static_cast<uint32_t>(current) - static_cast<uint32_t>(gap >> 1));
}

static double angleDifference(int32_t a, int32_t b) {
    return std::fabs(static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)) *
                     DEGREES_PER_UNIT);
}

// Point the emulated RISC OS pointer so the original's mouse coordinates are
// (x, y), for x up to +511 (the pointer's dead zone lies beyond)
static void setMouse(Player& player, int x, int y, uint32_t buttons = 0) {
    player.updateInput(x + 512, 512 - y, buttons);
}

// Settle the ship's orientation on the current mouse position
static void settle(Player& player) {
    for (int step = 0; step < 60 * STEPS_PER_FRAME; step++) {
        player.updateOrientation();
    }
}

// Start a ship high above the landscape, well clear of the terrain
static Player makeFloatingShip() {
    Player player;
    player.setPosition(Fixed::fromInt(40), Fixed::fromInt(-30), Fixed::fromInt(40));
    return player;
}

// -----------------------------------------------------------------------------
// Steering
// -----------------------------------------------------------------------------

TEST(steering_tracks_original_frame_by_frame) {
    // Swing the mouse to a series of positions and compare the ship's
    // direction and pitch with the original at the end of every frame
    const int targets[][2] = {{300, -200}, {-450, 100}, {20, 500}, {-100, -100}, {511, 0}};

    Player player = makeFloatingShip();
    int32_t direction = player.getShipDirection();
    int32_t pitch = player.getShipPitch();
    double worstDirection = 0.0;
    double worstPitch = 0.0;

    for (const auto& target : targets) {
        setMouse(player, target[0], target[1], 0);

        int32_t scaledX = static_cast<int32_t>(static_cast<uint32_t>(target[0]) << 22);
        int32_t scaledY = static_cast<int32_t>(static_cast<uint32_t>(target[1]) << 22);
        PolarCoordinates polar = getMouseInPolarCoordinates(scaledX, scaledY);
        int32_t targetPitch = std::min(polar.distance, 0x3FFFFFFF) * 2;

        for (int frame = 0; frame < 15; frame++) {
            for (int step = 0; step < STEPS_PER_FRAME; step++) {
                player.updateOrientation();
            }
            direction = originalApproach(direction, polar.angle);
            pitch = originalApproach(pitch, targetPitch);

            worstDirection = std::max(worstDirection, angleDifference(player.getShipDirection(), direction));
            worstPitch = std::max(worstPitch, angleDifference(player.getShipPitch(), pitch));
        }
    }

    printf("(worst direction %.2f, pitch %.2f degrees) ", worstDirection, worstPitch);
    ASSERT(worstDirection < 0.01);
    ASSERT(worstPitch < 0.01);
}

TEST(steering_settles_on_target) {
    Player player = makeFloatingShip();
    setMouse(player, -300, 250, 0);

    for (int step = 0; step < 120 * STEPS_PER_FRAME; step++) {
        player.updateOrientation();
    }

    PolarCoordinates polar = getMouseInPolarCoordinates(
        static_cast<int32_t>(static_cast<uint32_t>(-300) << 22),
        static_cast<int32_t>(static_cast<uint32_t>(250) << 22));
    ASSERT(player.getShipDirection() == polar.angle);
    ASSERT(player.getShipPitch() == std::min(polar.distance, 0x3FFFFFFF) * 2);
}

TEST(full_pitch_at_edge_of_mouse_range) {
    // Pushing the mouse to the edge (in either direction) gives full pitch
    const int edges[][2] = {{511, 0}, {-512, 0}, {0, 511}, {0, -511}};
    for (const auto& edge : edges) {
        Player player = makeFloatingShip();
        setMouse(player, edge[0], edge[1], 0);
        for (int step = 0; step < 60 * STEPS_PER_FRAME; step++) {
            player.updateOrientation();
        }
        double pitch = player.getShipPitch() / 2147483647.0;
        ASSERT(pitch > 0.99);
    }
}

TEST(moving_down_loops_the_ship_back_upright) {
    // Keep moving the mouse down: the ship pitches forwards, turns upside
    // down, and carries on round the loop until it's upright again, one full
    // turn per 1024 units of pointer travel. It must never jump.
    Player player = makeFloatingShip();
    player.updateInput(512, 512, 0);
    settle(player);
    double startRoofY = player.getRotationMatrix().roof().y.toDouble();

    double lowestRoofY = startRoofY;
    double biggestJump = 0.0;
    Vec3 previousRoof = player.getRotationMatrix().roof();

    for (int travelled = 4; travelled <= 1024; travelled += 4) {
        player.updateInput(512, 512 - travelled, 0);  // Pointer y goes below zero
        for (int step = 0; step < STEPS_PER_FRAME; step++) {
            player.updateOrientation();
        }
        Vec3 roof = player.getRotationMatrix().roof();
        double jump = std::hypot(std::hypot((roof.x - previousRoof.x).toDouble(),
                                            (roof.y - previousRoof.y).toDouble()),
                                 (roof.z - previousRoof.z).toDouble());
        biggestJump = std::max(biggestJump, jump);
        lowestRoofY = std::min(lowestRoofY, roof.y.toDouble());
        previousRoof = roof;
    }
    settle(player);
    double endRoofY = player.getRotationMatrix().roof().y.toDouble();

    printf("(roof y %.2f -> %.2f -> %.2f, biggest step %.3f) ",
           startRoofY, lowestRoofY, endRoofY, biggestJump);
    ASSERT(startRoofY > 0.99);                        // Upright
    ASSERT(lowestRoofY < -0.99);                      // Upside down on the way round
    ASSERT(std::fabs(endRoofY - startRoofY) < 0.01);  // Upright again
    ASSERT(biggestJump < 0.2);                        // Smoothly, with no flips
}

TEST(right_of_screen_is_a_dead_zone) {
    // The pointer can reach x = 1279, but the original caps it at 1023, so the
    // last 256 units of travel to the right do nothing
    Player player = makeFloatingShip();
    player.updateInput(1023, 512, 0);
    ASSERT(player.getInput().mouseX == 511);
    player.updateInput(MousePointer::MAX_X, 512, 0);
    ASSERT(player.getInput().mouseX == 511);
    player.updateInput(0, 512, 0);
    ASSERT(player.getInput().mouseX == -512);
}

// -----------------------------------------------------------------------------
// Physics
// -----------------------------------------------------------------------------

TEST(friction_matches_original_per_second) {
    // Coasting: the original keeps 63/64 of its velocity per frame, so
    // (63/64)^15 = 79% after a second
    Player player = makeFloatingShip();
    player.setVelocity(Fixed::fromRaw(0x00100000), Fixed(), Fixed());

    for (int step = 0; step < 15 * STEPS_PER_FRAME; step++) {
        player.updatePhysics();
    }

    double kept = player.getVelocity().x.raw / static_cast<double>(0x00100000);
    double expected = std::pow(63.0 / 64.0, 15);
    printf("(kept %.3f, original %.3f) ", kept, expected);
    ASSERT(std::fabs(kept - expected) < 0.005);
}

TEST(thrust_and_gravity_match_original_trajectory) {
    // Full thrust with the ship level, compared with the original's per-frame
    // integration. The original applies a whole frame of thrust before moving,
    // so it runs slightly ahead of our finer steps while speed builds up;
    // velocities match closely and positions to within a fraction of a tile.
    Player player = makeFloatingShip();
    setMouse(player, 0, 0, SDL_BUTTON_LMASK);
    for (int step = 0; step < 60 * STEPS_PER_FRAME; step++) {
        player.updateOrientation();  // Settle pitch at zero (level)
    }
    player.setPosition(Fixed::fromInt(40), Fixed::fromInt(-30), Fixed::fromInt(40));
    player.setVelocity(Fixed(), Fixed(), Fixed());

    // The original, using our exhaust vector scaled to its 1.31 format
    double exhaustY = player.getRotationMatrix().roof().y.raw * 128.0;
    double startY = player.getY().raw;
    double y = startY;
    double vy = 0.0;
    double worstVelocity = 0.0;
    double worstPosition = 0.0;

    for (int frame = 1; frame <= 30; frame++) {
        for (int step = 0; step < STEPS_PER_FRAME; step++) {
            player.updatePhysics();
        }
        vy -= vy / 64.0;
        vy -= exhaustY / 2048.0;
        y += vy;
        vy += 0x30000;

        if (frame >= 5) {
            double portVelocity = player.getVelocity().y.raw * static_cast<double>(STEPS_PER_FRAME);
            worstVelocity = std::max(worstVelocity, std::fabs(portVelocity / vy - 1.0));
            worstPosition = std::max(worstPosition,
                                     std::fabs(player.getY().raw - y) / GameConstants::TILE_SIZE.raw);
        }
    }

    printf("(climbed %.2f tiles in 2 seconds, worst velocity %.1f%%, position %.2f tiles) ",
           (startY - y) / GameConstants::TILE_SIZE.raw, worstVelocity * 100, worstPosition);
    ASSERT(worstVelocity < 0.03);
    ASSERT(worstPosition < 1.0);
}

TEST(terminal_velocity_matches_original) {
    // Under constant thrust, friction limits speed: the original settles where
    // velocity / 64 balances the thrust each frame
    Player player = makeFloatingShip();
    setMouse(player, 0, 0, SDL_BUTTON_LMASK);
    for (int step = 0; step < 60 * STEPS_PER_FRAME; step++) {
        player.updateOrientation();
    }

    for (int step = 0; step < 800 * STEPS_PER_FRAME; step++) {  // ~12 friction time constants
        player.setPosition(Fixed::fromInt(40), Fixed::fromInt(-30), Fixed::fromInt(40));
        player.updatePhysics();
    }

    // Per frame: v = 64 * (thrust - gravity), where our per-step velocity is 1/8
    double exhaustY = player.getRotationMatrix().roof().y.raw * 128.0;
    double originalPerFrame = 64.0 * (-exhaustY / 2048.0 + 0x30000);
    double portPerFrame = player.getVelocity().y.raw * static_cast<double>(STEPS_PER_FRAME);
    printf("(port %.0f, original %.0f per frame) ", portPerFrame, originalPerFrame);
    ASSERT(std::fabs(portPerFrame / originalPerFrame - 1.0) < 0.02);
}

TEST(gravity_rises_with_score) {
    // The original's PrintCurrentScore rules, applied every frame
    struct { int score; int32_t gravity; } sequence[] = {
        {500, 0x30000},   // Start of a game
        {1023, 0x30000},
        {1024, 0x50000},  // Heavier from 1024
        {900, 0x50000},   // ...and stays heavier below 1024
        {1488, 0x70000},  // Heavier still from 1488
        {1400, 0x50000},  // Back to &50000 below 1488
        {600, 0x50000},   // Only a new game resets it
    };

    int32_t gravity = Gravity::INITIAL;
    for (const auto& entry : sequence) {
        gravity = Gravity::forScore(gravity, entry.score);
        ASSERT(gravity == entry.gravity);
    }
}

TEST(ship_falls_faster_under_higher_gravity) {
    // Free fall for a second at the start and at the heaviest gravity: the
    // distance fallen scales with gravity (&70000 / &30000)
    auto fallInOneSecond = [](int32_t gravity) {
        Player player = makeFloatingShip();
        player.setGravity(Gravity::perStep(gravity));
        Fixed start = player.getY();
        for (int step = 0; step < 15 * STEPS_PER_FRAME; step++) {
            player.updatePhysics();
        }
        return (player.getY() - start).toDouble();
    };

    double light = fallInOneSecond(Gravity::INITIAL);
    double heavy = fallInOneSecond(0x70000);
    printf("(%.2f vs %.2f tiles) ", light, heavy);
    ASSERT(std::fabs(heavy / light - 7.0 / 3.0) < 0.01);
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    printf("Player Controls and Physics Tests\n");
    printf("=================================\n\n");

    RUN_TEST(steering_tracks_original_frame_by_frame);
    RUN_TEST(steering_settles_on_target);
    RUN_TEST(full_pitch_at_edge_of_mouse_range);
    RUN_TEST(moving_down_loops_the_ship_back_upright);
    RUN_TEST(right_of_screen_is_a_dead_zone);
    RUN_TEST(friction_matches_original_per_second);
    RUN_TEST(thrust_and_gravity_match_original_trajectory);
    RUN_TEST(terminal_velocity_matches_original);
    RUN_TEST(gravity_rises_with_score);
    RUN_TEST(ship_falls_faster_under_higher_gravity);

    printf("\n%d/%d tests passed\n", passCount, testCount);
    return (passCount == testCount) ? 0 : 1;
}
