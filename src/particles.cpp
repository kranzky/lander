#include "particles.h"
#include "landscape.h"
#include "screen.h"
#include "camera.h"
#include "projection.h"
#include "palette.h"

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

// =============================================================================
// Particle Rendering Implementation
// =============================================================================
//
// Port of MoveAndDrawParticles (Parts 2-4) from Lander.arm.
// This renders all active particles as small rectangles with shadows.
//
// Rendering process per particle:
// 1. Transform position to camera-relative coordinates
// 2. Project shadow position (at terrain height)
// 3. Draw shadow as 3x1 black rectangle
// 4. Project particle position
// 5. Draw particle as 3x2 colored rectangle
//
// =============================================================================

namespace {
    // Particle rendering constants
    // Original: 3x2 pixels for particles, 3x1 for shadows at 320x256
    // Scaled 4x for 1280x1024
    constexpr int PARTICLE_WIDTH = 12;   // 3 * 4
    constexpr int PARTICLE_HEIGHT = 8;   // 2 * 4
    constexpr int SHADOW_WIDTH = 12;     // 3 * 4
    constexpr int SHADOW_HEIGHT = 4;     // 1 * 4

    // Draw a filled rectangle at the given screen coordinates
    void drawRect(ScreenBuffer& screen, int x, int y, int width, int height, Color color) {
        // Center the rectangle on the coordinate
        int left = x - width / 2;
        int top = y - height / 2;

        // Clip to screen bounds
        int right = left + width;
        int bottom = top + height;

        if (left < 0) left = 0;
        if (top < 0) top = 0;
        if (right > ScreenBuffer::PHYSICAL_WIDTH) right = ScreenBuffer::PHYSICAL_WIDTH;
        if (bottom > ScreenBuffer::PHYSICAL_HEIGHT) bottom = ScreenBuffer::PHYSICAL_HEIGHT;

        // Draw the rectangle
        for (int py = top; py < bottom; py++) {
            for (int px = left; px < right; px++) {
                screen.plotPhysicalPixel(px, py, color);
            }
        }
    }
}

void renderParticles(const Camera& camera, ScreenBuffer& screen) {
    int count = particleSystem.getParticleCount();

    for (int i = 0; i < count; i++) {
        const Particle& p = particleSystem.getParticle(i);

        // Skip rocks - they're rendered as 3D objects (handled elsewhere)
        if (p.isRock()) {
            continue;
        }

        // Transform particle position to camera-relative coordinates
        Vec3 cameraRelPos = camera.worldToCamera(p.position);

        // Skip particles behind the camera
        if (cameraRelPos.z.raw <= 0) {
            continue;
        }

        // Get terrain height at particle position for shadow
        Fixed terrainY = getLandscapeAltitude(p.position.x, p.position.z);

        // Shadow position: same X and Z, but at terrain height
        Vec3 shadowWorldPos = p.position;
        shadowWorldPos.y = terrainY;
        Vec3 shadowRelPos = camera.worldToCamera(shadowWorldPos);

        // Draw shadow first (so it appears under the particle)
        if (shadowRelPos.z.raw > 0) {
            ProjectedVertex shadowProj = projectVertex(shadowRelPos);
            if (shadowProj.visible && shadowProj.onScreen) {
                drawRect(screen, shadowProj.screenX, shadowProj.screenY,
                         SHADOW_WIDTH, SHADOW_HEIGHT, Color::black());
            }
        }

        // Project and draw particle
        ProjectedVertex proj = projectVertex(cameraRelPos);
        if (proj.visible && proj.onScreen) {
            // Get particle color from palette (VIDC 256-color format)
            uint8_t colorIndex = p.getColorIndex();
            Color color = vidc256ToColor(colorIndex);

            // If particle has fading flag, adjust color based on lifespan
            // Original fades from white to red as particle ages
            if (p.hasFading()) {
                // Fade from white (high lifespan) toward base color (low lifespan)
                // Assuming initial lifespan around 60-120 frames
                int fade = p.lifespan * 4;  // Scale lifespan to 0-255 range
                if (fade > 255) fade = 255;

                // Blend toward white based on remaining life
                color.r = static_cast<uint8_t>((color.r * (255 - fade) + 255 * fade) / 255);
                color.g = static_cast<uint8_t>((color.g * (255 - fade) + 255 * fade) / 255);
                color.b = static_cast<uint8_t>((color.b * (255 - fade) + 255 * fade) / 255);
            }

            drawRect(screen, proj.screenX, proj.screenY,
                     PARTICLE_WIDTH, PARTICLE_HEIGHT, color);
        }
    }
}

