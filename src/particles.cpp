#include "particles.h"
#include "landscape.h"
#include "screen.h"
#include "camera.h"
#include "projection.h"
#include "palette.h"
#include "graphics_buffer.h"
#include "object_map.h"
#include "object3d.h"
#include "object_renderer.h"
#include <cstdio>

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

// Global particle events for sound triggers
static ParticleEvents particleEvents;

ParticleEvents& getParticleEvents() {
    return particleEvents;
}

ParticleSystem::ParticleSystem() : particleCount(0)
{
    clear();
}

void ParticleSystem::clear()
{
    particleCount = 0;
    // Mark all particles as inactive
    for (int i = 0; i < ParticleConstants::MAX_PARTICLES; i++)
    {
        particles[i].lifespan = 0;
        particles[i].flags = 0;
    }
}

bool ParticleSystem::addParticle(const Vec3 &pos, const Vec3 &vel, int32_t lifespan, uint32_t flags)
{
    // Check if room for more particles
    if (particleCount >= ParticleConstants::MAX_PARTICLES)
    {
        return false;
    }

    // Add new particle at the end
    Particle &p = particles[particleCount];
    p.position = pos;
    p.velocity = vel;
    p.lifespan = lifespan;
    p.flags = flags;

    particleCount++;
    return true;
}

void ParticleSystem::removeParticle(int index)
{
    // Swap with last particle and decrement count (swap-and-pop)
    if (index < particleCount - 1)
    {
        particles[index] = particles[particleCount - 1];
    }
    particleCount--;
}

