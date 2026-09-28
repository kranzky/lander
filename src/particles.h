#ifndef LANDER_PARTICLES_H
#define LANDER_PARTICLES_H

#include "fixed.h"
#include "math3d.h"
#include "palette.h"
#include "constants.h"
#include <cstdint>

// =============================================================================
// Particle System
// =============================================================================
//
// Port of the particle system from Lander.arm (MoveAndDrawParticles).
// The original uses 8 words per particle:
//   - (R0, R1, R2) = particle coordinate (x, y, z)
//   - (R3, R4, R5) = particle velocity (vx, vy, vz)
//   - R6 = particle lifespan counter
//   - R7 = particle flags
//
// Particle flags (from original):
//   - Bit 0-7:   Color index
//   - Bit 16:    Apply color fading (white → red over time)
//   - Bit 17:    Is a rock (special 3D object rendering)
//   - Bit 20:    Apply gravity
//   - Bit 21:    Can destroy objects on collision
//
// =============================================================================

namespace ParticleFlags {
    constexpr uint32_t COLOR_MASK = 0x000000FF;       // Bits 0-7: color index
    constexpr uint32_t FADING = 0x00010000;           // Bit 16: fade from white to red
    constexpr uint32_t IS_ROCK = 0x00020000;          // Bit 17: particle is a rock
    constexpr uint32_t SPLASH = 0x00040000;           // Bit 18: splash on impact with sea
    constexpr uint32_t BOUNCES = 0x00080000;          // Bit 19: bounce off terrain (vs delete)
    constexpr uint32_t GRAVITY = 0x00100000;          // Bit 20: apply gravity
    constexpr uint32_t DESTROYS_OBJECTS = 0x00200000; // Bit 21: can destroy objects
    constexpr uint32_t BIG_SPLASH = 0x00800000;       // Bit 23: big splash (65 vs 4 particles)
    constexpr uint32_t EXPLODES_ON_GROUND = 0x01000000; // Bit 24: explode on terrain (sparks)
    constexpr uint32_t IS_STAR = 0x02000000;          // Bit 25: particle is a star
}

namespace ParticleConstants {
    // Room for a full star field (StarConfig::MAX_STARS) plus a ship crash
    // (200), several object/rock explosions (80 each), exhaust, bullets and
    // smoke. addParticle drops new particles when the pool is full.
    constexpr int MAX_PARTICLES = 1500;

    // Bounce damping (velocity multiplier after bounce, approximate)
    constexpr int BOUNCE_DAMPING_SHIFT = 1;  // Divide by 2 on bounce

    // The ship is drawn 15 tiles in front of the camera for a consistent size,
    // but the player is really only 5 tiles in front. Most particles are stored
    // 10 tiles further out than their world position so they line up with the
    // ship on screen; subtract this to get back to world coordinates. Rocks are
    // the exception: they are stored in world coordinates.
    constexpr Fixed VISUAL_Z_OFFSET = GameConstants::LANDSCAPE_Z_FRONT;
}

// =============================================================================
// Star Particle Configuration
// =============================================================================

namespace StarConfig {
    constexpr int MAX_STARS = 400;              // Maximum active stars
    constexpr int32_t MIN_ALTITUDE = 10 << 24;  // 10 tiles in 8.24 fixed point
    constexpr int32_t MAX_ALTITUDE = 52 << 24;  // 52 tiles (engine cutout)
    constexpr int32_t SPAWN_RADIUS = 8 << 24;   // 8 tiles half-cube size (16x16x16 cube)
    constexpr int LIFETIME_MIN_FRAMES = 150;    // ~1.25 seconds at 120fps
    constexpr int LIFETIME_MAX_FRAMES = 210;    // ~1.75 seconds at 120fps
    constexpr int FADE_IN_FRAMES = 8;           // ~0.06 seconds at 120fps
    constexpr int FADE_OUT_FRAMES = 22;         // ~0.18 seconds at 120fps
    constexpr int MIN_BRIGHTNESS = 160;         // Minimum grey value
    constexpr int MAX_BRIGHTNESS = 255;         // Maximum grey value
    constexpr int MIN_SIZE = 1;                 // Minimum size in base pixels
    constexpr int MAX_SIZE = 3;                 // Maximum size in base pixels
}

