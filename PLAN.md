# Lander C++/SDL Port Plan

## Project Overview

This document outlines the plan to create a faithful C++/SDL port of **Lander** (Acorn Archimedes, 1987) by David Braben. The port will use the fully documented `Lander.arm` source code as the authoritative reference, preserving the original game logic while targeting modern hardware.

### Goals

1. **Faithfulness**: Match the original game mechanics, physics, and visual appearance
2. **Higher Resolution**: Render at modern resolutions (e.g., 1280x1024) while maintaining the original aesthetic
3. **Full Frame Rate**: Run at 60 FPS (original ran at ~12-15 FPS due to hardware constraints)
4. **Clean Code**: Modern C++17 with clear structure mirroring the original ARM assembly organization
5. **Cross-Platform**: Build and run on macOS, Linux, and Windows via SDL2

### Non-Goals

- Adding new features, gameplay modes, or enhancements
- Rewriting the algorithms (we preserve the original math and logic)
- Modern 3D API (OpenGL/Vulkan) - we use SDL2's software rendering to match the original

---

## Architecture Overview

The port will mirror the original's structure with these C++ modules:

```
lander-cpp/
├── src/
│   ├── main.cpp              # Entry point, game loop
│   ├── game.cpp/h            # Game state, main loop orchestration
│   ├── renderer.cpp/h        # Screen rendering, triangle rasterization
│   ├── landscape.cpp/h       # Terrain generation and tile management
│   ├── player.cpp/h          # Ship state, controls, physics
│   ├── particles.cpp/h       # Particle system (bullets, smoke, sparks)
│   ├── objects.cpp/h         # Static objects (trees, buildings, rockets)
│   ├── rocks.cpp/h           # Falling rock hazards
│   ├── math3d.cpp/h          # Fixed-point math, vectors, matrices
│   ├── lookup_tables.cpp/h   # Sin, arctan, sqrt, division tables
│   ├── input.cpp/h           # Mouse/keyboard input handling
│   ├── graphics_buffer.cpp/h # Depth-sorted drawing buffers
│   └── constants.h           # Game constants from original source
├── assets/                   # (none needed - all procedural)
├── CMakeLists.txt
└── README.md
```

---

## Technical Specifications

### Display

| Property | Original | Port |
|----------|----------|------|
| Resolution | 320x256 | 1280x1024 (4x scale) |
| Color depth | 256 colors | 32-bit RGBA |
| Frame rate | ~12-15 FPS | 60 FPS |
| Aspect ratio | 5:4 | 5:4 (preserved) |
| Rendering | Software | SDL2 software renderer |

The play area in the original was the top 128 lines (320x128). At 4x scale this becomes 1280x512, centered in a 1280x1024 window with the score/fuel UI below.

### Coordinate System

The original uses 32-bit fixed-point coordinates with the following conventions:

- **X**: Left-to-right (0 at left edge of landscape)
- **Y**: Altitude (inverted - larger Y = lower altitude, 0 = highest)
- **Z**: Depth (0 at camera, increasing toward front of landscape)
- **Fixed-point format**: 8.24 (8 integer bits, 24 fractional bits)
- **TILE_SIZE** = 0x01000000 = 1.0 in fixed-point

We will use the same fixed-point representation internally to ensure identical behavior.

### Input Mapping

| Original | Port |
|----------|------|
| Mouse position | Mouse position (scaled to 0-1023 range) |
| Left mouse button | Left mouse button (full thrust) |
| Middle mouse button | Middle mouse button (hover) |
| Right mouse button | Right mouse button (fire bullets) |
| Escape key | Escape key (exit) |

---

## Implementation Tasks

### Phase 1: Foundation

#### Task 1.1: Project Setup
- Create CMakeLists.txt with SDL2 dependency
- Set up basic project structure with empty source files
- Configure build for macOS/Linux/Windows
- Create window and basic event loop

#### Task 1.2: Fixed-Point Math Library
- Implement `Fixed` type (32-bit, 8.24 format)
- Arithmetic operators: +, -, *, /
- Comparison operators
- Conversion to/from int and float
- Bit manipulation helpers (shifts)

#### Task 1.3: Lookup Tables
- Generate sine table (1024 entries, matching original)
- Generate arctangent table (for polar coordinate conversion)
- Generate square root table
- Verify tables match original values

#### Task 1.4: Vector and Matrix Math
- `Vec3` structure using Fixed-point
- 3x3 rotation matrix structure
- Matrix multiplication
- Vector-matrix multiplication (vertex transformation)
- Dot product calculation

### Phase 2: Rendering Foundation

#### Task 2.1: Screen Buffer Setup
- Create SDL surface/texture for rendering
- Implement pixel plotting at scaled resolution
- Color palette setup (256 original colors mapped to RGB)
- Double buffering via SDL

#### Task 2.2: Projection System
- Implement `ProjectVertexOntoScreen` function
- Camera-relative coordinate calculation
- Screen coordinate scaling (original 320x128 to target resolution)
- Off-screen clipping detection

