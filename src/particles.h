#ifndef LANDER_PARTICLES_H
#define LANDER_PARTICLES_H

#include "fixed.h"
#include "math3d.h"
#include "palette.h"
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
}

namespace ParticleConstants {
    constexpr int MAX_PARTICLES = 484;  // From original Lander

    // Gravity for particles (same as player gravity but scaled differently in original)
    // Original uses the same gravity constant for particles
    constexpr int32_t PARTICLE_GRAVITY = 0x6000;  // Same as player gravity at 120fps

    // Bounce damping (velocity multiplier after bounce, approximate)
    constexpr int BOUNCE_DAMPING_SHIFT = 1;  // Divide by 2 on bounce
}

// Single particle data structure
struct Particle {
    Vec3 position;      // World position (x, y, z)
    Vec3 velocity;      // Velocity per frame
    int32_t lifespan;   // Frames remaining (0 = expired)
    uint32_t flags;     // ParticleFlags

    // Check if particle is active
    bool isActive() const { return lifespan > 0; }

    // Get color index from flags
    uint8_t getColorIndex() const { return flags & ParticleFlags::COLOR_MASK; }

    // Set color index in flags
    void setColorIndex(uint8_t color) {
        flags = (flags & ~ParticleFlags::COLOR_MASK) | color;
    }

    // Check flag helpers
    bool hasGravity() const { return (flags & ParticleFlags::GRAVITY) != 0; }
    bool hasFading() const { return (flags & ParticleFlags::FADING) != 0; }
    bool isRock() const { return (flags & ParticleFlags::IS_ROCK) != 0; }
    bool canDestroyObjects() const { return (flags & ParticleFlags::DESTROYS_OBJECTS) != 0; }
    bool splashesInSea() const { return (flags & ParticleFlags::SPLASH) != 0; }
    bool bouncesOffTerrain() const { return (flags & ParticleFlags::BOUNCES) != 0; }
    bool hasBigSplash() const { return (flags & ParticleFlags::BIG_SPLASH) != 0; }
    bool explodesOnGround() const { return (flags & ParticleFlags::EXPLODES_ON_GROUND) != 0; }
};

// Particle system manager
class ParticleSystem {
public:
    ParticleSystem();

    // Clear all particles
    void clear();

    // Add a new particle, returns true if successful (room available)
    bool addParticle(const Vec3& pos, const Vec3& vel, int32_t lifespan, uint32_t flags);

    // Update all particles (apply velocity, gravity, lifespan countdown)
    // Call once per frame
    void update();

    // Access particles for rendering
    int getParticleCount() const { return particleCount; }
    const Particle& getParticle(int index) const { return particles[index]; }

    // Get modifiable particle (for terrain collision updates)
    Particle& getParticle(int index) { return particles[index]; }

private:
    Particle particles[ParticleConstants::MAX_PARTICLES];
    int particleCount;  // Number of active particles

    // Remove particle at index by moving last particle into its place
    void removeParticle(int index);
};

// Global particle system instance
extern ParticleSystem particleSystem;

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

// Forward declarations
class ScreenBuffer;
class Camera;

// Render all particles
// Requires camera for projection and terrain for shadow placement
void renderParticles(const Camera& camera, ScreenBuffer& screen);

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
// - Create blue spray particles shooting upward
// - Number of particles: 4 for small splash, 65 for big splash
//
// When bullets hit terrain (not sea):
// - Create spark particles in random directions
// - Yellow/orange color (explosion-like)
//
// =============================================================================

// Spawn splash spray particles when something hits the sea
// pos: impact position (at sea level)
// impactVel: velocity of the impacting particle (used as bias for splash)
// bigSplash: true = 65 particles, false = 4 particles
void spawnSplashParticles(const Vec3& pos, const Vec3& impactVel, bool bigSplash);

// Spawn spark particles when a bullet hits terrain
// pos: impact position (at terrain surface)
// impactVel: velocity of the impacting particle (used as bias for sparks)
void spawnSparkParticles(const Vec3& pos, const Vec3& impactVel);

#endif // LANDER_PARTICLES_H