// Single particle data structure
struct Particle {
    Vec3 position;      // World position (x, y, z)
    Vec3 velocity;      // Velocity per frame
    int32_t lifespan;   // Frames remaining (0 = expired)
    uint32_t flags;     // ParticleFlags

    // Star-specific fields (only used when IS_STAR flag is set)
    int32_t initialLifespan;  // Starting lifespan (for fade calculation)
    uint8_t starSize;         // Size in base pixels (1-3)
    uint8_t starBrightness;   // Grey value (160-255)

    // Get color index from flags
    uint8_t getColorIndex() const { return flags & ParticleFlags::COLOR_MASK; }

    // Check flag helpers
    bool hasGravity() const { return (flags & ParticleFlags::GRAVITY) != 0; }
    bool hasFading() const { return (flags & ParticleFlags::FADING) != 0; }
    bool isRock() const { return (flags & ParticleFlags::IS_ROCK) != 0; }
    bool canDestroyObjects() const { return (flags & ParticleFlags::DESTROYS_OBJECTS) != 0; }
    bool splashesInSea() const { return (flags & ParticleFlags::SPLASH) != 0; }
    bool bouncesOffTerrain() const { return (flags & ParticleFlags::BOUNCES) != 0; }
    bool hasBigSplash() const { return (flags & ParticleFlags::BIG_SPLASH) != 0; }
    bool explodesOnGround() const { return (flags & ParticleFlags::EXPLODES_ON_GROUND) != 0; }
    bool isStar() const { return (flags & ParticleFlags::IS_STAR) != 0; }
};

// Particle system manager
class ParticleSystem {
public:
    ParticleSystem();

    // Clear all particles
    void clear();

    // Add a new particle, returning it, or nullptr if the pool is full
    Particle* addParticle(const Vec3& pos, const Vec3& vel, int32_t lifespan, uint32_t flags);

    // Number of free slots in the pool
    int getFreeCount() const { return ParticleConstants::MAX_PARTICLES - particleCount; }

    // Update all particles (apply velocity, gravity, lifespan countdown)
    // Call once per physics step
    void update();

    // Set gravity per physics step, shared with the ship (see Gravity in
    // constants.h)
    void setGravity(int32_t perStep) { gravity = perStep; }

    // Access particles for rendering
    int getParticleCount() const { return particleCount; }
    const Particle& getParticle(int index) const { return particles[index]; }

    // Get modifiable particle (for terrain collision updates)
    Particle& getParticle(int index) { return particles[index]; }

private:
    Particle particles[ParticleConstants::MAX_PARTICLES];
    int particleCount;  // Number of active particles
    int32_t gravity = Gravity::perStep(Gravity::INITIAL);

    // Remove particle at index by moving last particle into its place
    void removeParticle(int index);
};

// Global particle system instance
extern ParticleSystem particleSystem;

// =============================================================================
// Particle Events (for sound triggers)
// =============================================================================
//
// Events are accumulated during update() and should be polled each frame
// after calling update(). Counts reset on read.
//
// =============================================================================

struct ParticleEvents {
    int objectDestroyed;      // Object destroyed by bullet (boom sound)
    int bulletHitGround;      // Bullet hit terrain (shoot_impact sound)
    int bulletHitWater;       // Bullet hit water (splash sound)
    int exhaustHitWater;      // Exhaust particle hit water (water sound)
    int rockExploded;         // Rock hit ground/water (boom sound)

    // Positions of most recent events (for spatial audio)
    Vec3 objectDestroyedPos;
    Vec3 bulletHitGroundPos;
    Vec3 bulletHitWaterPos;
    Vec3 exhaustHitWaterPos;
    Vec3 rockExplodedPos;

