# Lander C++/SDL Port - Task Breakdown

This document breaks down the implementation into discrete, reviewable tasks. Each task should result in compilable, testable code that can be committed independently.

---

## Task List

### Task 1: Project Setup and Window
**Goal**: Create the basic project structure with CMake and SDL2, displaying an empty window.

**Deliverables**:
- `CMakeLists.txt` - Build configuration with SDL2
- `src/main.cpp` - Entry point with SDL initialization
- `src/constants.h` - Display constants (resolution, scale factor)
- Empty window at 1280x1024, black background
- Clean exit on Escape key or window close
- Basic frame timing (60 FPS cap)

**Verification**: Window opens, displays black, closes cleanly on Escape.

---

### Task 2: Fixed-Point Math Library
**Goal**: Implement the fixed-point number type matching the original's 32-bit format.

**Deliverables**:
- `src/fixed.h` - Fixed-point type definition and operations
- Arithmetic: `+`, `-`, `*`, `/`
- Comparison: `<`, `>`, `<=`, `>=`, `==`, `!=`
- Conversion: `fromInt()`, `toInt()`, `fromFloat()`, `toFloat()`
- Bit shifts: `operator>>`, `operator<<`
- Constants: `TILE_SIZE`, `LAUNCHPAD_ALTITUDE`, `SEA_LEVEL`, etc.

**Verification**: Unit test or manual verification that operations match expected fixed-point behavior.

---

### Task 3: Lookup Tables
**Goal**: Generate the sine, arctangent, and square root lookup tables matching the original.

**Deliverables**:
- `src/lookup_tables.h` - Table declarations
- `src/lookup_tables.cpp` - Table data and access functions
- `sinTable[1024]` - Full-circle sine values
- `getSin(angle)`, `getCos(angle)` functions
- `arctanTable[1024]` - For polar coordinate conversion
- `getArctan(x, y)` function

**Verification**: Spot-check key values against original source comments.

---

### Task 4: Vector and Matrix Types
**Goal**: Implement 3D vector and 3x3 matrix types for transformations.

**Deliverables**:
- `src/math3d.h` - Vec3 and Mat3x3 structures
- `src/math3d.cpp` - Implementation
- Vec3: add, subtract, dot product, scale
- Mat3x3: multiply vector, multiply matrix
- `calculateRotationMatrix(pitch, yaw)` - From two angles

**Verification**: Rotate a test vector and verify output matches expected.

---

### Task 5: Screen Buffer and Pixel Plotting
**Goal**: Set up the rendering surface and basic pixel operations.

**Deliverables**:
- `src/screen.h` - ScreenBuffer class and Color type
- `src/screen.cpp` - Implementation
- `src/stb_image_write.h` - PNG output library
- SDL texture for rendering (1280x1024 physical)
- Logical coordinate system (320x256) for game logic
- `clear(color)` function
- `plotPixel(x, y, color)` - plot at scaled logical position (single physical pixel)
- `plotPhysicalPixel(px, py, color)` - direct physical pixel access for smooth rendering
- `toPhysicalX/Y()` - coordinate conversion helpers
- `savePNG(filename)` - screenshot output
- `--screenshot <file>` command line flag

**Verification**: Plot test pattern, verify smooth rendering via screenshot.

**Note**: Logical coordinates scale to physical (4x), but plot single pixels for smooth rendering.
Future tasks will add sub-pixel precision (Fixed → physical) for smooth animation.

---

### Task 6: Color Palette
**Goal**: Implement the 256-color palette matching the original Acorn VIDC mode.

**Deliverables**:
- Color palette array (256 entries, RGB values)
- `getColor(index)` function returning RGB
- Landscape color gradient function
- Basic color constants for testing

**Verification**: Display color swatches to verify palette looks correct.

---

### Task 7: Horizontal Line Drawing
**Goal**: Implement optimized horizontal line drawing with clipping.

**Deliverables**:
- `drawHorizontalLine(x1, x2, y, color)` function
- Screen bounds clipping
- Optimized span fill

**Verification**: Draw horizontal lines at various positions, verify clipping works.

---

### Task 8: Triangle Rasterization
**Goal**: Implement the scan-line triangle rasterizer.

**Deliverables**:
- `drawTriangle(v0, v1, v2, color)` function
- Vertex sorting by Y coordinate
- Edge walking with fixed-point precision
- Proper handling of flat-top and flat-bottom triangles

**Verification**: Draw test triangles of various shapes and orientations.

---

### Task 9: 3D Projection
**Goal**: Implement the perspective projection from 3D to screen coordinates.