#### Task 2.3: Triangle Rasterization
- Implement `DrawTriangle` using scan-line algorithm
- Edge walking with fixed-point precision
- Horizontal line drawing
- Flat shading with solid colors

#### Task 2.4: Horizontal Line Drawing
- Optimized horizontal span fill
- Screen bounds clipping
- Color application

### Phase 3: Landscape System

#### Task 3.1: Landscape Altitude Generation
- Implement `GetLandscapeAltitude` using Fourier synthesis
- Use sine lookup table for terrain calculation
- Sea level capping
- Launchpad override zone

#### Task 3.2: Landscape Tile Rendering
- Implement tile corner calculation
- Back-to-front rendering order
- Quadrilateral splitting into triangles
- Altitude-based color shading

#### Task 3.3: Camera System
- Camera position tracking
- Tile-aligned camera snapping
- Landscape offset calculation
- View frustum (what tiles are visible)

### Phase 4: Player Ship

#### Task 4.1: Ship State Management
- Position (x, y, z) tracking
- Velocity (vx, vy, vz) tracking
- Rotation state (pitch, yaw)
- Fuel level, lives, score

#### Task 4.2: Input Processing
- Mouse position reading
- Polar coordinate conversion (distance → pitch, angle → yaw)
- Button state detection
- Escape key handling

#### Task 4.3: Ship Physics
- Rotation matrix calculation from mouse input
- Thrust vector extraction from matrix
- Velocity update (friction, thrust, gravity)
- Position integration

#### Task 4.4: Ship Rendering
- Ship 3D model (9 vertices, 9 faces)
- Model transformation by rotation matrix
- Projection and drawing
- Shadow rendering (optional, if original has it)

### Phase 5: Collision Detection

#### Task 5.1: Terrain Collision
- Ship-to-landscape collision detection
- Undercarriage offset calculation
- Crash detection (velocity threshold)
- Landing detection (on launchpad)

#### Task 5.2: Crash Handling
- Life decrement
- Explosion particle spawn
- Ship respawn on launchpad
- Game over detection

#### Task 5.3: Landing Handling
- Safe landing detection
- Score calculation and display
- Fuel bonus
- New game round start

### Phase 6: Particle System

#### Task 6.1: Particle Data Structure
- Particle buffer (max 484 particles)
- Per-particle state: position, velocity, lifespan, flags
- Particle type enumeration

#### Task 6.2: Particle Physics
- Position/velocity integration
- Gravity application (flag-based)
- Terrain bounce
- Lifespan countdown and removal

#### Task 6.3: Particle Rendering
- Sprite particles (single-pixel or small)
- Color fading (white → red with age)
- Screen projection

#### Task 6.4: Particle Spawning
- Exhaust particles from ship thrust
- Bullet particles from firing
- Smoke from destroyed objects
- Explosion sparks
- Water splash effects

### Phase 7: Static Objects

#### Task 7.1: Object Map System
- 256x256 object placement map
- Object type definitions (trees, buildings, rockets)
- Random object placement algorithm

#### Task 7.2: Object 3D Models
- Define object blueprints (vertex/face data)
- Object types: small tree, tall tree, gazebo, building, rocket
- Smoking wreckage variants

#### Task 7.3: Object Rendering
- Back-to-front object traversal
- 3D transformation and projection
- Backface culling
- Drawing to graphics buffers

#### Task 7.4: Object Destruction
- Bullet collision detection
- Object type replacement (to smoking wreckage)
- Score increment
- Smoke particle spawning

### Phase 8: Graphics Buffers (Depth Sorting)

#### Task 8.1: Buffer System
- One buffer per tile row (11 buffers)
- Triangle data storage
- Terminator markers

#### Task 8.2: Buffer Integration
- Objects write to appropriate row buffer
- Buffers drawn after corresponding landscape row
- Correct depth ordering

### Phase 9: Falling Rocks

#### Task 9.1: Rock Spawning
- Score threshold check (≥ 800)
- Random spawn timing
- Initial position and velocity

#### Task 9.2: Rock Physics
- Rock rotation matrix (continuous spin)
- Velocity update with gravity
- Position integration

#### Task 9.3: Rock Rendering
- 3D cube model (6 vertices, 8 faces)
- Rotation transformation
- Drawing as particle type

#### Task 9.4: Rock-Ship Collision
- Distance check to player
- Crash triggering
- Rock removal on collision

### Phase 10: UI and Polish

#### Task 10.1: Score Display
- Current score rendering
- High score tracking
- Bullet count display (ammo)

#### Task 10.2: Fuel Gauge
- Fuel bar rendering
- Fuel consumption tracking
- Low fuel warning

#### Task 10.3: Game State Transitions
- Title/start state
- Playing state
- Game over state
- State transitions

#### Task 10.4: Frame Timing
- 60 FPS frame limiter
- Delta time calculation (for consistent physics)
- Performance monitoring

### Phase 11: Testing and Refinement

#### Task 11.1: Physics Verification
- Compare ship movement to original
- Verify gravity, friction, thrust values
- Test collision boundaries

#### Task 11.2: Visual Comparison
- Screenshot comparison with original
- Color accuracy verification
- Perspective correctness