void ParticleSystem::update()
{
    // Reset event counters for this frame
    particleEvents.reset();

    // Process all particles (iterate backwards so removal doesn't skip)
    for (int i = particleCount - 1; i >= 0; i--)
    {
        Particle &p = particles[i];

        // Decrement lifespan
        p.lifespan--;
        if (p.lifespan <= 0)
        {
            // Particle expired - remove it
            removeParticle(i);
            continue;
        }

        // Apply velocity to position
        p.position.x = Fixed::fromRaw(p.position.x.raw + p.velocity.x.raw);
        p.position.y = Fixed::fromRaw(p.position.y.raw + p.velocity.y.raw);
        p.position.z = Fixed::fromRaw(p.position.z.raw + p.velocity.z.raw);

        // Apply gravity if flag is set
        if (p.hasGravity())
        {
            p.velocity.y = Fixed::fromRaw(p.velocity.y.raw + ParticleConstants::PARTICLE_GRAVITY);
        }

        // Check terrain collision
        // Most particles have a 10-tile Z offset to match the ship's visual position.
        // For terrain collision, subtract this offset to get the actual world Z.
        // Rocks are stored in actual world coordinates (no offset).
        constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000; // 10 tiles
        Fixed worldZ = p.isRock()
            ? p.position.z  // Rocks: already in world coordinates
            : Fixed::fromRaw(p.position.z.raw - SHIP_VISUAL_Z_OFFSET);  // Others: subtract offset
        Fixed terrainY = getLandscapeAltitude(p.position.x, worldZ);

        // =================================================================
        // Object Collision Detection
        // =================================================================
        // Port of ProcessObjectDestruction from Lander.arm (lines 3290-3364).
        //
        // If particle has DESTROYS_OBJECTS flag and is within SAFE_HEIGHT of
        // terrain, check if there's an object at the particle's tile position.
        // If so, mark the object as destroyed and spawn an explosion.
        //
        if (p.canDestroyObjects())
        {
            // Calculate height above ground: terrain altitude - particle Y
            // (positive Y is down, so terrainY - particleY gives height above)
            int32_t heightAboveGround = terrainY.raw - p.position.y.raw;

            // Only check for objects if particle is close to ground
            if (heightAboveGround < GameConstants::SAFE_HEIGHT.raw)
            {
                // Get tile coordinates from world position (8.24 >> 24 = integer tile)
                uint8_t tileX = static_cast<uint8_t>(p.position.x.raw >> 24);
                uint8_t tileZ = static_cast<uint8_t>(worldZ.raw >> 24);

                // Look up object at this tile
                uint8_t objectType = objectMap.getObjectAt(tileX, tileZ);

                // Check if there's an object (0xFF = no object)
                if (objectType != ObjectType::NONE)
                {
                    // Calculate object's world position (tile center)
                    // Objects are rendered at tile centers, so add 0.5 tiles to X and Z
                    // Y is half a tile above ground (positive Y is down, so subtract)
                    // NOTE: Do NOT add SHIP_VISUAL_Z_OFFSET here - spawnExplosionParticles
                    // adds it internally when converting to visual coordinates
                    Vec3 objectPos;
                    objectPos.x = Fixed::fromRaw(tileX << 24); // + (GameConstants::TILE_SIZE.raw >> 1));
                    objectPos.z = Fixed::fromRaw(tileZ << 24); // + (GameConstants::TILE_SIZE.raw >> 1));
                    Fixed groundY = getLandscapeAltitude(objectPos.x, Fixed::fromRaw(tileZ << 24));
                    objectPos.y = Fixed::fromRaw(groundY.raw - (GameConstants::TILE_SIZE.raw >> 1));

                    // Check if object is already destroyed (type >= 12)
                    if (ObjectMap::isDestroyedType(objectType))
                    {
                        // Already destroyed - bullet passes through, no effect
                        // (don't remove bullet, let it continue)
                    }
                    else
                    {
                        // Mark object as destroyed (add 12 to type)
                        uint8_t destroyedType = ObjectMap::getDestroyedType(objectType);
                        objectMap.setObjectAt(tileX, tileZ, destroyedType);

                        // Spawn medium explosion (20 clusters = 80 particles)
                        spawnExplosionParticles(objectPos, 20);

                        // Track event for sound (with position for spatial audio)
                        particleEvents.objectDestroyed++;
                        particleEvents.objectDestroyedPos = objectPos;

                        // TODO: Add score (Task 40)
                        // If not a rock (bit 17 clear), add 20 to score
                        // if (!p.isRock()) { score += 20; }

                        // Remove the bullet particle
                        removeParticle(i);
                        continue;
                    }
                }
            }
        }

        // Special handling for rocks: explode 1 tile above ground/water
        if (p.isRock())
        {
            constexpr int32_t ROCK_EXPLODE_HEIGHT = 1 * 0x01000000;  // 1 tile above ground
            int32_t heightAboveTerrain = terrainY.raw - p.position.y.raw;

            if (heightAboveTerrain <= ROCK_EXPLODE_HEIGHT)
            {
                // Rock is within 1 tile of ground/water - explode at current position
                // Rocks are in world coords, and spawnExplosionParticles adds +10 Z offset
                // which matches the visual offset we use when rendering rocks
                spawnExplosionParticles(p.position, 20);
                particleEvents.rockExploded++;
                particleEvents.rockExplodedPos = p.position;
                removeParticle(i);
                continue;
            }
        }

        // If particle is below terrain (positive Y is down)
        if (p.position.y.raw > terrainY.raw)
        {
            // Place particle on surface
            p.position.y = terrainY;

            // Check if this is a water impact (terrain at sea level)
            bool isWater = (terrainY.raw == GameConstants::SEA_LEVEL.raw);

            if (isWater && p.splashesInSea())
            {
                // Splash into sea - spawn spray particles and delete this particle
                Vec3 splashPos = p.position;
                // Raise splash position slightly above sea
                constexpr int32_t SPLASH_HEIGHT = 0x01000000 / 16; // 1/16 tile
                splashPos.y = Fixed::fromRaw(splashPos.y.raw - SPLASH_HEIGHT);
                spawnSplashParticles(splashPos, p.velocity, p.hasBigSplash());

                // Track what hit water (bullets have BIG_SPLASH, exhaust doesn't)
                if (p.hasBigSplash()) {
                    particleEvents.bulletHitWater++;
                    particleEvents.bulletHitWaterPos = p.position;
                } else {
                    particleEvents.exhaustHitWater++;
                    particleEvents.exhaustHitWaterPos = p.position;
                }

                removeParticle(i);
                continue;
            }

            if (!isWater && p.explodesOnGround())
            {
                // Bullet hit terrain - spawn sparks and delete this particle
                spawnSparkParticles(p.position, p.velocity);
                particleEvents.bulletHitGround++;
                particleEvents.bulletHitGroundPos = p.position;
                removeParticle(i);
                continue;
            }

            if (!p.bouncesOffTerrain())
            {
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

namespace
{
    // Particle rendering constants
    inline int getParticleWidth() {
        if (DisplayConfig::scale == 4) return 8;    // 1280x1024
        if (DisplayConfig::scale == 2) return 4;    // 640x512
        return 2;                                    // 320x256
    }
    inline int getParticleHeight() {
        if (DisplayConfig::scale == 4) return 6;    // 1280x1024
        if (DisplayConfig::scale == 2) return 3;    // 640x512
        return 2;                                    // 320x256
    }
    inline int getShadowWidth() {
        if (DisplayConfig::scale == 4) return 8;    // 1280x1024
        if (DisplayConfig::scale == 2) return 4;    // 640x512
        return 2;                                    // 320x256
    }
    inline int getShadowHeight() {
        if (DisplayConfig::scale == 4) return 6;    // 1280x1024
        if (DisplayConfig::scale == 2) return 3;    // 640x512
        return 2;                                    // 320x256
    }

    // Draw a filled rectangle at the given screen coordinates
    void drawRect(ScreenBuffer &screen, int x, int y, int width, int height, Color color)
    {
        // Center the rectangle on the coordinate
        int left = x - width / 2;
        int top = y - height / 2;

        // Clip to screen bounds
        int right = left + width;
        int bottom = top + height;

        if (left < 0)
            left = 0;
        if (top < 0)
            top = 0;
        if (right > ScreenBuffer::PHYSICAL_WIDTH())
            right = ScreenBuffer::PHYSICAL_WIDTH();
        if (bottom > ScreenBuffer::PHYSICAL_HEIGHT())
            bottom = ScreenBuffer::PHYSICAL_HEIGHT();

        // Draw the rectangle
        for (int py = top; py < bottom; py++)
        {
            for (int px = left; px < right; px++)
            {
                screen.plotPhysicalPixel(px, py, color);
            }
        }
    }
}

void renderParticles(const Camera &camera, ScreenBuffer &screen)
{
    int count = particleSystem.getParticleCount();

    for (int i = 0; i < count; i++)
    {
        const Particle &p = particleSystem.getParticle(i);

        // Skip rocks - they're rendered as 3D objects (handled elsewhere)
        if (p.isRock())
        {
            continue;
        }

        // Transform particle position to camera-relative coordinates
        Vec3 cameraRelPos = camera.worldToCamera(p.position);

        // Skip particles behind the camera
        if (cameraRelPos.z.raw <= 0)
        {
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
        constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000; // 10 tiles
        Fixed terrainLookupZ = Fixed::fromRaw(p.position.z.raw - SHIP_VISUAL_Z_OFFSET);
        Fixed terrainY = getLandscapeAltitude(p.position.x, terrainLookupZ);

        // Shadow position: at particle's visual X/Z, but at terrain height
        // looked up from the actual world Z position
        Vec3 shadowWorldPos;
        shadowWorldPos.x = p.position.x;
        shadowWorldPos.y = terrainY;
        shadowWorldPos.z = p.position.z; // Keep the visual Z offset for projection
        Vec3 shadowRelPos = camera.worldToCamera(shadowWorldPos);

        // Draw shadow first (so it appears under the particle)
        if (shadowRelPos.z.raw > 0)
        {
            ProjectedVertex shadowProj = projectVertex(shadowRelPos);
            if (shadowProj.visible && shadowProj.onScreen)
            {
                drawRect(screen, shadowProj.screenX, shadowProj.screenY,
                         getShadowWidth(), getShadowHeight(), Color::black());
            }
        }

        // Project and draw particle
        ProjectedVertex proj = projectVertex(cameraRelPos);
        if (proj.visible && proj.onScreen)
        {
            // Get particle color from palette (VIDC 256-color format)
            uint8_t colorIndex = p.getColorIndex();
            Color color = vidc256ToColor(colorIndex);

            // If particle has fading flag, cycle through white -> yellow -> orange -> red
            // Based on remaining lifespan (higher lifespan = newer = whiter)
            if (p.hasFading())
            {
                // Scale lifespan to 0-255 range (BASE_LIFESPAN is ~16 frames)
                int life = p.lifespan * 16; // 16 frames * 16 = 256
                if (life > 255)
                    life = 255;

                // White (255,255,255) -> Yellow (255,255,0) -> Orange (255,128,0) -> Red (255,0,0)
                // life 255-192: white to yellow (reduce blue)
                // life 192-64:  yellow to orange (reduce green from 255 to 128)
                // life 64-0:    orange to red (reduce green from 128 to 0)
                color.r = 255;
                if (life > 192)
                {
                    // White to yellow: blue fades out
                    color.g = 255;
                    color.b = static_cast<uint8_t>((life - 192) * 4); // 255 -> 0
                }
                else if (life > 64)
                {
                    // Yellow to orange: green fades from 255 to 128
                    color.g = static_cast<uint8_t>(128 + (life - 64)); // 255 -> 128
                    color.b = 0;
                }
                else
                {
                    // Orange to red: green fades from 128 to 0
                    color.g = static_cast<uint8_t>(life * 2); // 128 -> 0
                    color.b = 0;
                }
            }

            drawRect(screen, proj.screenX, proj.screenY,
                     getParticleWidth(), getParticleHeight(), color);
        }
    }
}

// =============================================================================
// Buffered Particle Rendering Implementation
// =============================================================================
//
// Depth-sorted particle rendering using the graphics buffer system.
// Particles are buffered by row so they interleave correctly with landscape.
// Rectangles are drawn as two triangles to use the existing buffer system.
//
// =============================================================================

namespace
{
    // Buffer a filled rectangle as two triangles
    void bufferRect(int row, int x, int y, int width, int height, Color color, bool isShadow)
    {
        // Calculate rectangle corners centered on (x, y)
        // Use exclusive right/bottom (right = left + width, not left + width - 1)
        // because the triangle rasterizer treats coordinates inclusively
        int left = x - width / 2;
        int top = y - height / 2;
        // Subtract 1 to convert from exclusive to inclusive coordinates
        int right = left + width - 1;
        int bottom = top + height - 1;

        // Ensure we have at least a 1-pixel rect
        if (right < left) right = left;
        if (bottom < top) bottom = top;

        // Draw as two triangles: top-left triangle and bottom-right triangle
        // Triangle 1: top-left, top-right, bottom-left
        // Triangle 2: top-right, bottom-right, bottom-left

        if (isShadow)
        {
            graphicsBuffers.addShadowTriangle(row,
                                              left, top,
                                              right, top,
                                              left, bottom,
                                              color);
            graphicsBuffers.addShadowTriangle(row,
                                              right, top,
                                              right, bottom,
                                              left, bottom,
                                              color);
        }
        else
        {
            graphicsBuffers.addTriangle(row,
                                        left, top,
                                        right, top,
                                        left, bottom,
                                        color);
            graphicsBuffers.addTriangle(row,
                                        right, top,
                                        right, bottom,
                                        left, bottom,
                                        color);
        }
    }
}

// Depth filter mode for particle buffering
enum class DepthFilter {
    BEHIND,     // Only particles with cameraRelPos.z > shipDepthZ
    IN_FRONT    // Only particles with cameraRelPos.z <= shipDepthZ
};

// Internal helper: buffer particles with depth filtering
static void bufferParticlesFiltered(const Camera &camera, Fixed shipDepthZ, DepthFilter filter)
{
    using namespace GameConstants;

    int count = particleSystem.getParticleCount();

    // Get camera tile position for visibility calculation
    int camTileX = camera.getXTile().toInt();
    int camTileZ = camera.getZTile().toInt();

    // Calculate visible tile bounds
    int halfTilesX = TILES_X / 2;
    int minVisibleX = camTileX - halfTilesX;
    int maxVisibleX = camTileX + halfTilesX;
    int minVisibleZ = camTileZ;
    int maxVisibleZ = camTileZ + TILES_Z - 1;

    for (int i = 0; i < count; i++)
    {
        const Particle &p = particleSystem.getParticle(i);

        // Skip rocks - they're rendered as 3D objects
        if (p.isRock())
        {
            continue;
        }

        // Transform particle position to camera-relative coordinates
        Vec3 cameraRelPos = camera.worldToCamera(p.position);

        // Skip particles behind the camera
        if (cameraRelPos.z.raw <= 0)
        {
            continue;
        }

        // Apply depth filter relative to ship
        bool isBehindShip = cameraRelPos.z.raw > shipDepthZ.raw;
        if (filter == DepthFilter::BEHIND && !isBehindShip)
        {
            continue;  // Skip particles in front when filtering for behind
        }
        if (filter == DepthFilter::IN_FRONT && isBehindShip)
        {
            continue;  // Skip particles behind when filtering for in front
        }

        // Calculate particle's tile position (accounting for visual Z offset)
        // Particles have a Z offset of 10 tiles to match ship visual position
        constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10; // tiles
        int particleTileX = p.position.x.toInt();
        int particleTileZ = p.position.z.toInt() - SHIP_VISUAL_Z_OFFSET;

        // Skip particles outside visible tile bounds
        if (particleTileX < minVisibleX || particleTileX > maxVisibleX ||
            particleTileZ < minVisibleZ || particleTileZ > maxVisibleZ)
        {
            continue;
        }

        // Calculate row for depth sorting
        int row = camTileZ + TILES_Z - 1 - particleTileZ;

        // Get terrain height for shadow (same offset logic as renderParticles)
        constexpr int32_t SHIP_VISUAL_Z_OFFSET_RAW = 10 * 0x01000000;
        Fixed terrainLookupZ = Fixed::fromRaw(p.position.z.raw - SHIP_VISUAL_Z_OFFSET_RAW);
        Fixed terrainY = getLandscapeAltitude(p.position.x, terrainLookupZ);

        // Shadow position: at particle's visual X/Z, but at terrain height
        Vec3 shadowWorldPos;
        shadowWorldPos.x = p.position.x;
        shadowWorldPos.y = terrainY;
        shadowWorldPos.z = p.position.z;
        Vec3 shadowRelPos = camera.worldToCamera(shadowWorldPos);

        // Buffer shadow first (so it appears under the particle)
        if (shadowRelPos.z.raw > 0)
        {
            ProjectedVertex shadowProj = projectVertex(shadowRelPos);
            if (shadowProj.visible && shadowProj.onScreen)
            {
                bufferRect(row, shadowProj.screenX, shadowProj.screenY,
                           getShadowWidth(), getShadowHeight(), Color::black(), true);
            }
        }

        // Project and buffer particle
        ProjectedVertex proj = projectVertex(cameraRelPos);
        if (proj.visible && proj.onScreen)
        {
            // Get particle color from palette (VIDC 256-color format)
            uint8_t colorIndex = p.getColorIndex();
            Color color = vidc256ToColor(colorIndex);

            // If particle has fading flag, cycle through white -> yellow -> orange -> red
            if (p.hasFading())
            {
                int life = p.lifespan * 16;
                if (life > 255)
                    life = 255;

                color.r = 255;
                if (life > 192)
                {
                    color.g = 255;
                    color.b = static_cast<uint8_t>((life - 192) * 4);
                }
                else if (life > 64)
                {
                    color.g = static_cast<uint8_t>(128 + (life - 64));
                    color.b = 0;
                }
                else
                {
                    color.g = static_cast<uint8_t>(life * 2);
                    color.b = 0;
                }
            }

            bufferRect(row, proj.screenX, proj.screenY,
                       getParticleWidth(), getParticleHeight(), color, false);
        }
    }
}

void bufferParticlesBehind(const Camera &camera, Fixed shipDepthZ)
{
    bufferParticlesFiltered(camera, shipDepthZ, DepthFilter::BEHIND);
}

void bufferParticlesInFront(const Camera &camera, Fixed shipDepthZ)
{
    bufferParticlesFiltered(camera, shipDepthZ, DepthFilter::IN_FRONT);
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

namespace
{
    // Simple pseudo-random number generator for particle variation
    // Uses a static seed that advances each call
    uint32_t exhaustRandomSeed = 0x12345678;

    int32_t exhaustRandom()
    {
        // Linear congruential generator
        exhaustRandomSeed = exhaustRandomSeed * 1103515245 + 12345;
        return static_cast<int32_t>(exhaustRandomSeed);
    }

    // Add a single exhaust particle with randomization
    void addExhaustParticle(const Vec3 &basePos, const Vec3 &baseVel,
                            int32_t baseLifespan, uint32_t flags)
    {
        // Add random variation to velocity
        // Original uses +/- 2^(32 - 10) = +/- 0x400000
        // Scaled for our 8.24 format: >> 7 gives similar range
        constexpr int32_t VEL_RANDOM_RANGE = 0x80000; // ~0.5 tiles/frame variation

        Vec3 particleVel = baseVel;
        particleVel.x = Fixed::fromRaw(particleVel.x.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE / 2);
        particleVel.y = Fixed::fromRaw(particleVel.y.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE / 2);
        particleVel.z = Fixed::fromRaw(particleVel.z.raw + ((exhaustRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE / 2);

        // Add random variation to lifespan (0 to 8 extra frames)
        int32_t lifespan = baseLifespan + ((exhaustRandom() >> 24) & 0x07);

        particleSystem.addParticle(basePos, particleVel, lifespan, flags);
    }
}

void spawnExhaustParticles(const Vec3 &pos, const Vec3 &vel, const Vec3 &exhaust, bool fullThrust)
{
    // Calculate particle velocity: shipVel + exhaustDir * exhaustSpeed
    // The exhaust direction is normalized (~1.0 in 8.24), scale for exhaust speed
    // Exhaust should move in the exhaust direction relative to the ship
    constexpr int32_t EXHAUST_SPEED_SHIFT = 3; // >> 3 = 0.125 tiles/frame exhaust speed (halved)

    Vec3 particleVel;
    particleVel.x = Fixed::fromRaw(vel.x.raw + (exhaust.x.raw >> EXHAUST_SPEED_SHIFT));
    particleVel.y = Fixed::fromRaw(vel.y.raw + (exhaust.y.raw >> EXHAUST_SPEED_SHIFT));
    particleVel.z = Fixed::fromRaw(vel.z.raw + (exhaust.z.raw >> EXHAUST_SPEED_SHIFT));

    // Calculate spawn position: at exhaust port, offset along exhaust direction
    //
    // Original uses exhaust >> 7 (divide by 128) as offset from ship center.
    // We add a similar offset along the exhaust direction to push particles
    // away from the ship, preventing them from appearing inside the ship.
    //
    // The offset varies based on ship orientation relative to camera:
    // - Default: smaller offset (>> 3) works most of the time
    // - When top clearly faces camera (exhaust.z > 0.5): larger offset (>> 1) needed
    constexpr int32_t EXHAUST_Z_THRESHOLD = 0x00400000; // 0.25 in 8.24 format
    int32_t offsetShift = (exhaust.z.raw > EXHAUST_Z_THRESHOLD) ? 1 : 2;

    // The ship is rendered at Z=15 from camera for consistent visual size, but
    // the player's actual position is only Z=5 from camera. We offset particle Z
    // by 10 tiles to match the ship's visual position.
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000; // 10 tiles in 8.24 format

    Vec3 particlePos;
    particlePos.x = Fixed::fromRaw(pos.x.raw + (exhaust.x.raw >> offsetShift));
    particlePos.y = Fixed::fromRaw(pos.y.raw + (exhaust.y.raw >> offsetShift));
    particlePos.z = Fixed::fromRaw(pos.z.raw + (exhaust.z.raw >> offsetShift) + SHIP_VISUAL_Z_OFFSET);

    // Particle flags: FADING | SPLASH | BOUNCES | GRAVITY
    // Exhaust particles splash in water, bounce off terrain
    uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                     ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;

    // Base lifespan: 8 frames at 15fps = ~64 frames at 120fps
    // But we want shorter exhaust trails, so scale down
    constexpr int32_t BASE_LIFESPAN = 16; // Tuned for visual appeal

    // Spawn particles (reduced to 1/4 rate for Task 39 tuning)
    int count = fullThrust ? 2 : 1;
    for (int i = 0; i < count; i++)
    {
        addExhaustParticle(particlePos, particleVel, BASE_LIFESPAN, flags);
    }
}

// =============================================================================
// Bullet Particle Spawning Implementation
// =============================================================================
//
// Port of MoveAndDrawPlayer Part 5 (Lander.arm lines 2369-2465).
//
// The original algorithm at 15fps:
// 1. Velocity = playerVel + gunDir >> 8
// 2. Position = playerPos - velocity + gunDir >> 7
//    (compensates for first velocity update + offsets to gun muzzle)
// 3. Lifespan = 20 frames
// 4. Flags = 0x01BC00FF
//
// At 120fps (8x), we scale accounting for format difference:
// - Original uses 31-bit values (1.0 ≈ 0x7FFFFFFF)
// - We use 8.24 format (1.0 = 0x01000000), ~128x smaller (2^7)
// - Velocity shift: 8 - 7 + 3 = 4 (format diff + frame rate scaling)
// - Position offset shift: 7 - 7 + 3 = 3
// - Lifespan: 20 * 8 = 160 frames
//
// =============================================================================

void spawnBulletParticle(const Vec3 &pos, const Vec3 &vel, const Vec3 &gunDir)
{
    // Calculate bullet velocity: playerVel + gunDir >> 4
    // Tuned for gameplay feel (original speed)
    Vec3 bulletVel;
    bulletVel.x = Fixed::fromRaw(vel.x.raw + (gunDir.x.raw >> 4));
    bulletVel.y = Fixed::fromRaw(vel.y.raw + (gunDir.y.raw >> 4));
    bulletVel.z = Fixed::fromRaw(vel.z.raw + (gunDir.z.raw >> 4));

    // Spawn position: just use the provided spawn point with Z offset for visual display
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000; // 10 tiles in 8.24 format

    Vec3 bulletPos;
    bulletPos.x = pos.x;
    bulletPos.y = pos.y;
    bulletPos.z = Fixed::fromRaw(pos.z.raw + SHIP_VISUAL_Z_OFFSET);

    // Bullet flags: splash, bounce, gravity, destroy objects, big splash, explode on ground
    uint32_t flags = ParticleFlags::SPLASH | ParticleFlags::BOUNCES |
                     ParticleFlags::GRAVITY | ParticleFlags::DESTROYS_OBJECTS |
                     ParticleFlags::BIG_SPLASH | ParticleFlags::EXPLODES_ON_GROUND | 0xFF;

    // Lifespan: 20 frames at 15fps = 160 frames at 120fps
    constexpr int32_t BULLET_LIFESPAN = 160;

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

void spawnSplashParticles(const Vec3 &pos, const Vec3 &impactVel, bool bigSplash)
{
    // Spawn 1-3 particles (random count)
    int count = 1 + ((exhaustRandom() >> 30) & 0x03); // 1 to 3

    // Use impact velocity as bias (scaled down) for splash direction
    // X and Z inherit some of the bullet's horizontal momentum
    // Y is always upward (negative) regardless of impact direction
    int32_t biasX = impactVel.x.raw >> 4; // 1/16 of impact velocity
    int32_t biasZ = impactVel.z.raw >> 4;

    for (int i = 0; i < count; i++)
    {
        // Random velocity with impact bias
        int32_t vx = biasX + (exhaustRandom() >> 13) - 0x040000;
        int32_t vy = -(exhaustRandom() >> 12) - 0x080000; // Always upward
        int32_t vz = biasZ + (exhaustRandom() >> 13) - 0x040000;

        Vec3 vel;
        vel.x = Fixed::fromRaw(vx);
        vel.y = Fixed::fromRaw(vy);
        vel.z = Fixed::fromRaw(vz);

        // Light blue color (VIDC palette index for cyan/light blue)
        uint8_t colorIndex = 0xCB; // Light blue (R=51, G=187, B=255)

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

void spawnSparkParticles(const Vec3 &pos, const Vec3 &impactVel)
{
    constexpr int SPARK_COUNT = 8;

    // Use impact velocity as bias (scaled down) for spark direction
    // Sparks bounce back so we negate Y and add some horizontal spread
    int32_t biasX = impactVel.x.raw >> 4; // 1/16 of impact velocity
    int32_t biasZ = impactVel.z.raw >> 4;

    for (int i = 0; i < SPARK_COUNT; i++)
    {
        // Random velocity with impact bias
        int32_t vx = biasX + (exhaustRandom() >> 13) - 0x040000;
        int32_t vy = (exhaustRandom() >> 12) - 0x0C0000; // Upward (bounce back)
        int32_t vz = biasZ + (exhaustRandom() >> 13) - 0x040000;

        Vec3 vel;
        vel.x = Fixed::fromRaw(vx);
        vel.y = Fixed::fromRaw(vy);
        vel.z = Fixed::fromRaw(vz);

        // Yellow/orange color - use FADING flag to get white->yellow->orange->red
        // Color index doesn't matter much when FADING is set
        uint8_t colorIndex = 0xFF; // White base

        // Flags: FADING | GRAVITY | BOUNCES (sparks can bounce)
        uint32_t flags = ParticleFlags::FADING | ParticleFlags::GRAVITY |
                         ParticleFlags::BOUNCES | colorIndex;

        // Longer lifespan for visible spark effect
        int32_t lifespan = 12 + ((exhaustRandom() >> 28) & 0x0F);

        particleSystem.addParticle(pos, vel, lifespan, flags);
    }
}

// =============================================================================
// Explosion Particle Spawning Implementation
// =============================================================================
//
// Port of AddExplosionToBuffer from Lander.arm (lines 4406-4438).
//
// An explosion consists of clusters, each with 4 particles:
// - 2x Spark particles (white fading to red)
// - 1x Debris particle (purple-brown-green bouncing)
// - 1x Smoke particle (grey rising)
//
// Cluster count parameter determines explosion size:
// - Small explosion (bullets hitting things): 3 clusters = 12 particles
// - Medium explosion (objects destroyed): 20 clusters = 80 particles
// - Large explosion (ship crash): 50 clusters = 200 particles
//
// =============================================================================

// Helper to generate random VIDC color for debris (purple-brown-green)
static uint8_t generateDebrisColor()
{
    // From original: R=4-11, G=2-9, B=4-7
    uint32_t rand1 = exhaustRandom();
    uint32_t rand2 = exhaustRandom();

    int red = 4 + (rand1 & 0x07);          // 4-11
    int green = 2 + ((rand1 >> 8) & 0x07); // 2-9
    int blue = 4 + ((rand2 >> 16) & 0x03); // 4-7

    // Build VIDC color using palette.cpp encoding
    return buildVidcColor(red, green, blue);
}

// Helper to generate random VIDC color for smoke (grey)
static uint8_t generateSmokeColor()
{
    // From original: all channels equal, 3-10
    uint32_t rand = exhaustRandom();
    int intensity = 3 + (rand & 0x07); // 3-10

    return buildVidcColor(intensity, intensity, intensity);
}

void spawnExplosionParticles(const Vec3 &pos, int clusterCount)
{
    // Offset position upward slightly (10 tiles in z-offset for visual display)
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000;
    Vec3 explosionPos = pos;
    explosionPos.z = Fixed::fromRaw(pos.z.raw + SHIP_VISUAL_Z_OFFSET);

    // Frame rate scaling: original was 15fps, we run at 120fps (8x faster)
    // Add 3 bits to shift values to slow down velocities by 8x
    constexpr int FPS_SHIFT = 3;

    for (int cluster = 0; cluster < clusterCount; cluster++)
    {
        // === SPARK PARTICLE 1 ===
        {
            // Random velocity (original: shift 8, scaled for 120fps: shift 11)
            int32_t vx = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));
            int32_t vy = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));
            int32_t vz = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));

            Vec3 vel;
            vel.x = Fixed::fromRaw(vx);
            vel.y = Fixed::fromRaw(vy);
            vel.z = Fixed::fromRaw(vz);

            // Spark: FADING | SPLASH | BOUNCES | GRAVITY
            uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                             ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;

            // Lifespan: 8 base + random 0-8 (original: 8 + random>>29)
            int32_t lifespan = 64 + ((exhaustRandom() >> 26) & 0x3F); // Scaled for 120fps

            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }

        // === DEBRIS PARTICLE ===
        {
            // Random velocity (original: shift 10, scaled for 120fps: shift 13)
            int32_t vx = (static_cast<int32_t>(exhaustRandom()) >> (10 + FPS_SHIFT));
            int32_t vy = (static_cast<int32_t>(exhaustRandom()) >> (10 + FPS_SHIFT));
            int32_t vz = (static_cast<int32_t>(exhaustRandom()) >> (10 + FPS_SHIFT));

            Vec3 vel;
            vel.x = Fixed::fromRaw(vx);
            vel.y = Fixed::fromRaw(vy);
            vel.z = Fixed::fromRaw(vz);

            // Debris color (purple-brown-green)
            uint8_t color = generateDebrisColor();

            // Debris: SPLASH | BOUNCES | GRAVITY
            uint32_t flags = ParticleFlags::SPLASH | ParticleFlags::BOUNCES |
                             ParticleFlags::GRAVITY | color;

            // Lifespan: 15 base + random 0-64 (scaled for 120fps)
            int32_t lifespan = 120 + ((exhaustRandom() >> 24) & 0xFF);

            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }

        // === SMOKE PARTICLE ===
        {
            // Smoke rises slowly - base upward velocity with small random spread
            // Original: MVN R4, #SMOKE_RISING_SPEED (negative = upward)
            // Scaled for 120fps (divide by 8)
            constexpr int32_t SMOKE_RISING_SPEED = -0x8000; // Slow rise (was -0x40000)

            int32_t vx = (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));
            int32_t vy = SMOKE_RISING_SPEED + (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));
            int32_t vz = (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));

            Vec3 vel;
            vel.x = Fixed::fromRaw(vx);
            vel.y = Fixed::fromRaw(vy);
            vel.z = Fixed::fromRaw(vz);

            // Smoke color (grey)
            uint8_t color = generateSmokeColor();

            // Smoke: BOUNCES only (no gravity - it rises!)
            uint32_t flags = ParticleFlags::BOUNCES | color;

            // Lifespan: 15 base + random 0-128 (scaled for 120fps)
            int32_t lifespan = 120 + ((exhaustRandom() >> 23) & 0x1FF);

            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }

        // === SPARK PARTICLE 2 ===
        {
            // Random velocity (same as spark 1, scaled for 120fps)
            int32_t vx = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));
            int32_t vy = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));
            int32_t vz = (static_cast<int32_t>(exhaustRandom()) >> (8 + FPS_SHIFT));

            Vec3 vel;
            vel.x = Fixed::fromRaw(vx);
            vel.y = Fixed::fromRaw(vy);
            vel.z = Fixed::fromRaw(vz);

            // Spark: FADING | SPLASH | BOUNCES | GRAVITY
            uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                             ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;

            // Lifespan: same as spark 1
            int32_t lifespan = 64 + ((exhaustRandom() >> 26) & 0x3F);

            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }
    }
}