**Deliverables**:
- `projectVertex(x, y, z)` returning screen coordinates
- Camera-relative coordinate calculation
- Sub-pixel precision: Fixed coordinates scale directly to physical pixels for smooth movement
- `plotPixelF(Fixed x, Fixed y, color)` for sub-pixel accurate rendering
- Off-screen clipping flag
- Projection constants from original

**Verification**: Project test points and verify screen positions. Verify smooth sub-pixel movement.

---

### Task 10: Landscape Altitude Generation
**Goal**: Implement the terrain height calculation using Fourier synthesis.

**Deliverables**:
- `src/landscape.h` - Landscape functions
- `src/landscape.cpp` - Implementation
- `getAltitude(x, z)` - Returns terrain height at world position
- Uses sine table for wave calculation
- Sea level capping
- Launchpad flat zone

**Verification**: Query altitudes at various positions, verify plausible terrain.

---

### Task 11: Basic Landscape Rendering
**Goal**: Render the landscape as a grid of colored tiles.

**Deliverables**:
- Tile corner coordinate calculation
- Project all visible tile corners
- Draw each tile as two triangles
- Altitude-based coloring (green hills, blue sea)
- Back-to-front rendering order

**Verification**: Static landscape view renders correctly.

---

### Task 12: Camera System
**Goal**: Implement camera positioning and movement with the player.

**Deliverables**:
- Camera position (x, y, z) state
- Camera follows player position
- Tile-aligned snapping for stable rendering
- Landscape offset calculation
- View matrix application to vertices

**Verification**: Camera can be moved programmatically, landscape scrolls.

---

### Task 13: Player State and Basic Input
**Goal**: Implement player ship state and mouse input reading.

**Deliverables**:
- `src/player.h` - Player state structure
- `src/player.cpp` - Implementation
- Position (x, y, z), velocity (vx, vy, vz)
- Mouse position reading (SDL)
- Mouse button state
- Escape key detection

**Verification**: Print mouse coordinates, button states to console.

---

### Task 14: Mouse Polar Conversion
**Goal**: Convert mouse position to polar coordinates for ship control.

**Deliverables**:
- `getMousePolar(mouseX, mouseY)` returning (distance, angle)
- Use arctangent lookup table
- Scale to match original sensitivity
- Center-relative calculation

**Verification**: Move mouse, verify distance/angle change appropriately.

---

### Task 15: Ship Rotation Matrix
**Goal**: Calculate the ship's orientation matrix from mouse input.

**Deliverables**:
- Pitch angle from mouse distance
- Yaw angle from mouse angle
- `calculateShipRotation(pitch, yaw)` returning Mat3x3
- Extract thrust vector from matrix (nose direction)

**Verification**: Move mouse, log rotation matrix values.

---

### Task 16: Ship 3D Model
**Goal**: Define the ship's 3D model data structure.

**Deliverables**:
- `src/object3d.h` - Object model structures
- `src/object3d.cpp` - Ship model data
- Ship vertex data (9 vertices)
- Ship face data (9 triangles)
- Face normals for backface culling
- Face colors

**Verification**: Data structures compile, unit tests pass.

---

### Task 17: 3D Object Rendering
**Goal**: Implement general 3D object rendering with transformations.

**Deliverables**:
- `drawObject(model, position, rotation, scale)` function
- Vertex transformation by rotation matrix
- Camera-relative positioning
- Backface culling using dot product
- Per-face projection and triangle drawing

**Verification**: Render a simple cube or the ship model statically.

---

### Task 18: Ship Rendering Integration
**Goal**: Render the player's ship with correct orientation.

**Deliverables**:
- Ship rendered at player position
- Ship rotated by player's rotation matrix
- Visible on screen following camera

**Verification**: Ship appears on screen, rotates with mouse movement.

---

### Task 19: Ship Physics
**Goal**: Implement ship movement physics (thrust, gravity, friction).

**Deliverables**:
- Gravity acceleration (constant downward)
- Thrust application (from rotation matrix, when button held)
- Hover thrust (reduced, middle button)
- Friction/damping (velocity decay)
- Position integration (pos += vel each frame)
- Fuel consumption

**Verification**: Ship falls with gravity, thrust counters it, friction slows movement.

---

### Task 20: Terrain Collision Detection
**Goal**: Detect when the ship collides with the landscape.

**Deliverables**:
- `checkTerrainCollision(x, y, z)` function
- Undercarriage offset (ship bottom below center)
- Compare ship altitude to terrain altitude
- Return collision state

