#include "particles.h"
#include "landscape.h"

// =============================================================================
// Particle System Implementation
// =============================================================================
//
// Port of MoveAndDrawParticles (Part 1) from Lander.arm (lines 2780-2878).
//
// The original uses a linear buffer with a null terminator (flags = 0).
// We use a simpler array with a count, using swap-and-pop for deletion.
//
// Update loop per particle:
// 1. Decrement lifespan, delete if expired
// 2. Add velocity to position
// 3. If gravity flag set, add gravity to Y velocity
// 4. If fading flag set, update color based on age (handled in rendering)
// 5. Check terrain collision and bounce
//
// =============================================================================

// Global instance
ParticleSystem particleSystem;

ParticleSystem::ParticleSystem() : particleCount(0) {
    clear();
}

void ParticleSystem::clear() {
    particleCount = 0;
    // Mark all particles as inactive
    for (int i = 0; i < ParticleConstants::MAX_PARTICLES; i++) {
        particles[i].lifespan = 0;
        particles[i].flags = 0;
    }
}

bool ParticleSystem::addParticle(const Vec3& pos, const Vec3& vel, int32_t lifespan, uint32_t flags) {
    // Check if room for more particles
    if (particleCount >= ParticleConstants::MAX_PARTICLES) {
        return false;
    }

    // Add new particle at the end
    Particle& p = particles[particleCount];
    p.position = pos;
    p.velocity = vel;
    p.lifespan = lifespan;
    p.flags = flags;

    particleCount++;
    return true;
}

void ParticleSystem::removeParticle(int index) {
    // Swap with last particle and decrement count (swap-and-pop)
    if (index < particleCount - 1) {
        particles[index] = particles[particleCount - 1];
    }
    particleCount--;
}

void ParticleSystem::update() {
    // Process all particles (iterate backwards so removal doesn't skip)
    for (int i = particleCount - 1; i >= 0; i--) {
        Particle& p = particles[i];

        // Decrement lifespan
        p.lifespan--;
        if (p.lifespan <= 0) {
            // Particle expired - remove it
            removeParticle(i);
            continue;
        }

        // Apply velocity to position
        p.position.x = Fixed::fromRaw(p.position.x.raw + p.velocity.x.raw);
        p.position.y = Fixed::fromRaw(p.position.y.raw + p.velocity.y.raw);
        p.position.z = Fixed::fromRaw(p.position.z.raw + p.velocity.z.raw);

        // Apply gravity if flag is set
        if (p.hasGravity()) {
            p.velocity.y = Fixed::fromRaw(p.velocity.y.raw + ParticleConstants::PARTICLE_GRAVITY);
        }

        // Check terrain collision
        // Get altitude at particle's (x, z) position
        Fixed terrainY = getLandscapeAltitude(p.position.x, p.position.z);

        // If particle is below terrain (positive Y is down)
        if (p.position.y.raw > terrainY.raw) {
            // Bounce: reflect Y velocity and dampen
            // Original: BounceParticle applies reflection and damping
            p.position.y = terrainY;  // Place on surface

            // Reflect Y velocity and apply damping
            p.velocity.y = Fixed::fromRaw(-(p.velocity.y.raw >> ParticleConstants::BOUNCE_DAMPING_SHIFT));

            // Also dampen X and Z velocity on bounce
            p.velocity.x = Fixed::fromRaw(p.velocity.x.raw >> ParticleConstants::BOUNCE_DAMPING_SHIFT);
            p.velocity.z = Fixed::fromRaw(p.velocity.z.raw >> ParticleConstants::BOUNCE_DAMPING_SHIFT);
        }
    }
}