// =============================================================================
// Smoke Particle Spawning (for destroyed objects)
// =============================================================================
//
// Port of AddSmokeParticleToBuffer from Lander.arm (lines 3885-3977).
//
// Smoke particles rise slowly from destroyed objects:
// - Grey color (random intensity 3-10)
// - Rising velocity (negative Y = upward)
// - Random velocity spread in all directions
// - Bounces off terrain
// - Short lifespan with random variation
//
// =============================================================================

void spawnSmokeParticle(const Vec3 &pos)
{
    // Offset position with visual Z offset (particles use visual coordinates)
    constexpr int32_t SHIP_VISUAL_Z_OFFSET = 10 * 0x01000000; // 10 tiles in 8.24 format
    Vec3 smokePos = pos;
    smokePos.z = Fixed::fromRaw(pos.z.raw + SHIP_VISUAL_Z_OFFSET);

    // Frame rate scaling: original was 15fps, we run at 120fps (8x faster)
    constexpr int FPS_SHIFT = 3;

    // Smoke rises slowly - base upward velocity with random spread
    // Original: MVN R4, #SMOKE_RISING_SPEED where SMOKE_RISING_SPEED = 0x00080000
    // At 15fps this is -0x80000. At 120fps we scale by 1/8: -0x10000
    constexpr int32_t SMOKE_RISING_SPEED = -0x10000;

    // Random velocity variation (original uses shift 13, we add 3 for fps)
    int32_t vx = (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));
    int32_t vy = SMOKE_RISING_SPEED + (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));
    int32_t vz = (static_cast<int32_t>(exhaustRandom()) >> (13 + FPS_SHIFT));

    Vec3 vel;
    vel.x = Fixed::fromRaw(vx);
    vel.y = Fixed::fromRaw(vy);
    vel.z = Fixed::fromRaw(vz);

    // Smoke color (grey, random intensity 3-10)
    uint8_t color = generateSmokeColor();

    // Smoke: BOUNCES only (no gravity - it rises!)
    uint32_t flags = ParticleFlags::BOUNCES | color;

    // Lifespan: short-lived for subtle effect
    // ~120 frames base + small random variation
    int32_t lifespan = 120 + ((exhaustRandom() >> 22) & 0xFF);

    particleSystem.addParticle(smokePos, vel, lifespan, flags);
}