**Verification**: Flying into terrain triggers collision detection.

---

### Task 21: Shadow Rendering
**Goal**: Project ship shadow onto terrain.

**Deliverables**:
- `drawObjectShadow()` function in object_renderer
- For each vertex, calculate world position and look up terrain altitude
- Project shadow vertices (same X/Z but Y = terrain altitude)
- Draw upward-facing faces as black triangles
- Shadow rendered before ship so it appears underneath

**Verification**: Ship shadow visible on launchpad and terrain.

---

### Task 22: Launchpad Landing
**Goal**: Implement safe landing detection on the launchpad.

**Deliverables**:
- Launchpad zone detection (position check)
- Landing velocity check (below threshold)
- Successful landing state
- Score increment on landing
- Reset for next round

**Verification**: Land gently on launchpad, score increases.

---

### Task 23: Crash Handling
**Goal**: Implement crash detection and response.

**Deliverables**:
- Crash detection (terrain collision + velocity check)
- Life decrement
- Ship explosion state
- Respawn on launchpad after delay
- Game over when lives = 0

**Verification**: Crash into terrain, lose life, respawn.

---

### Task 24: Particle System Foundation
**Goal**: Implement the basic particle data structure and update loop.

**Deliverables**:
- `src/particles.h` - Particle structures
- `src/particles.cpp` - Implementation
- Particle array (max 484)
- Per-particle: position, velocity, lifespan, flags, color
- `updateParticles()` - Position integration, lifespan countdown
- `addParticle()` - Spawn new particle
- Particle removal when lifespan expires

**Verification**: Spawn particles, watch them move and disappear.

---

### Task 25: Particle Rendering
**Goal**: Render particles as small sprites/points.

**Deliverables**:
- Project particle positions to screen
- Draw as small rectangles (scaled pixels)
- Color based on particle type/age
- Skip off-screen particles

**Verification**: Particles visible on screen, correct colors.

---

### Task 26: Exhaust Particles
**Goal**: Spawn exhaust particles from ship thrust.

**Deliverables**:
- Spawn particles behind ship when thrusting
- Initial velocity opposite to thrust direction
- Smoke-like behavior (rising, fading)
- Color gradient (white → gray)

**Verification**: Thrust produces visible exhaust trail.

---

### Task 27: Bullet Particles
**Goal**: Implement bullet firing from the ship.

**Deliverables**:
- Fire bullets on right mouse button
- Bullets spawn in front of ship
- High velocity in nose direction
- Short lifespan
- Ammo tracking

**Verification**: Firing produces fast-moving bullet particles.

---

### Task 28: Particle Terrain Collision
**Goal**: Particles bounce off or splash into terrain.

**Deliverables**:
- Check particle altitude vs terrain
- Bounce particles off terrain (velocity reflection)
- Water splash effect for sea impacts
- Terrain impact effects (sparks)

**Verification**: Particles interact with terrain correctly.

---

### Task 29: Explosion Particles
**Goal**: Spawn explosion effects on crash or object destruction.

**Deliverables**:
- `spawnExplosion(x, y, z)` function
- Multiple particles in random directions
- Color progression (yellow → orange → red)
- Gravity-affected debris

**Verification**: Explosions look appropriate.

---

### Task 30: Object Map System
**Goal**: Implement the object placement grid.

**Deliverables**:
- 256x256 byte array for object map
- Object type enumeration (tree, building, rocket, etc.)
- `getObjectAt(tileX, tileZ)` function
- `setObjectAt(tileX, tileZ, type)` function

**Verification**: Can read/write object map entries.

---

### Task 31: Random Object Placement
**Goal**: Populate the landscape with random objects at game start.

**Deliverables**:
- `placeObjects()` function
- Random number generator (matching original algorithm)
- Skip sea-level tiles
- Skip launchpad area
- Weighted object type selection

**Verification**: Objects placed at sensible locations.

---

### Task 32: Object 3D Models
**Goal**: Define 3D models for all object types.

**Deliverables**:
- Small leafy tree model
- Tall leafy tree model
- Gazebo model
- Building model
- Rocket model
- Smoking wreckage models

**Verification**: Models can be loaded/inspected.

---

### Task 33: Object Rendering
**Goal**: Render static objects on the landscape.

**Deliverables**:
- Iterate visible tiles
- For each object, calculate world position
- Draw object using `drawObject()`
- Back-to-front ordering

**Verification**: Objects visible on landscape at correct positions.

---

### Task 34: Graphics Buffers for Depth Sorting
**Goal**: Implement the depth-sorted graphics buffer system.

