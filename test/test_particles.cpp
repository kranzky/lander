#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "particles.h"
#include "camera.h"
#include "graphics_buffer.h"
#include "object_map.h"

// =============================================================================
// Particle System Tests
// =============================================================================

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

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST(initial_state) {
    particleSystem.clear();
    ASSERT(particleSystem.getParticleCount() == 0);
}

TEST(add_particle) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(5), Fixed::fromInt(-2), Fixed::fromInt(10) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    bool added = particleSystem.addParticle(pos, vel, 60, ParticleFlags::GRAVITY);
    ASSERT(added);
    ASSERT(particleSystem.getParticleCount() == 1);

    const Particle& p = particleSystem.getParticle(0);
    ASSERT(p.lifespan == 60);
    ASSERT(p.hasGravity());
    ASSERT(!p.hasFading());
    ASSERT(!p.isRock());
}

TEST(particle_expiry) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(5), Fixed::fromInt(-2), Fixed::fromInt(10) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    // Add particle with lifespan of 3 frames
    particleSystem.addParticle(pos, vel, 3, 0);
    ASSERT(particleSystem.getParticleCount() == 1);

    // Update 1
    particleSystem.update();
    ASSERT(particleSystem.getParticleCount() == 1);
    ASSERT(particleSystem.getParticle(0).lifespan == 2);

    // Update 2
    particleSystem.update();
    ASSERT(particleSystem.getParticleCount() == 1);
    ASSERT(particleSystem.getParticle(0).lifespan == 1);

    // Update 3 - particle should expire and be removed
    particleSystem.update();
    ASSERT(particleSystem.getParticleCount() == 0);
}

TEST(velocity_integration) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };
    Vec3 vel = { Fixed::fromInt(1), Fixed::fromInt(0), Fixed::fromInt(2) };

    particleSystem.addParticle(pos, vel, 100, 0);  // No gravity

    particleSystem.update();

    const Particle& p = particleSystem.getParticle(0);
    // Position should have increased by velocity
    ASSERT(p.position.x.toInt() == 1);
    ASSERT(p.position.z.toInt() == 2);
}

TEST(gravity_application) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(-10), Fixed::fromInt(0) };  // High up
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };    // Stationary

    particleSystem.addParticle(pos, vel, 100, ParticleFlags::GRAVITY);

    // After update, Y velocity should increase (falling)
    particleSystem.update();

    const Particle& p = particleSystem.getParticle(0);
    // With gravity, velocity.y should now be positive (falling)
    ASSERT(p.velocity.y.raw > 0);
}

TEST(color_flags) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    // Add particle with color 42 and fading flag
    uint32_t flags = 42 | ParticleFlags::FADING;
    particleSystem.addParticle(pos, vel, 100, flags);

    const Particle& p = particleSystem.getParticle(0);
    ASSERT(p.getColorIndex() == 42);
    ASSERT(p.hasFading());
}

TEST(multiple_particles) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    // Add 10 particles
    for (int i = 0; i < 10; i++) {
        particleSystem.addParticle(pos, vel, 100, i);  // Color = index
    }

    ASSERT(particleSystem.getParticleCount() == 10);

    // Verify colors
    for (int i = 0; i < 10; i++) {
        ASSERT(particleSystem.getParticle(i).getColorIndex() == i);
    }
}

TEST(max_particles_limit) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    // Add max particles
    for (int i = 0; i < ParticleConstants::MAX_PARTICLES; i++) {
        bool added = particleSystem.addParticle(pos, vel, 100, 0);
        ASSERT(added);
    }

    ASSERT(particleSystem.getParticleCount() == ParticleConstants::MAX_PARTICLES);

    // Try to add one more - should fail
    bool added = particleSystem.addParticle(pos, vel, 100, 0);
    ASSERT(!added);
    ASSERT(particleSystem.getParticleCount() == ParticleConstants::MAX_PARTICLES);
}