// =============================================================================
// Rock Spawning Implementation
// =============================================================================
//
// Port of DropARockFromTheSky from Lander.arm (lines 4120-4224).
//
// Rocks are special particles rendered as 3D objects:
// - Have IS_ROCK flag set
// - Fall under gravity
// - Bounce off terrain, splash into sea
// - Can destroy objects on collision
// - Kill the player on collision
//
// =============================================================================

// Global rotation angle for all rocks (they spin together)
// This matches original behavior where rocks share a rotation matrix
static int32_t rockRotationAngle = 0;

void spawnRock(const Vec3& pos)
{
    // Generate random rock color (purple-brown-green)
    // From original: red 4-11, green 2-9, blue 4-7
    uint32_t rand1 = exhaustRandom();
    uint32_t rand2 = exhaustRandom();

    int r = 4 + (rand1 & 7);           // 4-11
    int g = 2 + ((rand1 >> 29) & 7);   // 2-9
    int b = 4 + ((rand2 >> 30) & 3);   // 4-7

    // Build VIDC 256-color index from RGB components
    // Bits: 7=b3, 6=g3, 5=g2, 4=r3, 3=b2, 2=r2, 1=(r1|g1|b1), 0=(r0|g0|b0)
    uint8_t colorIndex = 0;
    colorIndex |= (r & 3) | ((g & 3) | (b & 3));           // bits 0-1
    colorIndex |= ((r >> 2) & 1) << 2;                      // bit 2
    colorIndex |= ((b >> 2) & 1) << 3;                      // bit 3
    colorIndex |= ((r >> 3) & 1) << 4;                      // bit 4
    colorIndex |= ((g >> 2) & 3) << 5;                      // bits 5-6
    colorIndex |= ((b >> 3) & 1) << 7;                      // bit 7

    // Rock flags: IS_ROCK | SPLASH | BOUNCES | GRAVITY | DESTROYS_OBJECTS | BIG_SPLASH | EXPLODES_ON_GROUND
    // Original: bits 17-24 = 0xFE (all rock behaviors)
    uint32_t flags = ParticleFlags::IS_ROCK |
                     ParticleFlags::SPLASH |
                     ParticleFlags::BOUNCES |
                     ParticleFlags::GRAVITY |
                     ParticleFlags::DESTROYS_OBJECTS |
                     ParticleFlags::BIG_SPLASH |
                     ParticleFlags::EXPLODES_ON_GROUND |
                     colorIndex;

    // Rock starts with zero velocity (just falls)
    // Original adds small random velocity variation
    // Shift by 16 gives tiny drift (~0.004 tiles/frame max)
    Vec3 vel;
    vel.x = Fixed::fromRaw((static_cast<int32_t>(exhaustRandom()) >> 16));
    vel.y = Fixed::fromRaw(0);  // Starts stationary, gravity will accelerate it
    vel.z = Fixed::fromRaw((static_cast<int32_t>(exhaustRandom()) >> 16));

    // Lifespan: 170 iterations at 15fps = 1360 iterations at 120fps
    // Long enough to fall from 32 tiles
    int32_t lifespan = 1360 + ((exhaustRandom() >> 27) & 0x1F);

    // Rocks are stored in actual world coordinates (no visual offset)
    // This is different from exhaust/bullets which need the +10 tile offset
    // to appear at the visual ship position (15 tiles from camera vs 5 actual)
    particleSystem.addParticle(pos, vel, lifespan, flags);
}