// =============================================================================
// Exhaust Particle Spawning Implementation
// =============================================================================
//
// Port of MoveAndDrawPlayer Part 4 (Lander.arm lines 2224-2365).
//
// The original algorithm:
// 1. Calculate particle velocity: (shipVel + exhaust/128) / 2
//    This makes particles move with ship but slow down relative to it
// 2. Calculate spawn position: shipPos - particleVel + exhaust/128
//    This offsets particles in exhaust direction, accounting for first update
// 3. Add random variation to velocity and lifespan
// 4. Spawn 8 particles for full thrust, 2 for hover
//
// =============================================================================

namespace {
    // Simple pseudo-random number generator for particle variation
    // Uses a static seed that advances each call
    uint32_t exhaustRandomSeed = 0x12345678;

    int32_t exhaustRandom() {
        // Linear congruential generator
        exhaustRandomSeed = exhaustRandomSeed * 1103515245 + 12345;
        return static_cast<int32_t>(exhaustRandomSeed);
    }

    // Add a single exhaust particle with randomization
    void addExhaustParticle(const Vec3& basePos, const Vec3& baseVel,
                            int32_t baseLifespan, uint32_t flags) {
        // Add random variation to velocity
        // Original uses +/- 2^(32 - 10) = +/- 0x400000
        // Scaled for our 8.24 format: >> 7 gives similar range
        constexpr int32_t VEL_RANDOM_RANGE = 0x80000;  // ~0.5 tiles/frame variation

        Vec3 particleVel = baseVel;
        particleVel.x = Fixed::fromRaw(particleVel.x.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE/2);
        particleVel.y = Fixed::fromRaw(particleVel.y.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE/2);
        particleVel.z = Fixed::fromRaw(particleVel.z.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE/2);

        // Add random variation to lifespan (0 to 8 extra frames)
        int32_t lifespan = baseLifespan + ((exhaustRandom() >> 24) & 0x07);

        particleSystem.addParticle(basePos, particleVel, lifespan, flags);
    }
}

void spawnExhaustParticles(const Vec3& pos, const Vec3& vel, const Vec3& exhaust, bool fullThrust) {
    // Calculate particle velocity: (shipVel + exhaust/128) / 2
    // Original: ADD R3, R0, R6, ASR #7; MOV R3, R3, ASR #1
    // For our 8.24 format, exhaust is already normalized ~1.0, so scale appropriately
    Vec3 particleVel;
    particleVel.x = Fixed::fromRaw((vel.x.raw + (exhaust.x.raw >> 3)) >> 1);
    particleVel.y = Fixed::fromRaw((vel.y.raw + (exhaust.y.raw >> 3)) >> 1);
    particleVel.z = Fixed::fromRaw((vel.z.raw + (exhaust.z.raw >> 3)) >> 1);

    // Calculate spawn position: shipPos - particleVel + exhaust/128
    // This places particle behind ship and compensates for first velocity update
    //
    // Note: The ship is rendered at a fixed Z distance (15 tiles from camera) for
    // consistent visual size, but the player's actual position is only 5 tiles from
    // camera. We offset particle Z by 10 tiles to match the ship's visual position.
    constexpr int32_t SHIP_Z_OFFSET = 10 * 0x01000000;  // 10 tiles in 8.24 format

    Vec3 particlePos;
    particlePos.x = Fixed::fromRaw(pos.x.raw - particleVel.x.raw + (exhaust.x.raw >> 3));
    particlePos.y = Fixed::fromRaw(pos.y.raw - particleVel.y.raw + (exhaust.y.raw >> 3));
    particlePos.z = Fixed::fromRaw(pos.z.raw - particleVel.z.raw + (exhaust.z.raw >> 3) + SHIP_Z_OFFSET);

    // Particle flags: FADING | GRAVITY | BOUNCE | SPLASH
    // Original: 0x001D0000 = bits 16, 18, 19, 20
    // We use FADING (16) and GRAVITY (20) for now
    uint32_t flags = ParticleFlags::FADING | ParticleFlags::GRAVITY;

    // Base lifespan: 8 frames at 15fps = ~64 frames at 120fps
    // But we want shorter exhaust trails, so scale down
    constexpr int32_t BASE_LIFESPAN = 16;  // Tuned for visual appeal

    // Spawn particles
    int count = fullThrust ? 8 : 2;
    for (int i = 0; i < count; i++) {
        addExhaustParticle(particlePos, particleVel, BASE_LIFESPAN, flags);
    }
}