TEST(particle_removal_order) {
    particleSystem.clear();

    Vec3 pos = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };
    Vec3 vel = { Fixed::fromInt(0), Fixed::fromInt(0), Fixed::fromInt(0) };

    // Add 3 particles with different lifespans
    particleSystem.addParticle(pos, vel, 1, 100);  // Dies first, color 100
    particleSystem.addParticle(pos, vel, 2, 200);  // Dies second, color 200
    particleSystem.addParticle(pos, vel, 3, 300);  // Dies third, color 300

    ASSERT(particleSystem.getParticleCount() == 3);

    // After 1 update, first particle should be gone
    particleSystem.update();
    ASSERT(particleSystem.getParticleCount() == 2);

    // After 2nd update, second particle should be gone
    particleSystem.update();
    ASSERT(particleSystem.getParticleCount() == 1);

    // Remaining particle should have color 300
    ASSERT(particleSystem.getParticle(0).getColorIndex() == 44);  // 300 & 0xFF = 44
}

// -----------------------------------------------------------------------------
// Rendering regressions
// -----------------------------------------------------------------------------

// Wrapping add, as the 8.24 world coordinates wrap every 256 tiles
static Fixed wrapAdd(Fixed a, int tiles) {
    return Fixed::fromRaw(static_cast<int32_t>(static_cast<uint32_t>(a.raw) +
                                               (static_cast<uint32_t>(tiles) << 24)));
}

// Buffer particles 15 tiles in front of the camera (the ship's visual depth) and
// dx tiles to the side, and return how many triangles were buffered
static size_t bufferParticleAhead(const Vec3& cameraPos, int copies, int dx = 0) {
    particleSystem.clear();
    graphicsBuffers.clearAll();

    Camera camera;
    camera.setPosition(cameraPos);

    Vec3 pos = { wrapAdd(cameraPos.x, dx), Fixed::fromInt(-1), wrapAdd(cameraPos.z, 15) };
    Vec3 vel;
    for (int i = 0; i < copies; i++) {
        ASSERT(particleSystem.addParticle(pos, vel, 100, 0));
    }

    bufferParticlesInFront(camera, Fixed::fromInt(15));
    return graphicsBuffers.getTotalTriangleCount();
}

TEST(dense_cluster_is_fully_buffered) {
    // A crash explosion plus stars puts hundreds of particles in one row; each
    // needs 2 triangles plus 2 for its shadow, and none may be dropped
    Vec3 cameraPos = { Fixed::fromRaw(0x00800000), Fixed::fromInt(-3), Fixed::fromRaw(0x00800000) };
    ASSERT(bufferParticleAhead(cameraPos, 600) == 600 * 4);
}

TEST(particles_visible_across_world_seam) {
    // Camera just short of the 128-tile seam: the particle's coordinates wrap
    // negative but it is still right in front of the camera
    Vec3 nearSeamX = { Fixed::fromRaw(0x7F800000), Fixed::fromInt(-3), Fixed::fromRaw(0x00800000) };
    ASSERT(bufferParticleAhead(nearSeamX, 1, 1) == 4);

    Vec3 nearSeamZ = { Fixed::fromRaw(0x00800000), Fixed::fromInt(-3), Fixed::fromRaw(0x76800000) };
    ASSERT(bufferParticleAhead(nearSeamZ, 1) == 4);
}

TEST(smoke_rate_is_per_physics_step) {
    // A destroyed object just in front of the camera smokes twice every 128
    // physics steps; smoke used to be spawned per rendered frame, so the rate
    // depended on the frame rate
    particleSystem.clear();
    objectMap.clear();

    Camera camera;
    camera.setPosition(Vec3(Fixed::fromInt(40), Fixed::fromInt(-3), Fixed::fromInt(40)));
    objectMap.setObjectAt(40, 45, ObjectMap::getDestroyedType(ObjectType::BUILDING));

    for (uint32_t tick = 0; tick < 128; tick++) {
        spawnSmokeFromDestroyedObjects(camera, tick);
    }
    ASSERT(particleSystem.getParticleCount() == 2);

    objectMap.clear();
    particleSystem.clear();
}