int getRockCount()
{
    int count = 0;
    int total = particleSystem.getParticleCount();
    for (int i = 0; i < total; i++) {
        if (particleSystem.getParticle(i).isRock()) {
            count++;
        }
    }
    return count;
}

// Update the global rock rotation angle (call once per frame)
void updateRockRotation()
{
    // Rocks rotate at a fixed speed
    // Original used main loop counter; we increment each frame
    rockRotationAngle += 0x02000000;  // ~45 degrees per second at 120fps
}

void bufferRocks(const Camera& camera)
{
    // Update rock rotation
    updateRockRotation();

    // Calculate rotation matrix for all rocks
    Mat3x3 rockRotation = calculateRotationMatrix(rockRotationAngle, rockRotationAngle >> 1);

    int count = particleSystem.getParticleCount();
    int camTileX = camera.getXTile().toInt();
    int camTileZ = camera.getZTile().toInt();

    // Calculate visible tile bounds (same as regular particles)
    int halfTilesX = TILES_X / 2;
    int minVisibleX = camTileX - halfTilesX;
    int maxVisibleX = camTileX + halfTilesX;
    int minVisibleZ = camTileZ;
    int maxVisibleZ = camTileZ + TILES_Z - 1;

    for (int i = 0; i < count; i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (!p.isRock()) {
            continue;
        }

        // Rocks are in world coordinates (no visual offset)
        int rockTileX = p.position.x.toInt();
        int rockTileZ = p.position.z.toInt();

        // Skip rocks outside visible tile bounds
        if (rockTileX < minVisibleX || rockTileX > maxVisibleX ||
            rockTileZ < minVisibleZ || rockTileZ > maxVisibleZ)
        {
            continue;
        }

        // Transform rock position to camera-relative coordinates
        // Add 10-tile visual offset to Z so rocks render at the visual ship position
        // (Ship renders at 15 tiles from camera, but actual player is at 5 tiles)
        Vec3 cameraRelPos = camera.worldToCamera(p.position);
        constexpr int32_t VISUAL_Z_OFFSET = 10 * 0x01000000;  // 10 tiles
        cameraRelPos.z = Fixed::fromRaw(cameraRelPos.z.raw + VISUAL_Z_OFFSET);

        // Skip rocks behind the camera or too close (would cause projection overflow)
        // Minimum distance of 1 tile prevents division-by-small-number issues
        constexpr int32_t MIN_RENDER_Z = 0x01000000;  // 1 tile in 8.24 format
        if (cameraRelPos.z.raw <= MIN_RENDER_Z) {
            continue;
        }

        // Calculate row for depth sorting
        // Use the actual world Z (not visual) to match ship's row calculation
        int row = camTileZ + TILES_Z - 1 - rockTileZ;

        // Clamp to valid row range
        if (row < 0) row = 0;
        if (row >= TILES_Z) row = TILES_Z - 1;

        // Buffer rock shadow first (so it appears under the rock)
        Vec3 cameraWorldPos;
        cameraWorldPos.x = camera.getX();
        cameraWorldPos.y = camera.getY();
        cameraWorldPos.z = camera.getZ();
        bufferObjectShadow(rockBlueprint, cameraRelPos, rockRotation,
                           p.position, cameraWorldPos, row);

        // Buffer rock as 3D object
        bufferObject(rockBlueprint, cameraRelPos, rockRotation, row);
    }
}