#### Task 11.3: Gameplay Testing
- Full playthrough testing
- Edge case handling
- Balance verification

---

## Constants Reference

Key constants from `Lander.arm` to preserve exactly:

```cpp
// Landscape
constexpr Fixed TILE_SIZE       = 0x01000000;
constexpr int   TILES_X         = 13;  // 12 tiles + 1 corner
constexpr int   TILES_Z         = 11;  // 10 tiles + 1 corner

// Altitudes (Y is inverted - larger = lower)
constexpr Fixed LAUNCHPAD_ALTITUDE = 0x03500000;
constexpr Fixed SEA_LEVEL          = 0x05500000;
constexpr Fixed HIGHEST_ALTITUDE   = 0x34000000;

// Physics
constexpr Fixed LANDING_SPEED      = 0x00200000;
constexpr Fixed UNDERCARRIAGE_Y    = 0x00640000;
constexpr Fixed SMOKE_RISING_SPEED = 0x00080000;

// Particles
constexpr int   MAX_PARTICLES      = 484;

// Landscape generation
constexpr Fixed LAND_MID_HEIGHT    = TILE_SIZE * 5;
constexpr Fixed ROCK_HEIGHT        = TILE_SIZE * 32;

// Launchpad
constexpr Fixed LAUNCHPAD_SIZE     = TILE_SIZE * 8;
```

---

## Color Palette

The original uses Acorn VIDC 256-color mode. We'll need to:

1. Extract or recreate the original palette
2. Map landscape altitude gradients to appropriate colors
3. Match object colors exactly
4. Support particle color fading (white → orange → red)

Palette ranges (approximate):
- Landscape: Green/brown gradients based on altitude
- Water: Blue tones
- Sky: Black (space)
- Ship: White/gray
- Trees: Green tones
- Buildings: Gray/brown
- Explosions: Yellow/orange/red

---

## Frame Rate Considerations

The original ran at ~12-15 FPS due to CPU limitations. Our port runs at 60 FPS. To maintain faithful behavior:

1. **Physics tick rate**: Run physics at original ~15 Hz internally, interpolate for display
   - OR: Scale all velocity/acceleration values by frame rate ratio
2. **Visual smoothness**: Render at 60 FPS for smooth animation
3. **Input responsiveness**: Sample input at 60 Hz for better feel

Recommended approach: Scale physics constants by (15/60 = 0.25) so the same values produce correct behavior at 60 FPS. This keeps the code close to original.

---

## Risk Areas

1. **Fixed-point precision**: Must match original's 32-bit arithmetic exactly
2. **Lookup table accuracy**: Tables must be identical to original
3. **Projection math**: The 3D→2D conversion is critical for correct visuals
4. **Triangle rasterization**: Must handle edge cases the same as original
5. **Z-ordering**: Graphics buffer system must produce correct depth sorting

---

## Testing Strategy

1. **Unit tests**: Math library, lookup tables
2. **Visual tests**: Screenshot comparison at key moments
3. **Physics tests**: Record ship trajectories, compare to original
4. **Integration tests**: Full gameplay sequences
5. **Performance tests**: Maintain 60 FPS on target hardware

---

## Dependencies

- **SDL2**: Window management, input, software rendering
- **C++17**: Modern language features
- **CMake 3.16+**: Build system

No other external dependencies. All game logic is self-contained.

---

## File-by-File Mapping

| Original (Lander.arm) | Port File | Description |
|-----------------------|-----------|-------------|
| Lines 1-200 | constants.h | Game constants |
| Lines 201-700 | lookup_tables.cpp | Sine, arctan, sqrt tables |
| Lines 724-1228 | landscape.cpp | Landscape rendering |
| Lines 1229-1742 | landscape.cpp | Altitude calculation |
| Lines 1743-2053 | input.cpp, player.cpp | Mouse handling |
| Lines 2054-2763 | player.cpp | Ship physics |
| Lines 2764-4569 | particles.cpp | Particle system |
| Lines 4570-4669 | rocks.cpp | Rock spawning |
| Lines 4670-4978 | objects.cpp | Object rendering |
| Lines 4979-5989 | renderer.cpp | 3D object drawing |
| Lines 5990-6312 | math3d.cpp | Matrix operations |
| Lines 6313-7120 | math3d.cpp | Rotation matrices |
| Lines 7121-7796 | renderer.cpp | Projection |
| Lines 7797-8100 | game.cpp | Random numbers |
| Lines 9219-9279 | renderer.cpp | Quadrilateral |
| Lines 9280-11800 | renderer.cpp | Triangle rasterization |
| Lines 12090-12700 | main.cpp, game.cpp | Entry, game loop |
| Lines 12712-13045 | objects.cpp | Object definitions |

---

## Next Steps

After this plan is approved:

1. Review and refine task breakdown
2. Begin Phase 1 (Foundation)
3. Implement incrementally, testing each phase
4. Regular comparison against original behavior

---

## References

- **Source**: `1-source-files/main-sources/Lander.arm`
- **Documentation**: https://lander.bbcelite.com
- **Original**: Lander (1987) by David Braben for Acorn Archimedes
