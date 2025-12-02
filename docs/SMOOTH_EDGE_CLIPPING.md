# Smooth Edge Clipping for Landscape Rendering

## Problem Description

The landscape is rendered as a scrolling grid of tiles (default 12x10) centered on the player. As the player moves, new tiles come into view at the edges while tiles on the opposite edge disappear.

Currently, tiles "pop" in and out of existence abruptly:
- A tile is either fully visible or completely invisible
- There's no partial rendering of tiles at the edges
- This creates a jarring visual effect as the landscape scrolls

The desired behavior is smooth scrolling where tiles at the edges are **partially visible** - clipped against the visible landscape boundary so they gradually slide in/out of view.

## Current Implementation

### The Visible Landscape Box

The game already defines a rectangular box in world space that determines which tiles are visible:
- Centered on the player's position
- Size: TILES_X × TILES_Z (e.g., 12×10 tiles)
- Moves with the player as they fly around

Currently, a tile is either fully inside this box (drawn) or fully outside (not drawn). The box boundaries are at integer tile positions, so tiles pop in/out as the player crosses tile boundaries.

### Rendering Flow

1. **Tile Selection**: The renderer iterates over the TILES_X × TILES_Z grid centered on the camera
2. **Corner Projection**: Each tile corner is projected from 3D to 2D screen coordinates
3. **Tile Drawing**: Each tile is drawn as two triangles

### Why Tiles Pop

The tile grid snaps to integer tile boundaries. When the player moves from tile (5,5) to tile (6,5):
- The left edge of the visible area jumps from x=0 to x=1
- A whole column of tiles suddenly appears on the right
- A whole column suddenly disappears on the left

There's no mechanism to show partial tiles at the edges.

## Solution: 3D Clipping Against the Visible Box

### Key Insight

The clipping must happen in **3D world space**, not in 2D screen space. We clip the edge tiles against the visible landscape box boundaries, then project and draw only the visible portion.

### The Clipping Box

The visible landscape box is defined by 4 vertical planes in world space:
- **Left plane**: x = camera_x - (TILES_X / 2) * TILE_SIZE + camera_x_fraction
- **Right plane**: x = camera_x + (TILES_X / 2) * TILE_SIZE + camera_x_fraction
- **Near plane**: z = camera_z - some_offset + camera_z_fraction
- **Far plane**: z = camera_z + TILES_Z * TILE_SIZE + camera_z_fraction

The `camera_x_fraction` and `camera_z_fraction` are the sub-tile position of the camera, which is what makes the scrolling smooth - the box edges move continuously with the player, not in discrete tile jumps.

There is no top/bottom clipping (no horizontal planes) - just the 4 vertical planes.

### Which Tiles Need Clipping

Only tiles at the edges of the visible area need clipping:
- **Left edge**: First column of tiles (col = 0)
- **Right edge**: Last column of tiles (col = TILES_X - 2, since we draw between corners)
- **Near edge**: First row of tiles (row = TILES_Z - 1, nearest to camera)
- **Far edge**: Last row of tiles (row = 0, furthest from camera)

Interior tiles are fully inside the box and need no clipping.

### Implementation Plan

#### Step 1: Calculate Clipping Planes

In `LandscapeRenderer::render()`, calculate the world-space coordinates of the 4 clipping planes based on camera position (including fractional part).

```cpp
Fixed clipLeft = camX - Fixed::fromInt(TILES_X / 2);
Fixed clipRight = camX + Fixed::fromInt(TILES_X / 2);
Fixed clipNear = camZ - someOffset;
Fixed clipFar = camZ + Fixed::fromInt(TILES_Z);
```

#### Step 2: Identify Edge Tiles

When iterating through tiles, detect if we're on an edge:
```cpp
bool isLeftEdge = (col == 0);
bool isRightEdge = (col == TILES_X - 2);
bool isNearEdge = (row == TILES_Z - 1);
bool isFarEdge = (row == 0);
bool needsClipping = isLeftEdge || isRightEdge || isNearEdge || isFarEdge;
```

#### Step 3: Clip Edge Tiles in 3D

For tiles that need clipping:
1. Get the 4 corners in world space (x, y, z for each)
2. Clip the quad against the relevant plane(s):
   - Left edge tiles: clip against left plane
   - Right edge tiles: clip against right plane
   - Near edge tiles: clip against near plane
   - Far edge tiles: clip against far plane
   - Corner tiles: clip against two planes
3. The result is a polygon with 3-6 vertices (quad clipped against 1-2 planes)

#### Step 4: Project and Draw Clipped Polygon

1. Project each vertex of the clipped polygon to screen coordinates
2. Triangulate the polygon (fan triangulation from first vertex)
3. Draw the resulting triangles

### Clipping a Quad Against a Vertical Plane

For a vertical clipping plane at x = clipX:

```cpp
// For each edge of the quad, check if it crosses the plane
// If one vertex is inside (x >= clipX) and one is outside (x < clipX):
//   - Calculate intersection point: lerp based on x distance to plane
//   - The y and z coordinates are interpolated accordingly
```

The intersection calculation for an edge from P1 to P2 against plane x = clipX:
```cpp
float t = (clipX - P1.x) / (P2.x - P1.x);
intersection.x = clipX;
intersection.y = P1.y + t * (P2.y - P1.y);
intersection.z = P1.z + t * (P2.z - P1.z);
```

### Files to Modify

- `src/clipping.h/cpp` - New files for 3D quad clipping
- `src/landscape_renderer.cpp` - Calculate clip planes, clip edge tiles before drawing
- `src/main.cpp` - Add key toggle for smooth clipping (key 4)
- `CMakeLists.txt` - Add new source files

### Testing

1. Enable clipping with key 4
2. Fly around slowly and observe edges of the landscape
3. Tiles should smoothly slide in/out at all edges (left, right, near, far)
4. Interior tiles should render exactly as before
5. No visual artifacts or gaps at tile boundaries
6. Performance should be acceptable (only edge tiles are clipped)

## Summary

The solution is to:
1. Define the visible landscape box with continuous (not snapped) boundaries
2. Clip edge tiles against these boundaries in 3D world space
3. Project and draw only the visible portion of each edge tile
4. Interior tiles are drawn as normal without clipping