void renderRocks(const Camera& camera, ScreenBuffer& screen)
{
    // Rocks are buffered via bufferRocks() and rendered during landscape rendering
    // This function is for immediate-mode rendering (debugging)

    Mat3x3 rockRotation = calculateRotationMatrix(rockRotationAngle, rockRotationAngle >> 1);

    int count = particleSystem.getParticleCount();

    for (int i = 0; i < count; i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (!p.isRock()) {
            continue;
        }

        Vec3 cameraRelPos = camera.worldToCamera(p.position);

        // Skip rocks behind or too close to camera
        constexpr int32_t MIN_RENDER_Z = 0x01000000;  // 1 tile
        if (cameraRelPos.z.raw <= MIN_RENDER_Z) {
            continue;
        }

        // Draw rock object directly
        drawObject(rockBlueprint, cameraRelPos, rockRotation, screen);
    }
}

// =============================================================================
// Rock-Player Collision Detection
// =============================================================================
//
// Port of rock collision from MoveAndDrawParticles Part 3 (Lander.arm lines 2955-3014).
//
// Original logic:
// - Rock is at world coordinates (R0, R1, R2)
// - Player is at (0, yPlayer, 15 tiles from camera in Z)
// - Check if |rock.x| < TILE_SIZE and |rock.z - 15 tiles| < TILE_SIZE
// - If so, check if |rock.y - yPlayer| < TILE_SIZE
// - Rock is 1 tile in size on each axis
//
// =============================================================================