    void reset() {
        objectDestroyed = 0;
        bulletHitGround = 0;
        bulletHitWater = 0;
        exhaustHitWater = 0;
        rockExploded = 0;
    }
};

// Get events from last update() call
ParticleEvents& getParticleEvents();

// =============================================================================
// Particle Rendering
// =============================================================================
//
// Port of particle rendering from Lander.arm:
// - DrawParticleToBuffer (lines 8435-8503): 3x2 colored particle
// - DrawParticleShadowToBuffer (lines 8555-8620): 3x1 black shadow
// - ProjectParticleOntoScreen: standard perspective projection
//
// Particle sizes in original (at 320x256):
// - Large particles: 3x2 pixels (commands 0-8)
// - Small particles/shadows: 3x1 pixels (commands 9-17)
//
// We scale to 4x for 1280x1024 resolution:
// - Large particles: 12x8 pixels
// - Small particles/shadows: 12x4 pixels
//
// =============================================================================

class Camera;

// Buffer particles to the graphics buffer system for depth-sorted rendering
// Particles are split into two passes for correct depth sorting with the ship:
// - bufferParticlesBehind: particles further from camera than shipDepthZ (drawn before ship)
// - bufferParticlesInFront: particles closer to camera than shipDepthZ (drawn after ship)
// shipDepthZ is the ship's camera-relative Z (typically 15 tiles)
void bufferParticlesBehind(const Camera& camera, Fixed shipDepthZ);
void bufferParticlesInFront(const Camera& camera, Fixed shipDepthZ);

// =============================================================================
// Exhaust Particle Spawning
// =============================================================================
//
// Port of exhaust plume spawning from MoveAndDrawPlayer Part 4 (lines 2224-2365)
//
// When thrusting:
// - Spawns 8 particles for full thrust, 2 for hover
// - Particles spawn behind ship in direction of exhaust vector
// - Velocity combines ship velocity + exhaust direction
// - Random variation added to velocity and lifespan
// - Flags: FADING | GRAVITY (white fading to red, affected by gravity)
//
// =============================================================================

// Spawn exhaust particles behind the ship
// pos: ship position
// vel: ship velocity
// exhaust: exhaust direction vector (points away from ship bottom)
// fullThrust: true = 8 particles (left button), false = 2 particles (middle button)
void spawnExhaustParticles(const Vec3& pos, const Vec3& vel, const Vec3& exhaust, bool fullThrust);

// =============================================================================
// Bullet Particle Spawning
// =============================================================================
//
// Port of bullet firing from MoveAndDrawPlayer Part 5 (lines 2369-2465)
//
// When right button pressed:
// - Spawns 1 bullet particle per frame
// - Velocity = playerVelocity + gunDirection/256
// - Position = playerPos - bulletVel + gunDirection/128
// - Flags: GRAVITY | DESTROYS_OBJECTS | white color
// - Lifespan: 20 frames at 15fps (~160 frames at 120fps)
//
// =============================================================================

// Spawn a bullet particle in the direction the ship is facing
// pos: ship position
// vel: ship velocity
// gunDir: gun direction vector (nose of ship, points forward)
void spawnBulletParticle(const Vec3& pos, const Vec3& vel, const Vec3& gunDir);

// =============================================================================
// Impact Effect Spawning
// =============================================================================
//
// Port of SplashParticleIntoSea and AddSmallExplosionToBuffer from Lander.arm.
//
// When particles hit the sea:
// - Create 1-4 blue spray particles shooting upward (the original's 4 or 65
//   particles were too dense at 120fps)
//
// When bullets hit terrain (not sea):
// - Create spark particles in random directions
// - Yellow/orange color (explosion-like)
//
// =============================================================================

// Spawn splash spray particles when something hits the sea
// pos: impact position (at sea level)
// impactVel: velocity of the impacting particle (used as bias for splash)
void spawnSplashParticles(const Vec3& pos, const Vec3& impactVel);

