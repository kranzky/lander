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
#include "clipping.h"
#include <algorithm>

using namespace ParticleConstants;

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

ParticleSystem::ParticleSystem()
{
    clear();
}

void ParticleSystem::clear()
{
    particleCount = 0;
}

Particle* ParticleSystem::addParticle(const Vec3 &pos, const Vec3 &vel, int32_t lifespan, uint32_t flags)
{
    if (particleCount >= MAX_PARTICLES)
    {
        return nullptr;
    }

    Particle &p = particles[particleCount++];
    p = Particle{};
    p.position = pos;
    p.velocity = vel;
    p.lifespan = lifespan;
    p.flags = flags;
    return &p;
}

void ParticleSystem::removeParticle(int index)
{
    // Swap with last particle and decrement count (swap-and-pop)
    particles[index] = particles[--particleCount];
}

void ParticleSystem::update()
{
    // Reset event counters for this frame
    particleEvents.reset();

    // Iterate backwards so swap-and-pop removal never skips a particle. New
    // particles spawned during the loop are appended past index i, so they are
    // first updated next frame.
    for (int i = particleCount - 1; i >= 0; i--)
    {
        Particle &p = particles[i];

        p.lifespan--;
        if (p.lifespan <= 0)
        {
            removeParticle(i);
            continue;
        }

        // Stars are stationary and always far above the landscape
        if (p.isStar())
        {
            continue;
        }

        p.position += p.velocity;
        if (p.hasGravity())
        {
            p.velocity.y += Fixed::fromRaw(PARTICLE_GRAVITY);
        }

        // Terrain lookups use world coordinates (rocks are already stored that way)
        Fixed worldZ = p.isRock() ? p.position.z : p.position.z - VISUAL_Z_OFFSET;
        Fixed terrainY = getLandscapeAltitude(p.position.x, worldZ);

        // Height above the terrain (positive Y is down)
        int32_t heightAboveTerrain = terrainY.raw - p.position.y.raw;

        // =================================================================
        // Object Collision Detection
        // =================================================================
        // Port of ProcessObjectDestruction from Lander.arm (lines 3290-3364).
        //
        // If particle has DESTROYS_OBJECTS flag and is within SAFE_HEIGHT of
        // terrain, check if there's an object at the particle's tile position.
        // If so, mark the object as destroyed and spawn an explosion.
        //
        if (p.canDestroyObjects() && heightAboveTerrain < GameConstants::SAFE_HEIGHT.raw)
        {
            // The object map wraps every 256 tiles, as does the integer part
            uint8_t tileX = static_cast<uint8_t>(p.position.x.toInt());
            uint8_t tileZ = static_cast<uint8_t>(worldZ.toInt());
            uint8_t objectType = objectMap.getObjectAt(tileX, tileZ);

            // Destroyed objects don't stop bullets; they pass straight through
            if (objectType != ObjectType::NONE && !ObjectMap::isDestroyedType(objectType))
            {
                objectMap.setObjectAt(tileX, tileZ, ObjectMap::getDestroyedType(objectType));

                // Explode half a tile above the object's tile corner (which is
                // where objects are drawn)
                Vec3 objectPos;
                objectPos.x = p.position.x.floor();
                objectPos.z = worldZ.floor();
                objectPos.y = getLandscapeAltitude(objectPos.x, objectPos.z) -
                              Fixed::fromRaw(GameConstants::TILE_SIZE.raw >> 1);

                // Medium explosion (20 clusters = 80 particles)
                spawnExplosionParticles(objectPos, 20);

                particleEvents.objectDestroyed++;
                particleEvents.objectDestroyedPos = objectPos;

                removeParticle(i);
                continue;
            }
        }

        // Rocks explode 1 tile above ground/water
        if (p.isRock() && heightAboveTerrain <= GameConstants::TILE_SIZE.raw)
        {
            // spawnExplosionParticles adds the visual Z offset, which matches
            // the offset used when rendering rocks
            spawnExplosionParticles(p.position, 20);
            particleEvents.rockExploded++;
            particleEvents.rockExplodedPos = p.position;
            removeParticle(i);
            continue;
        }

        // Nothing more to do unless the particle is below the terrain
        if (heightAboveTerrain >= 0)
        {
            continue;
        }

        // Place particle on surface
        p.position.y = terrainY;

        bool isWater = (terrainY == GameConstants::SEA_LEVEL);

        if (isWater && p.splashesInSea())
        {
            // Splash into sea from 1/16 tile above the surface
            Vec3 splashPos = p.position;
            splashPos.y -= GameConstants::SPLASH_HEIGHT;
            spawnSplashParticles(splashPos, p.velocity);

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
            // Bullet hit terrain
            spawnSparkParticles(p.position, p.velocity);
            particleEvents.bulletHitGround++;
            particleEvents.bulletHitGroundPos = p.position;
            removeParticle(i);
            continue;
        }

        if (!p.bouncesOffTerrain())
        {
            removeParticle(i);
            continue;
        }

        // Bounce: reflect Y velocity and dampen all velocities
        p.velocity.y = -(p.velocity.y >> BOUNCE_DAMPING_SHIFT);
        p.velocity.x = p.velocity.x >> BOUNCE_DAMPING_SHIFT;
        p.velocity.z = p.velocity.z >> BOUNCE_DAMPING_SHIFT;
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
    struct RectSize {
        int width;
        int height;
    };

    // Particle (and particle shadow) size for the current display resolution
    RectSize particleSize()
    {
        switch (DisplayConfig::scale) {
            case 4:  return {8, 6};  // 1280x1024
            case 2:  return {4, 3};  // 640x512
            default: return {2, 2};  // 320x256
        }
    }

    // Buffer a filled rectangle centred on (x, y) as two triangles
    void bufferRect(int row, int x, int y, RectSize size, Color color, bool isShadow)
    {
        // The rasterizer treats coordinates inclusively, so the far edges are
        // at width-1 and height-1 (clamped to at least a 1-pixel rect)
        int left = x - size.width / 2;
        int top = y - size.height / 2;
        int right = left + std::max(size.width - 1, 0);
        int bottom = top + std::max(size.height - 1, 0);

        auto add = isShadow ? &GraphicsBufferSystem::addShadowTriangle
                            : &GraphicsBufferSystem::addTriangle;
        (graphicsBuffers.*add)(row, left, top, right, top, left, bottom, color);
        (graphicsBuffers.*add)(row, right, top, right, bottom, left, bottom, color);
    }

    // Colour of a FADING particle: white -> yellow -> orange -> red as it ages
    Color fadingColor(int32_t lifespan)
    {
        int life = std::min(lifespan * 16, 255);

        if (life > 192)
            return Color(255, 255, static_cast<uint8_t>((life - 192) * 4));
        if (life > 64)
            return Color(255, static_cast<uint8_t>(128 + (life - 64)), 0);
        return Color(255, static_cast<uint8_t>(life * 2), 0);
    }

    // Particles are split into two passes for correct depth sorting with the ship
    enum class DepthFilter {
        BEHIND,     // Only particles with cameraRelPos.z > shipDepthZ
        IN_FRONT    // Only particles with cameraRelPos.z <= shipDepthZ
    };

    void bufferParticlesFiltered(const Camera &camera, Fixed shipDepthZ, DepthFilter filter)
    {
        int halfTilesX = TILES_X / 2;
        RectSize size = particleSize();

        for (int i = 0; i < particleSystem.getParticleCount(); i++)
        {
            const Particle &p = particleSystem.getParticle(i);

            // Rocks are rendered as 3D objects and stars separately without shadows
            if (p.isRock() || p.isStar())
            {
                continue;
            }

            Vec3 cameraRelPos = camera.worldToCamera(p.position);

            // Skip particles behind the camera
            if (cameraRelPos.z.raw <= 0)
            {
                continue;
            }

            bool isBehindShip = cameraRelPos.z > shipDepthZ;
            if (isBehindShip != (filter == DepthFilter::BEHIND))
            {
                continue;
            }

            // Skip particles outside the visible tile grid
            Fixed worldZ = p.position.z - VISUAL_Z_OFFSET;
            int tileX = camera.tileOffsetX(p.position.x);
            int row = camera.rowForZ(worldZ);
            if (tileX < -halfTilesX || tileX > halfTilesX || row < 0 || row >= TILES_Z)
            {
                continue;
            }

            // Shadow: at the particle's visual X/Z, but at terrain height.
            // Buffered first so it appears under the particle.
            Vec3 shadowWorldPos = p.position;
            shadowWorldPos.y = getLandscapeAltitude(p.position.x, worldZ);
            ProjectedVertex shadowProj = projectVertex(camera.worldToCamera(shadowWorldPos));
            if (shadowProj.visible && shadowProj.onScreen)
            {
                bufferRect(row, shadowProj.screenX, shadowProj.screenY, size, Color::black(), true);
            }

            ProjectedVertex proj = projectVertex(cameraRelPos);
            if (proj.visible && proj.onScreen)
            {
                Color color = p.hasFading() ? fadingColor(p.lifespan)
                                            : vidc256ToColor(p.getColorIndex());
                bufferRect(row, proj.screenX, proj.screenY, size, color, false);
            }
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
// Random Numbers
// =============================================================================

namespace
{
    // Linear congruential generator shared by all particle effects
    uint32_t randomSeed = 0x12345678;

    int32_t particleRandom()
    {
        randomSeed = randomSeed * 1103515245 + 12345;
        return static_cast<int32_t>(randomSeed);
    }

    // Random velocity with each signed component scaled down by `shift` bits.
    // Components are generated in x, y, z order to keep the random sequence stable.
    Vec3 randomVelocity(int shift)
    {
        Vec3 v;
        v.x = Fixed::fromRaw(particleRandom() >> shift);
        v.y = Fixed::fromRaw(particleRandom() >> shift);
        v.z = Fixed::fromRaw(particleRandom() >> shift);
        return v;
    }

    // Random spray velocity for splashes and sparks. Horizontal components keep
    // 1/16 of the impact velocity; `vertical` maps a random value to Y velocity.
    template <typename VerticalFn>
    Vec3 sprayVelocity(const Vec3 &impactVel, VerticalFn vertical)
    {
        auto horizontal = [](Fixed impact) {
            return Fixed::fromRaw((impact.raw >> 4) + (particleRandom() >> 13) - 0x040000);
        };

        Vec3 v;
        v.x = horizontal(impactVel.x);
        v.y = Fixed::fromRaw(vertical(particleRandom() >> 12));
        v.z = horizontal(impactVel.z);
        return v;
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

namespace
{
    // Add a single exhaust particle with randomization
    void addExhaustParticle(const Vec3 &basePos, const Vec3 &baseVel,
                            int32_t baseLifespan, uint32_t flags)
    {
        // Random velocity variation of +/- VEL_RANDOM_RANGE / 2 (~0.03 tiles/frame)
        constexpr int32_t VEL_RANDOM_RANGE = 0x80000;
        auto jitter = [] {
            return Fixed::fromRaw(((particleRandom() >> 8) % VEL_RANDOM_RANGE) - VEL_RANDOM_RANGE / 2);
        };

        Vec3 particleVel = baseVel;
        particleVel.x += jitter();
        particleVel.y += jitter();
        particleVel.z += jitter();

        // Random variation to lifespan (0 to 7 extra frames)
        int32_t lifespan = baseLifespan + ((particleRandom() >> 24) & 0x07);

        particleSystem.addParticle(basePos, particleVel, lifespan, flags);
    }
}

void spawnExhaustParticles(const Vec3 &pos, const Vec3 &vel, const Vec3 &exhaust, bool fullThrust)
{
    // Particles move with the ship plus 1/8 of the exhaust direction
    constexpr int EXHAUST_SPEED_SHIFT = 3;
    Vec3 particleVel = vel + (exhaust >> EXHAUST_SPEED_SHIFT);

    // Spawn offset along the exhaust direction so particles don't appear inside
    // the ship. The original uses exhaust >> 7; a larger offset (>> 1) is needed
    // when the ship's top clearly faces the camera (exhaust.z > 0.25).
    constexpr int32_t EXHAUST_Z_THRESHOLD = 0x00400000;
    int offsetShift = (exhaust.z.raw > EXHAUST_Z_THRESHOLD) ? 1 : 2;

    Vec3 particlePos = pos + (exhaust >> offsetShift);
    particlePos.z += VISUAL_Z_OFFSET;

    // Exhaust particles fade, splash in water and bounce off terrain
    uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                     ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;

    // Short exhaust trails, tuned for visual appeal
    constexpr int32_t BASE_LIFESPAN = 16;

    // Reduced to 1/4 of the original rate (Task 39 tuning)
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
// We spawn at the caller's muzzle position and tune the velocity (gunDir >> 4)
// for gameplay feel; the lifespan scales to 160 frames at 120fps.
//
// =============================================================================

void spawnBulletParticle(const Vec3 &pos, const Vec3 &vel, const Vec3 &gunDir)
{
    Vec3 bulletVel = vel + (gunDir >> 4);

    Vec3 bulletPos = pos;
    bulletPos.z += VISUAL_Z_OFFSET;

    // Bullets are white, and splash, bounce, fall, destroy objects and spark
    uint32_t flags = ParticleFlags::SPLASH | ParticleFlags::BOUNCES |
                     ParticleFlags::GRAVITY | ParticleFlags::DESTROYS_OBJECTS |
                     ParticleFlags::BIG_SPLASH | ParticleFlags::EXPLODES_ON_GROUND | 0xFF;

    constexpr int32_t BULLET_LIFESPAN = 160;

    particleSystem.addParticle(bulletPos, bulletVel, BULLET_LIFESPAN, flags);
}

// =============================================================================
// Splash Particle Spawning Implementation
// =============================================================================
//
// Port of SplashParticleIntoSea and AddSprayParticleToBuffer from Lander.arm.
//
// When something hits water, spawn 1-4 light blue spray particles that
// inherit some of the impact's horizontal momentum, shoot upward and fall back.
//
// =============================================================================

void spawnSplashParticles(const Vec3 &pos, const Vec3 &impactVel)
{
    int count = 1 + ((particleRandom() >> 30) & 0x03);

    for (int i = 0; i < count; i++)
    {
        // Always upward
        Vec3 vel = sprayVelocity(impactVel, [](int32_t r) { return -r - 0x080000; });

        // Light blue (R=51, G=187, B=255), gravity only: spray just falls and fades
        constexpr uint8_t SPRAY_COLOR = 0xCB;
        uint32_t flags = ParticleFlags::GRAVITY | SPRAY_COLOR;

        int32_t lifespan = 16 + ((particleRandom() >> 26) & 0x1F);

        particleSystem.addParticle(pos, vel, lifespan, flags);
    }
}

// =============================================================================
// Spark Particle Spawning Implementation
// =============================================================================
//
// Port of AddSmallExplosionToBuffer from Lander.arm.
//
// When a bullet hits terrain (not water), spawn fading sparks that bounce back
// up from the impact point.
//
// =============================================================================

void spawnSparkParticles(const Vec3 &pos, const Vec3 &impactVel)
{
    constexpr int SPARK_COUNT = 8;

    for (int i = 0; i < SPARK_COUNT; i++)
    {
        // Mostly upward, bouncing back from the impact
        Vec3 vel = sprayVelocity(impactVel, [](int32_t r) { return r - 0x0C0000; });

        // White base colour; FADING takes it through yellow and orange to red
        uint32_t flags = ParticleFlags::FADING | ParticleFlags::GRAVITY |
                         ParticleFlags::BOUNCES | 0xFF;

        int32_t lifespan = 12 + ((particleRandom() >> 28) & 0x0F);

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
// Velocities use the original shifts plus FPS_SHIFT, since the original ran at
// 15fps and we simulate at 120fps (8x slower per frame).
//
// =============================================================================

namespace
{
    constexpr int FPS_SHIFT = 3;

    // Random debris colour (purple-brown-green): R=4-11, G=2-9, B=4-7
    uint8_t generateDebrisColor()
    {
        uint32_t rand1 = particleRandom();
        uint32_t rand2 = particleRandom();

        int red = 4 + (rand1 & 0x07);
        int green = 2 + ((rand1 >> 8) & 0x07);
        int blue = 4 + ((rand2 >> 16) & 0x03);

        return buildVidcColor(red, green, blue);
    }

    // Random smoke colour: grey with all channels 3-10
    uint8_t generateSmokeColor()
    {
        int intensity = 3 + (particleRandom() & 0x07);
        return buildVidcColor(intensity, intensity, intensity);
    }
}

void spawnExplosionParticles(const Vec3 &pos, int clusterCount)
{
    Vec3 explosionPos = pos;
    explosionPos.z += VISUAL_Z_OFFSET;

    auto addSpark = [&explosionPos] {
        Vec3 vel = randomVelocity(8 + FPS_SHIFT);
        uint32_t flags = ParticleFlags::FADING | ParticleFlags::SPLASH |
                         ParticleFlags::BOUNCES | ParticleFlags::GRAVITY;
        int32_t lifespan = 64 + ((particleRandom() >> 26) & 0x3F);
        particleSystem.addParticle(explosionPos, vel, lifespan, flags);
    };

    for (int cluster = 0; cluster < clusterCount; cluster++)
    {
        addSpark();

        // Debris: bounces and falls
        {
            Vec3 vel = randomVelocity(10 + FPS_SHIFT);
            uint32_t flags = ParticleFlags::SPLASH | ParticleFlags::BOUNCES |
                             ParticleFlags::GRAVITY | generateDebrisColor();
            int32_t lifespan = 120 + ((particleRandom() >> 24) & 0xFF);
            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }

        // Smoke: no gravity, rises slowly (original SMOKE_RISING_SPEED, slowed down)
        {
            constexpr int32_t SMOKE_RISING_SPEED = -0x8000;
            Vec3 vel = randomVelocity(13 + FPS_SHIFT);
            vel.y += Fixed::fromRaw(SMOKE_RISING_SPEED);
            uint32_t flags = ParticleFlags::BOUNCES | generateSmokeColor();
            int32_t lifespan = 120 + ((particleRandom() >> 23) & 0x1FF);
            particleSystem.addParticle(explosionPos, vel, lifespan, flags);
        }

        addSpark();
    }
}

// =============================================================================
// Smoke Particle Spawning (for destroyed objects)
// =============================================================================
//
// Port of AddSmokeParticleToBuffer from Lander.arm (lines 3885-3977).
//
// Grey smoke rises slowly from destroyed objects with a little random drift
// and bounces off terrain.
//
// =============================================================================

void spawnSmokeParticle(const Vec3 &pos)
{
    Vec3 smokePos = pos;
    smokePos.z += VISUAL_Z_OFFSET;

    // Original SMOKE_RISING_SPEED is 0x80000 per frame at 15fps; 1/8 at 120fps
    constexpr int32_t SMOKE_RISING_SPEED = -0x10000;
    Vec3 vel = randomVelocity(13 + FPS_SHIFT);
    vel.y += Fixed::fromRaw(SMOKE_RISING_SPEED);

    uint32_t flags = ParticleFlags::BOUNCES | generateSmokeColor();
    int32_t lifespan = 120 + ((particleRandom() >> 22) & 0xFF);

    particleSystem.addParticle(smokePos, vel, lifespan, flags);
}

void spawnSmokeFromDestroyedObjects(const Camera& camera, uint32_t tick)
{
    // The original smokes every 4 frames at 15fps (3.75 per second). This
    // pattern fires twice every 128 steps (~1.9 per second) for a subtler effect.
    if ((tick & 0x5F) != 0)
    {
        return;
    }

    // The same tiles that bufferObjects() draws objects on, back to front
    int halfTilesX = TILES_X / 2;
    int extraTiles = ClippingConfig::enabled ? 1 : 0;
    int camTileX = camera.getXTile().toInt();
    int camTileZ = camera.getZTile().toInt();

    for (int row = 0; row < TILES_Z; row++)
    {
        int worldZ = camTileZ + (TILES_Z - 1 - row);

        for (int col = 1 - extraTiles; col < TILES_X + extraTiles; col++)
        {
            int worldX = camTileX - halfTilesX + col;

            // The object map wraps every 256 tiles
            uint8_t objectType = objectMap.getObjectAt(static_cast<uint8_t>(worldX),
                                                       static_cast<uint8_t>(worldZ));
            if (objectType == ObjectType::NONE || !ObjectMap::isDestroyedType(objectType))
            {
                continue;
            }

            // Smoke rises from SMOKE_HEIGHT (3/4 tile) above the object's base
            Vec3 smokePos;
            smokePos.x = Fixed::fromInt(worldX);
            smokePos.z = Fixed::fromInt(worldZ);
            smokePos.y = getLandscapeAltitude(smokePos.x, smokePos.z) - GameConstants::SMOKE_HEIGHT;

            spawnSmokeParticle(smokePos);
        }
    }
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
// Unlike other particles, rocks are stored in world coordinates (no visual
// Z offset); the offset is applied when rendering.
//
// =============================================================================

// Global rotation angle for all rocks (they spin together, as in the original)
static int32_t rockRotationAngle = 0;

void spawnRock(const Vec3& pos)
{
    // Random rock colour (purple-brown-green): R=4-11, G=2-9, B=4-7
    uint32_t rand1 = particleRandom();
    uint32_t rand2 = particleRandom();

    int r = 4 + (rand1 & 7);
    int g = 2 + ((rand1 >> 29) & 7);
    int b = 4 + ((rand2 >> 30) & 3);

    uint32_t flags = ParticleFlags::IS_ROCK |
                     ParticleFlags::SPLASH |
                     ParticleFlags::BOUNCES |
                     ParticleFlags::GRAVITY |
                     ParticleFlags::DESTROYS_OBJECTS |
                     ParticleFlags::BIG_SPLASH |
                     ParticleFlags::EXPLODES_ON_GROUND |
                     buildVidcColor(r, g, b);

    // Starts falling from rest, with a tiny random horizontal drift
    Vec3 vel;
    vel.x = Fixed::fromRaw(particleRandom() >> 16);
    vel.z = Fixed::fromRaw(particleRandom() >> 16);

    // 170 iterations at 15fps: long enough to fall from 32 tiles
    int32_t lifespan = 1360 + ((particleRandom() >> 27) & 0x1F);

    particleSystem.addParticle(pos, vel, lifespan, flags);
}

void bufferRocks(const Camera& camera)
{
    // Rocks rotate at a fixed speed (~45 degrees per second at 120fps)
    rockRotationAngle += 0x02000000;
    Mat3x3 rockRotation = calculateRotationMatrix(rockRotationAngle, rockRotationAngle >> 1);

    int halfTilesX = TILES_X / 2;

    for (int i = 0; i < particleSystem.getParticleCount(); i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (!p.isRock()) {
            continue;
        }

        // Skip rocks outside the visible tile grid
        int tileX = camera.tileOffsetX(p.position.x);
        int row = camera.rowForZ(p.position.z);
        if (tileX < -halfTilesX || tileX > halfTilesX || row < 0 || row >= TILES_Z)
        {
            continue;
        }

        // Render at the visual ship depth (see VISUAL_Z_OFFSET)
        Vec3 cameraRelPos = camera.worldToCamera(p.position);
        cameraRelPos.z += VISUAL_Z_OFFSET;

        // Skip rocks within 1 tile of the camera (projection would overflow)
        if (cameraRelPos.z <= GameConstants::TILE_SIZE) {
            continue;
        }

        bufferObjectShadow(rockBlueprint, cameraRelPos, rockRotation,
                           p.position, camera.getPosition(), row);
        bufferObject(rockBlueprint, cameraRelPos, rockRotation, row);
    }
}

// =============================================================================
// Rock-Player Collision Detection
// =============================================================================
//
// Port of rock collision from MoveAndDrawParticles Part 3 (Lander.arm lines 2955-3014).
//
// A rock hits the player when it is within 1 tile on every axis. Differences
// wrap with the world, so this also works across the 256-tile seam.
//
// =============================================================================

bool checkRockPlayerCollision(const Vec3& playerPos)
{
    auto withinTile = [](Fixed a, Fixed b) {
        return (a - b).abs() < GameConstants::TILE_SIZE;
    };

    for (int i = 0; i < particleSystem.getParticleCount(); i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (p.isRock() &&
            withinTile(p.position.x, playerPos.x) &&
            withinTile(p.position.z, playerPos.z) &&
            withinTile(p.position.y, playerPos.y))
        {
            return true;
        }
    }

    return false;
}

// =============================================================================
// Star Particle System
// =============================================================================
//
// Stars provide visual feedback at high altitude. They spawn in a cube around
// the player and fade in/out smoothly.
//
// =============================================================================

// Separate generator so stars don't perturb the effects' random sequence
static uint32_t starRandom()
{
    static uint32_t seed = 12345;
    seed = seed * 1103515245 + 12345;
    return seed;
}

// Spawn a single star at a random position in the cube around the player
// The cube center is offset in the direction of player velocity
static void spawnStar(const Vec3& playerPos, const Vec3& playerVel)
{
    // Random position within the spawn cube centred on the player
    int32_t offsetX = (starRandom() % (StarConfig::SPAWN_RADIUS * 2)) - StarConfig::SPAWN_RADIUS;
    int32_t offsetY = (starRandom() % (StarConfig::SPAWN_RADIUS * 2)) - StarConfig::SPAWN_RADIUS;
    int32_t offsetZ = (starRandom() % (StarConfig::SPAWN_RADIUS * 2)) - StarConfig::SPAWN_RADIUS;

    // Shift the cube ahead of the player's movement (~1.5 tiles at high speed)
    constexpr int32_t VEL_SCALE = 32;
    Vec3 pos;
    pos.x = playerPos.x + Fixed::fromRaw(offsetX + (playerVel.x.raw >> 16) * VEL_SCALE);
    pos.y = playerPos.y + Fixed::fromRaw(offsetY + (playerVel.y.raw >> 16) * VEL_SCALE);
    pos.z = playerPos.z + Fixed::fromRaw(offsetZ + (playerVel.z.raw >> 16) * VEL_SCALE);

    int32_t lifespanRange = StarConfig::LIFETIME_MAX_FRAMES - StarConfig::LIFETIME_MIN_FRAMES;
    int32_t lifespan = StarConfig::LIFETIME_MIN_FRAMES + (starRandom() % lifespanRange);

    int brightnessRange = StarConfig::MAX_BRIGHTNESS - StarConfig::MIN_BRIGHTNESS;
    uint8_t brightness = StarConfig::MIN_BRIGHTNESS + (starRandom() % brightnessRange);

    uint8_t size = StarConfig::MIN_SIZE + (starRandom() % (StarConfig::MAX_SIZE - StarConfig::MIN_SIZE + 1));

    // Stars don't move
    if (Particle* p = particleSystem.addParticle(pos, Vec3(), lifespan, ParticleFlags::IS_STAR))
    {
        p->initialLifespan = lifespan;
        p->starSize = size;
        p->starBrightness = brightness;
    }
}

void updateStars(const Vec3& playerPos, const Vec3& playerVel, int32_t playerAltitude)
{
    // No stars below MIN_ALTITUDE, linear ramp to MAX_STARS at MAX_ALTITUDE
    int targetStars = 0;

    if (playerAltitude >= StarConfig::MAX_ALTITUDE)
    {
        targetStars = StarConfig::MAX_STARS;
    }
    else if (playerAltitude >= StarConfig::MIN_ALTITUDE)
    {
        int64_t range = StarConfig::MAX_ALTITUDE - StarConfig::MIN_ALTITUDE;
        int64_t above = playerAltitude - StarConfig::MIN_ALTITUDE;
        targetStars = static_cast<int>(above * StarConfig::MAX_STARS / range);
    }

    // Spawn at most 5 per frame towards the target
    int spawnCount = std::clamp(targetStars - getStarCount(), 0, 5);

    for (int i = 0; i < spawnCount; i++)
    {
        spawnStar(playerPos, playerVel);
    }
}

int getStarCount()
{
    int count = 0;
    for (int i = 0; i < particleSystem.getParticleCount(); i++)
    {
        if (particleSystem.getParticle(i).isStar())
        {
            count++;
        }
    }
    return count;
}

void bufferStars(const Camera& camera)
{
    for (int i = 0; i < particleSystem.getParticleCount(); i++)
    {
        const Particle& p = particleSystem.getParticle(i);

        if (!p.isStar())
        {
            continue;
        }

        Vec3 camSpace = camera.worldToCamera(p.position);
        if (camSpace.z.raw <= 0)
        {
            continue;
        }

        ProjectedVertex proj = projectVertex(camSpace);
        if (!proj.visible || !proj.onScreen)
        {
            continue;
        }

        // Depth sort like other particles, clamping stars beyond the landscape
        // to the back or front row
        int row = std::clamp(camera.rowForZ(p.position.z - VISUAL_Z_OFFSET), 0, TILES_Z - 1);

        // Fade in at start, fade out at end
        int alpha = 255;
        int32_t age = p.initialLifespan - p.lifespan;

        if (age < StarConfig::FADE_IN_FRAMES)
        {
            alpha = (age * 255) / StarConfig::FADE_IN_FRAMES;
        }
        else if (p.lifespan < StarConfig::FADE_OUT_FRAMES)
        {
            alpha = (p.lifespan * 255) / StarConfig::FADE_OUT_FRAMES;
        }

        // Apply alpha to brightness (pre-multiply)
        uint8_t grey = static_cast<uint8_t>((p.starBrightness * alpha) / 255);

        // Size scales with display scale (1-3 base pixels -> 4-12 pixels at scale 4)
        int size = p.starSize * DisplayConfig::scale;

        bufferRect(row, proj.screenX, proj.screenY, {size, size}, Color(grey, grey, grey), false);
    }
}