bool checkRockPlayerCollision(const Vec3& playerPos, const Vec3& cameraPos)
{
    // Rocks and player are both in world coordinates
    // Check if any rock is within collision radius of player position
    (void)cameraPos;  // Not needed - both rock and player are in world coords

    // Collision radius: 1 tile for X/Z, 1 tile for Y
    constexpr int32_t COLLISION_RADIUS_XZ = 1 * 0x01000000;  // 1 tile
    constexpr int32_t COLLISION_RADIUS_Y = 1 * 0x01000000;   // 1 tile

    int count = particleSystem.getParticleCount();

    for (int i = 0; i < count; i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (!p.isRock()) {
            continue;
        }

        // Check X distance
        int32_t xDiff = p.position.x.raw - playerPos.x.raw;
        int32_t absX = (xDiff < 0) ? -xDiff : xDiff;
        if (absX >= COLLISION_RADIUS_XZ) {
            continue;
        }

        // Check Z distance
        int32_t zDiff = p.position.z.raw - playerPos.z.raw;
        int32_t absZ = (zDiff < 0) ? -zDiff : zDiff;
        if (absZ >= COLLISION_RADIUS_XZ) {
            continue;
        }

        // Check Y distance (altitude)
        int32_t yDiff = p.position.y.raw - playerPos.y.raw;
        int32_t absY = (yDiff < 0) ? -yDiff : yDiff;
        if (absY >= COLLISION_RADIUS_Y) {
            continue;
        }

        // Collision detected!
        particleEvents.rockHitPlayer++;
        particleEvents.rockHitPlayerPos = p.position;
        return true;
    }

    return false;
}