**Deliverables**:
- `src/graphics_buffer.h/cpp`
- One buffer per tile row (11 total)
- Store triangle data for deferred rendering
- `addTriangleToBuffer(row, v0, v1, v2, color)`
- `drawBuffer(row)` function
- Terminator markers

**Verification**: Objects drawn in correct depth order.

---

### Task 35: Object-Buffer Integration
**Goal**: Objects write to graphics buffers instead of immediate drawing.

**Deliverables**:
- Objects add triangles to appropriate row buffer
- Buffers drawn after landscape row
- Correct Z-ordering between objects and terrain

**Verification**: Objects correctly sorted with landscape.

---

### Task 36: Bullet-Object Collision
**Goal**: Bullets destroy objects on impact.

**Deliverables**:
- Check bullet particles against object map
- On collision, replace object with wreckage
- Spawn smoke particles
- Increment score
- Remove bullet particle

**Verification**: Shooting objects destroys them.

---

### Task 37: Smoke from Destroyed Objects
**Goal**: Destroyed objects emit rising smoke.

**Deliverables**:
- Wreckage objects spawn smoke particles over time
- Smoke rises and fades
- Smoke color (gray tones)

**Verification**: Destroyed objects visibly smoke.

---

### Task 38: Particle Spawn Locations
**Goal**: Fix bullet and exhaust particle spawn positions to match ship geometry.

**Research**: Examine ship model vertex data in `object3d.cpp` to identify nose vertices and exhaust port triangle.

**Deliverables**:
- Bullets spawn from midpoint of the two nose vertices (not ship center)
- Exhaust spawns from center of yellow triangular exhaust port on ship underside
- Transform local model coordinates through ship rotation matrix to get world positions
- Helper function to get transformed spawn points from ship state

**Verification**: Visual inspection - bullets emerge from ship nose, exhaust emerges from underside exhaust port.

---

### Task 39: Exhaust Particle Tuning
**Goal**: Reduce exhaust particle spawn rate to be consistent with other particle effects.

**Research**: None required.

**Deliverables**:
- Reduce number of exhaust particles spawned per frame
- Maintain visual quality while using fewer particles
- Iterative tuning based on user feedback

**Verification**: Visual comparison with other particle effects; user approval of aesthetic result.

---

### Task 40: Bullet Reimplementation
**Goal**: Reimplement bullet behavior to exactly match the original game.

**Research**: Study `Lander.arm` disassembly to understand original bullet mechanics - velocity, trajectory, lifespan, spawn rate, and any other relevant parameters.

**Deliverables**:
- Complete rewrite of bullet spawning and behavior
- Match original velocity and trajectory calculations
- Match original lifespan and spawn timing
- Match original collision behavior

**Verification**: Side-by-side comparison with original game behavior.

---

### Task 41: Particle-Ship Depth Sorting
**Goal**: Correct draw order so particles render in front of or behind ship based on depth.

**Research**: Study `Lander.arm` to understand how original game handled particle/ship depth sorting.

**Deliverables**:
- Particles compare depth with ship relative to camera
- Particles behind ship (from camera perspective) draw before ship
- Particles in front of ship draw after ship
- May require integrating particles into graphics buffer system

**Verification**: Rotate ship in various directions; particles correctly appear in front/behind based on orientation.

---

### Task 42: Particle Visibility Clipping
**Goal**: Only render particles within the visible landscape region.

**Research**: None required - use existing landscape visibility bounds.

**Deliverables**:
- Check particle world position against visible tile bounds before rendering
- Particles outside visible region are skipped (not removed, just not drawn)
- Smoke from distant destroyed objects no longer renders when out of view

**Verification**: Destroy object, fly away - smoke stops rendering when tile leaves visible area.

---

### Task 43: Star Particles [REMOVED]
**Status**: Removed - not part of original Lander demo.

---

### Task 44: Smooth Ship Controls
**Goal**: Eliminate discrete stepping in ship rotation and tilt for precise aiming.

**Research**: None required - this is interpolation/smoothing of existing control system.

**Deliverables**:
- Ship orientation changes smoothly rather than in discrete chunks
- Interpolation or higher-resolution angle calculations
- Possibly adjust control sensitivity
- Iterative tuning based on playtesting

**Verification**: Mouse movement produces smooth ship rotation; precise aiming at ground targets is achievable.

---

### Task 45: Sound Effects
**Goal**: Integrate sound effects from Amiga Virus with spatial audio.

**Research**: None required - user will provide WAV files.