TEST(no_rocks_until_score_passes_800) {
    particleSystem.clear();
    Vec3 playerPos(Fixed::fromInt(4), Fixed::fromInt(-3), Fixed::fromInt(4));
    for (int frame = 0; frame < 10000; frame++) {
        ASSERT(!dropRocksFromTheSky(playerPos, 800));
    }
    ASSERT(particleSystem.getParticleCount() == 0);
}

TEST(rocks_drop_in_front_of_ship) {
    // At a score of 800 + 16384 every frame drops a rock: 32 tiles up, in line
    // with the ship and one tile in front of it, almost at rest
    Vec3 playerPos(Fixed::fromInt(20), Fixed::fromInt(-3), Fixed::fromInt(30));
    constexpr int32_t MAX_SPEED = 1 << 18;  // +/- 2^21 per original frame

    for (int i = 0; i < 200; i++) {
        particleSystem.clear();
        ASSERT(dropRocksFromTheSky(playerPos, 800 + 16384));
        ASSERT(particleSystem.getParticleCount() == 1);

        const Particle& rock = particleSystem.getParticle(0);
        ASSERT(rock.isRock());
        ASSERT(rock.position.x == playerPos.x);
        ASSERT(rock.position.y.raw == -32 * 0x01000000 - 1);
        ASSERT(rock.position.z == playerPos.z - Fixed::fromInt(1));
        ASSERT(std::abs(rock.velocity.x.raw) <= MAX_SPEED);
        ASSERT(std::abs(rock.velocity.y.raw) <= MAX_SPEED);
        ASSERT(std::abs(rock.velocity.z.raw) <= MAX_SPEED);
        ASSERT(rock.lifespan >= 170 * 8 && rock.lifespan <= (170 + 31) * 8);
    }
    particleSystem.clear();
}

TEST(rock_frequency_rises_with_score) {
    // Each frame drops a rock if the original's random number, 0 to 16383, is
    // below (score - 800). That generator gives very small values only about
    // half as often as a uniform one, so just over 800 rocks are about half as
    // frequent as the formula suggests, as in the original; from about 1000 up
    // they follow it closely.
    Vec3 playerPos(Fixed::fromInt(20), Fixed::fromInt(-3), Fixed::fromInt(30));
    struct { int score; double minRatio; double maxRatio; } checks[] = {
        {900, 0.3, 0.7},   // About half the formula
        {1500, 0.9, 1.05}, // Close to the formula
    };
    for (const auto& check : checks) {
        constexpr int FRAMES = 200000;
        int drops = 0;
        for (int frame = 0; frame < FRAMES; frame++) {
            particleSystem.clear();
            drops += dropRocksFromTheSky(playerPos, check.score) ? 1 : 0;
        }
        double ratio = drops / (FRAMES * (check.score - 800) / 16384.0);
        printf("[%d: %.2f of formula] ", check.score, ratio);
        ASSERT(ratio > check.minRatio && ratio < check.maxRatio);
    }
    particleSystem.clear();
}

// -----------------------------------------------------------------------------
// Main
// -----------------------------------------------------------------------------

int main() {
    printf("Particle System Tests\n");
    printf("=====================\n\n");

    RUN_TEST(initial_state);
    RUN_TEST(add_particle);
    RUN_TEST(particle_expiry);
    RUN_TEST(velocity_integration);
    RUN_TEST(gravity_application);
    RUN_TEST(color_flags);
    RUN_TEST(multiple_particles);
    RUN_TEST(max_particles_limit);
    RUN_TEST(particle_removal_order);
    RUN_TEST(dense_cluster_is_fully_buffered);
    RUN_TEST(particles_visible_across_world_seam);
    RUN_TEST(smoke_rate_is_per_physics_step);
    RUN_TEST(no_rocks_until_score_passes_800);
    RUN_TEST(rocks_drop_in_front_of_ship);
    RUN_TEST(rock_frequency_rises_with_score);

    printf("\n%d/%d tests passed\n", passCount, testCount);
    return (passCount == testCount) ? 0 : 1;
}
