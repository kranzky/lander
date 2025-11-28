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
        // Particles have a 10-tile Z offset to match the ship's visual position.
        // For terrain collision, subtract this offset to get the actual world Z.
        constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000;  // 10 tiles
        Fixed worldZ = Fixed::fromRaw(p.position.z.raw - SHIP_VISUAL_Z_OFFSET);
        Fixed terrainY = getLandscapeAltitude(p.position.x, worldZ);

        // If particle is below terrain (positive Y is down)
        if (p.position.y.raw > terrainY.raw) {
            // Place particle on surface
            p.position.y = terrainY;

            // Check if this is a water impact (terrain at sea level)
            bool isWater = (terrainY.raw == GameConstants::SEA_LEVEL.raw);

            if (isWater && p.splashesInSea()) {
                // Splash into sea - spawn spray particles and delete this particle
                Vec3 splashPos = p.position;
                // Raise splash position slightly above sea
                constexpr int32_t SPLASH_HEIGHT = 0x01000000 / 16;  // 1/16 tile
                splashPos.y = Fixed::fromRaw(splashPos.y.raw - SPLASH_HEIGHT);
                spawnSplashParticles(splashPos, p.velocity, p.hasBigSplash());
                removeParticle(i);
                continue;
            }

            if (!isWater && p.explodesOnGround()) {
                // Bullet hit terrain - spawn sparks and delete this particle
                spawnSparkParticles(p.position, p.velocity);
                removeParticle(i);
                continue;
            }

            if (!p.bouncesOffTerrain()) {
                // Particle doesn't bounce - just delete it
                removeParticle(i);
                continue;
            }

            // Bounce: reflect Y velocity and dampen all velocities
            p.velocity.y = Fixed::fromRaw(-(p.velocity.y.raw >> ParticleConstants::BOUNCE_DAMPING_SHIFT));
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
    // Scaled 2x for better visual appearance at 1280x1024
    constexpr int PARTICLE_WIDTH = 6;    // 3 * 2
    constexpr int PARTICLE_HEIGHT = 4;   // 2 * 2
    constexpr int SHADOW_WIDTH = 6;      // 3 * 2
    constexpr int SHADOW_HEIGHT = 2;     // 1 * 2

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

        // Get terrain height for shadow
        //
        // Particles have a Z offset of 10 tiles to match the ship's visual position.
        // For shadow rendering, we need to look up terrain at the player's actual
        // world Z position (subtracting the offset), but project the shadow at the
        // particle's visual Z position (with the offset) so it appears correctly.
        //
        // This matches how drawObjectShadow works for the ship: it uses worldPos
        // (actual player position) for terrain lookup but cameraRelPos (visual
        // position) for projection.
        constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000;  // 10 tiles
        Fixed terrainLookupZ = Fixed::fromRaw(p.position.z.raw - SHIP_VISUAL_Z_OFFSET);
        Fixed terrainY = getLandscapeAltitude(p.position.x, terrainLookupZ);

        // Shadow position: at particle's visual X/Z, but at terrain height
        // looked up from the actual world Z position
        Vec3 shadowWorldPos;
        shadowWorldPos.x = p.position.x;
        shadowWorldPos.y = terrainY;
        shadowWorldPos.z = p.position.z;  // Keep the visual Z offset for projection
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

            // If particle has fading flag, cycle through white -> yellow -> orange -> red
            // Based on remaining lifespan (higher lifespan = newer = whiter)
            if (p.hasFading()) {
                // Scale lifespan to 0-255 range (BASE_LIFESPAN is ~16 frames)
                int life = p.lifespan * 16;  // 16 frames * 16 = 256
                if (life > 255) life = 255;

                // White (255,255,255) -> Yellow (255,255,0) -> Orange (255,128,0) -> Red (255,0,0)
                // life 255-192: white to yellow (reduce blue)
                // life 192-64:  yellow to orange (reduce green from 255 to 128)
                // life 64-0:    orange to red (reduce green from 128 to 0)
                color.r = 255;
                if (life > 192) {
                    // White to yellow: blue fades out
                    color.g = 255;
                    color.b = static_cast<uint8_t>((life - 192) * 4);  // 255 -> 0
                } else if (life > 64) {
                    // Yellow to orange: green fades from 255 to 128
                    color.g = static_cast<uint8_t>(128 + (life - 64));  // 255 -> 128
                    color.b = 0;
                } else {
                    // Orange to red: green fades from 128 to 0
                    color.g = static_cast<uint8_t>(life * 2);  // 128 -> 0
                    color.b = 0;
                }
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
    // Calculate particle velocity: shipVel + exhaustDir * exhaustSpeed
    // The exhaust direction is normalized (~1.0 in 8.24), scale for exhaust speed
    // Exhaust should move in the exhaust direction relative to the ship
    constexpr int32_t EXHAUST_SPEED_SHIFT = 3;  // >> 3 = 0.125 tiles/frame exhaust speed (halved)

    Vec3 particleVel;
    particleVel.x = Fixed::fromRaw(vel.x.raw + (exhaust.x.raw >> EXHAUST_SPEED_SHIFT));
    particleVel.y = Fixed::fromRaw(vel.y.raw + (exhaust.y.raw >> EXHAUST_SPEED_SHIFT));
    particleVel.z = Fixed::fromRaw(vel.z.raw + (exhaust.z.raw >> EXHAUST_SPEED_SHIFT));

    // Calculate spawn position: at ship's visual position
    //
    // The ship is rendered at Z=15 from camera for consistent visual size, but
    // the player's actual position is only Z=5 from camera. We offset particle Z
    // by 10 tiles to match the ship's visual position.
    //
    // For shadow rendering, we subtract this offset to look up terrain at the
    // player's actual world Z position.
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000;  // 10 tiles in 8.24 format

    Vec3 particlePos;
    particlePos.x = pos.x;
    particlePos.y = pos.y;
    particlePos.z = Fixed::fromRaw(pos.z.raw + SHIP_VISUAL_Z_OFFSET);

    // Particle flags: FADING | SPLASH | BOUNCES | GRAVITY
    // Exhaust particles splash in water, bounce off terrain
    uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                     ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;

    // Base lifespan: 8 frames at 15fps = ~64 frames at 120fps
    // But we want shorter exhaust trails, so scale down
    constexpr int32_t BASE_LIFESPAN = 16;  // Tuned for visual appeal

    // Spawn particles
    int count = fullThrust ? 8 : 2;
    for (int i = 0; i < count; i++) {
        addExhaustParticle(particlePos, particleVel, BASE_LIFESPAN, flags);
    }
}

// =============================================================================
// Bullet Particle Spawning Implementation
// =============================================================================
//
// Port of MoveAndDrawPlayer Part 5 (Lander.arm lines 2369-2465).
//
// The original algorithm:
// 1. Calculate bullet velocity: playerVel + gunDir/256
//    This makes bullets move with the ship + in the gun direction
// 2. Calculate spawn position: playerPos - bulletVel + gunDir/128
//    This places bullet at gun muzzle, compensating for first velocity update
// 3. Lifespan: 20 frames at 15fps
// 4. Flags: GRAVITY | DESTROYS_OBJECTS | white color
//
// =============================================================================

void spawnBulletParticle(const Vec3& pos, const Vec3& vel, const Vec3& gunDir) {
    // Calculate bullet velocity: playerVel + gunDir * speed
    // Original uses gunDir >> 8, but we need faster bullets at 120fps
    // gunDir is normalized (~1.0 in 8.24), so >> 1 gives ~0.5 tiles/frame
    Vec3 bulletVel;
    bulletVel.x = Fixed::fromRaw(vel.x.raw + (gunDir.x.raw >> 1));
    bulletVel.y = Fixed::fromRaw(vel.y.raw + (gunDir.y.raw >> 1));
    bulletVel.z = Fixed::fromRaw(vel.z.raw + (gunDir.z.raw >> 1));

    // Calculate spawn position: at ship's visual position (same offset as exhaust)
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000;  // 10 tiles in 8.24 format

    Vec3 bulletPos;
    bulletPos.x = pos.x;
    bulletPos.y = pos.y;
    bulletPos.z = Fixed::fromRaw(pos.z.raw + SHIP_VISUAL_Z_OFFSET);

    // Bullet flags from original: 0x01BC00FF = bits 18,19,20,21,23,24 + color 0xFF
    // Bit 18 = SPLASH, Bit 19 = BOUNCES, Bit 20 = GRAVITY, Bit 21 = DESTROYS_OBJECTS
    // Bit 23 = BIG_SPLASH, Bit 24 = EXPLODES_ON_GROUND
    uint32_t flags = ParticleFlags::SPLASH | ParticleFlags::BOUNCES |
                     ParticleFlags::GRAVITY | ParticleFlags::DESTROYS_OBJECTS |
                     ParticleFlags::BIG_SPLASH | ParticleFlags::EXPLODES_ON_GROUND | 0xFF;

    // Lifespan: reduced for shorter bullet range
    constexpr int32_t BULLET_LIFESPAN = 60;  // ~0.5 second at 120fps

    particleSystem.addParticle(bulletPos, bulletVel, BULLET_LIFESPAN, flags);
}

// =============================================================================
// Splash Particle Spawning Implementation
// =============================================================================
//
// Port of SplashParticleIntoSea and AddSprayParticleToBuffer from Lander.arm.
//
// When something hits water:
// 1. Spawn 4 or 65 spray particles (depending on bigSplash flag)
// 2. Spray particles are blue-ish, have gravity, move upward/outward
// 3. Spray particles fade and disappear
//
// =============================================================================

void spawnSplashParticles(const Vec3& pos, const Vec3& impactVel, bool bigSplash) {
    int count = bigSplash ? 65 : 4;

    // Use impact velocity as bias (scaled down) for splash direction
    // X and Z inherit some of the bullet's horizontal momentum
    // Y is always upward (negative) regardless of impact direction
    int32_t biasX = impactVel.x.raw >> 4;  // 1/16 of impact velocity
    int32_t biasZ = impactVel.z.raw >> 4;

    for (int i = 0; i < count; i++) {
        // Random velocity with impact bias
        int32_t vx = biasX + (exhaustRandom() >> 13) - 0x040000;
        int32_t vy = -(exhaustRandom() >> 12) - 0x080000; // Always upward
        int32_t vz = biasZ + (exhaustRandom() >> 13) - 0x040000;

        Vec3 vel;
        vel.x = Fixed::fromRaw(vx);
        vel.y = Fixed::fromRaw(vy);
        vel.z = Fixed::fromRaw(vz);

        // Light blue color (VIDC palette index for cyan/light blue)
        uint8_t colorIndex = 0xCB;  // Light blue (R=51, G=187, B=255)

        // Flags: GRAVITY only (no bounce, no splash - spray just fades)
        uint32_t flags = ParticleFlags::GRAVITY | colorIndex;

        // Longer lifespan for visible spray effect
        int32_t lifespan = 16 + ((exhaustRandom() >> 26) & 0x1F);

        particleSystem.addParticle(pos, vel, lifespan, flags);
    }
}

// =============================================================================
// Spark Particle Spawning Implementation
// =============================================================================
//
// Port of AddSmallExplosionToBuffer from Lander.arm.
//
// When a bullet hits terrain (not water):
// 1. Spawn several spark particles in random directions
// 2. Sparks are yellow/orange, have gravity, short lifespan
// 3. Similar to explosion particles but smaller
//
// =============================================================================

void spawnSparkParticles(const Vec3& pos, const Vec3& impactVel) {
    constexpr int SPARK_COUNT = 8;

    // Use impact velocity as bias (scaled down) for spark direction
    // Sparks bounce back so we negate Y and add some horizontal spread
    int32_t biasX = impactVel.x.raw >> 4;  // 1/16 of impact velocity
    int32_t biasZ = impactVel.z.raw >> 4;

    for (int i = 0; i < SPARK_COUNT; i++) {
        // Random velocity with impact bias
        int32_t vx = biasX + (exhaustRandom() >> 13) - 0x040000;
        int32_t vy = (exhaustRandom() >> 12) - 0x0C0000;  // Upward (bounce back)
        int32_t vz = biasZ + (exhaustRandom() >> 13) - 0x040000;

        Vec3 vel;
        vel.x = Fixed::fromRaw(vx);
        vel.y = Fixed::fromRaw(vy);
        vel.z = Fixed::fromRaw(vz);

        // Yellow/orange color - use FADING flag to get white->yellow->orange->red
        // Color index doesn't matter much when FADING is set
        uint8_t colorIndex = 0xFF;  // White base

        // Flags: FADING | GRAVITY | BOUNCES (sparks can bounce)
        uint32_t flags = ParticleFlags::FADING | ParticleFlags::GRAVITY |
                         ParticleFlags::BOUNCES | colorIndex;

        // Longer lifespan for visible spark effect
        int32_t lifespan = 12 + ((exhaustRandom() >> 28) & 0x0F);

        particleSystem.addParticle(pos, vel, lifespan, flags);
    }
}