// Spawn spark particles when a bullet hits terrain
// pos: impact position (at terrain surface)
// impactVel: velocity of the impacting particle (used as bias for sparks)
void spawnSparkParticles(const Vec3& pos, const Vec3& impactVel);

// =============================================================================
// Explosion Particle Spawning
// =============================================================================
//
// Port of AddExplosionToBuffer from Lander.arm (lines 4406-4438).
//
// An explosion consists of clusters of 4 particles each:
// - 2x Spark particles (white fading to red, with FADING flag)
// - 1x Debris particle (purple-brown-green, bounces)
// - 1x Smoke particle (grey, rises slowly)
//
// Cluster count determines explosion size:
// - Small (3): 12 particles - bullet/object impacts
// - Medium (20): 80 particles - destroyed objects
// - Large (50): 200 particles - ship crash
//
// =============================================================================

// Spawn explosion particles (spark, debris, smoke clusters)
// pos: explosion center position
// clusterCount: number of 4-particle clusters (3=small, 20=medium, 50=large)
void spawnExplosionParticles(const Vec3& pos, int clusterCount);

// =============================================================================
// Smoke Particle Spawning (for destroyed objects)
// =============================================================================
//
// Port of AddSmokeParticleToBuffer from Lander.arm (lines 3885-3977).
//
// Smoke particles are spawned from destroyed objects every few frames:
// - Grey color (random intensity 3-10)
// - Rises slowly upward (SMOKE_RISING_SPEED)
// - Random velocity variation
// - Bounces off terrain
// - Short lifespan with random variation
//
// =============================================================================

// Spawn a single smoke particle rising from a destroyed object
// pos: position to spawn smoke (should be SMOKE_HEIGHT above object base)
void spawnSmokeParticle(const Vec3& pos);

// Spawn smoke from destroyed objects in view (port of DrawObjects Part 3,
// Lander.arm lines 4910-4947). Call once per physics step, so the smoke rate
// doesn't depend on the frame rate; tick is the physics step count.
void spawnSmokeFromDestroyedObjects(const Camera& camera, uint32_t tick);

// =============================================================================
// Falling Rock Spawning
// =============================================================================
//
// Port of DropARockFromTheSky from Lander.arm (lines 4120-4224).
//
// Rocks are spawned as particles with IS_ROCK flag:
// - Rendered as 3D rotating objects (not as sprites)
// - Dropped from 32 tiles up, one tile in front of the ship
// - Start almost at rest, with a small random velocity in every direction
// - Have gravity, bounce, splash, and can destroy objects
// - Kill the player on collision
//
// =============================================================================

// Port of DropRocksFromTheSky (Lander.arm lines 4578-4625), called once per
// original frame. Over a score of 800, drops a rock if a random number from 0
// to 16383 is below (score - 800), so rocks get more frequent as the score
// rises. Returns true if a rock was dropped.
bool dropRocksFromTheSky(const Vec3& playerPos, int score);

// Spawn a falling rock at the given position
void spawnRock(const Vec3& pos);

// Buffer rocks into graphics buffer system for depth-sorted rendering
void bufferRocks(const Camera& camera);

// Check for rock-player collision (both in world coordinates)
// Returns true if a rock hit the player
bool checkRockPlayerCollision(const Vec3& playerPos);

// =============================================================================
// Star Particle System
// =============================================================================
//
// Stars provide visual feedback for speed and direction at high altitude.
// They spawn in a cube around the player and fade in/out smoothly.
//
// =============================================================================

// Update star spawning based on player position and altitude
// Called once per frame when stars are enabled
// playerPos: player's world position
// playerVel: player's velocity (used to offset spawn cube ahead of movement)
// playerAltitude: height above terrain (for density calculation)
void updateStars(const Vec3& playerPos, const Vec3& playerVel, int32_t playerAltitude);

// Get current star count
int getStarCount();

// Buffer stars for depth-sorted rendering
// Stars are rendered as filled squares with fade effect
void bufferStars(const Camera& camera);

#endif // LANDER_PARTICLES_H
