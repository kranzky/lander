#include <cstdio>
#include <cstdlib>
#include "particles.h"

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

    printf("\n%d/%d tests passed\n", passCount, testCount);
    return (passCount == testCount) ? 0 : 1;
}
