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

### Task 38: Falling Rocks
**Goal**: Implement the falling rock hazard system.

**Deliverables**:
- `src/rocks.cpp/h`
- Rock spawning when score ≥ 800
- Random spawn timing
- Rock 3D model (cube)
- Rock physics (gravity, rotation)
- Rock rendering as particle type

**Verification**: Rocks spawn and fall when score is high enough.

---

### Task 39: Rock-Ship Collision
**Goal**: Rocks kill the player on contact.

**Deliverables**:
- Distance check between rock and ship
- Trigger crash on collision
- Rock removed on collision

**Verification**: Rock hitting ship causes crash.

---

### Task 40: Score Display
**Goal**: Render the current score on screen.

**Deliverables**:
- Score variable tracking
- Text/number rendering (simple bitmap font or rectangles)
- Position below play area
- High score tracking

**Verification**: Score displays and updates correctly.

---

### Task 41: Fuel Gauge
**Goal**: Render the fuel level indicator.

**Deliverables**:
- Fuel bar rendering (filled rectangle)
- Fuel level tracking
- Fuel consumption on thrust
- Low fuel visual indication

**Verification**: Fuel bar depletes when thrusting.

---

### Task 42: Lives Display
**Goal**: Show remaining lives on screen.

**Deliverables**:
- Lives counter display
- Update on crash
- Game over state when lives = 0

**Verification**: Lives display correctly, game over works.

---

### Task 43: Game State Machine
**Goal**: Implement clean game state transitions.

**Deliverables**:
- States: Playing, Crashed, Landed, GameOver
- State transition logic
- Reset functionality for new game
- Clean initialization

**Verification**: All state transitions work correctly.

---

### Task 44: Frame Timing and Physics Scaling
**Goal**: Implement user-selectable frame rates with appropriate physics scaling.

**Deliverables**:
- Support for 120/60/30/15 FPS options (user-selectable via key or menu)
- Lookup table for physics constants at each frame rate (gravity, thrust, friction)
- Original Lander ran at ~15 FPS; current implementation scaled for ~120 FPS
- Frame rate limiting with vsync support
- Smooth visual updates at all supported frame rates

**Verification**: Game plays consistently at all supported frame rates with matching physics feel.

---

### Task 45: Polish and Bug Fixes
**Goal**: Final cleanup and bug fixing.

**Deliverables**:
- Code cleanup
- Bug fixes found during testing
- Performance optimization if needed
- Sutherland-Hodgman triangle clipping for smooth screen-edge rendering (Zarch-style)
- Allow the player to change the size of the region of landscape that is visible
- Integrate sound effects from the Amiga version of Virus
- Minimap like Zarch
- Title screen with rotating ship
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
| 9 | 38-39 | Rocks (spawning, collision) |
| 10 | 40-44 | UI and game state |
| 11 | 45 | Polish |

**Total: 45 tasks**

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