**Deliverables**:
- SDL audio initialization and management
- Load WAV files from `sounds/` directory
- Play appropriate sounds for game events (thrust, bullets, explosions, etc.)
- Spatial audio: volume scales with distance from player
- Sound effect triggers integrated into game systems

**Verification**: All game events produce appropriate sounds; distant sounds are quieter than nearby sounds.

---

### Task 46: Falling Rocks (includes Task 47: Rock-Ship Collision)
**Goal**: Implement the falling rock hazard system with player collision.

**Deliverables**:
- Rock spawning when score ≥ 800 (probability increases with score)
- Rocks spawn in 30-tile radius circle around player, 32 tiles above
- Rock 3D model (rotating cube) rendered as particle with IS_ROCK flag
- Rock physics (gravity, bounces off terrain)
- Rock shadows rendered on terrain
- Rocks explode 1 tile above ground/water with medium explosion
- Rock explosion plays boom sound
- Rocks clip to visible landscape area
- Rock-player collision triggers crash (1-tile collision radius)
- Frame rate scaling for spawn checks (every 8th frame at 120fps)

**Verification**: Rocks spawn when score is high, fall and explode on impact, kill player on collision.

---

### Task 48: Score Display
**Goal**: Render the current score on screen.

**Deliverables**:
- Score variable tracking
- Text/number rendering (simple bitmap font or rectangles)
- Position below play area
- High score tracking

**Verification**: Score displays and updates correctly.

---

### Task 49: Fuel Gauge (includes Task 50 and Task 51)
**Goal**: Render the fuel level indicator, lives display, and complete game state machine.

**Deliverables**:
- Fuel bar rendering (3-pixel tall green bar below score bar, matching original)
- Fuel level tracking and consumption on thrust (bits 1-2 of button state)
- Engine disabled when fuel is empty
- Lives display at column 30 of score bar
- Game state machine: Playing, Exploding, GameOver
- "GAME OVER - press a key to start again" message on game over
- Wait for keypress before restarting (not auto-restart)
- Full game reset on new game

**Verification**: Fuel bar depletes when thrusting, engine stops when empty, lives decrement on crash, game over message shows and waits for keypress.

---

### Task 52: Frame Timing and Physics Scaling
**Goal**: Implement user-selectable frame rates with appropriate physics scaling.

**Deliverables**:
- Support for 120/60/30/15 FPS options (user-selectable via key or menu)
- Lookup table for physics constants at each frame rate (gravity, thrust, friction)
- Original Lander ran at ~15 FPS; current implementation scaled for ~120 FPS
- Frame rate limiting with vsync support
- Smooth visual updates at all supported frame rates

**Verification**: Game plays consistently at all supported frame rates with matching physics feel.

---

### Task 53: Polish and Bug Fixes
**Goal**: Final cleanup and bug fixing.

**Deliverables**:
- Sutherland-Hodgman triangle clipping for smooth screen-edge rendering (Zarch-style),
  disabled by default but toggled with the 4 key
- Save and load current settings (landscape size, fps, resolution, full screen,
  smooth screen edge rendering, audio)
- Code cleanup
- Performance optimization if needed
- Final verification against original
- Release build deployed to itch.io for Mac and Windows

**Verification**: Game plays correctly end-to-end.

---

## Summary

| Phase | Tasks | Description |
|-------|-------|-------------|
| 1 | 1-4 | Foundation (project, math, tables) |
| 2 | 5-9 | Rendering (buffer, colors, triangles, projection) |
| 3 | 10-12 | Landscape (terrain, rendering, camera) |
| 4 | 13-19 | Player (input, physics, model, rendering) |
| 5 | 20-23 | Collision (terrain, shadow, landing, crash) |
| 6 | 24-29 | Particles (system, rendering, types) |
| 7 | 30-37 | Objects (map, models, rendering, destruction) |
| 8 | 34-35 | Graphics buffers (depth sorting) |
| 9 | 38-42 | Particle refinements (spawn locations, depth sorting, clipping) |
| 10 | 44 | Smooth ship controls |
| 11 | 45 | Sound effects |
| 12 | 46 | Falling Rocks (merged with Task 47) |
| 13 | 48-52 | UI and game state |
| 14 | 53 | Polish |

**Total: 52 tasks** (Task 43 removed - not part of original demo)

Each task is designed to be:
- Completable in a focused session
- Independently testable
- Buildable on previous work
- Reviewable before committing

---

## Working Process

For each task:
1. I implement the task
2. You review and test
3. If approved, we commit
4. Proceed to next task

This ensures incremental progress with verification at each step.
