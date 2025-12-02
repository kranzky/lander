// landscape_renderer.cpp
// Renders the 3D landscape terrain

#include "landscape_renderer.h"
#include "graphics_buffer.h"
#include "particles.h"
#include "clipping.h"

using namespace GameConstants;

// Frame counter for smoke spawning (static to persist across calls)
static uint32_t smokeFrameCounter = 0;

// =============================================================================
// Constructor
// =============================================================================

LandscapeRenderer::LandscapeRenderer()
{
    // Initialize corner storage
    for (int i = 0; i < MAX_CORNERS; i++) {
        currentRow[i] = {0, 0, Fixed(), false};
        previousRow[i] = {0, 0, Fixed(), false};
    }
}

// =============================================================================
// Corner Projection
// =============================================================================

LandscapeRenderer::CornerData LandscapeRenderer::projectCorner(
    Fixed worldX, Fixed worldY, Fixed worldZ,
    Fixed camX, Fixed camY, Fixed camZ)
{
    CornerData corner;
    corner.altitude = worldY;
    corner.valid = false;

    // Transform to camera-relative coordinates
    // X: offset from camera X (centered on landscape)
    // Y: altitude relative to camera Y
    // Z: distance from camera (into the screen)
    Fixed relX = worldX - camX;
    Fixed relY = worldY - camY;
    Fixed relZ = worldZ - camZ;

    // Project to screen
    ProjectedVertex proj = projectVertex(relX, relY, relZ);

    if (proj.visible) {
        corner.screenX = proj.screenX;
        corner.screenY = proj.screenY;
        corner.valid = true;
    }

    return corner;
}

LandscapeRenderer::CornerData LandscapeRenderer::projectCornerRelative(
    Fixed relX, Fixed relY, Fixed relZ)
{
    CornerData corner;
    corner.altitude = Fixed();  // Set by caller
    corner.valid = false;

    // Project to screen (coordinates are already camera-relative)
    ProjectedVertex proj = projectVertex(relX, relY, relZ);

    if (proj.visible) {
        corner.screenX = proj.screenX;
        corner.screenY = proj.screenY;
        corner.valid = true;
    }

    return corner;
}

// =============================================================================
// Tile Type Detection
// =============================================================================

TileType LandscapeRenderer::getTileType(Fixed x, Fixed z, Fixed altitude) {
    // Check for launchpad area (x and z both < 8 tiles from origin)
    if (x < LAUNCHPAD_SIZE && z < LAUNCHPAD_SIZE &&
        x.raw >= 0 && z.raw >= 0) {
        return TileType::Launchpad;
    }

    // Check for sea level
    if (altitude >= SEA_LEVEL) {
        return TileType::Sea;
    }

    return TileType::Land;
}

// =============================================================================
// Tile Drawing
// =============================================================================

void LandscapeRenderer::drawTile(
    ScreenBuffer& screen,
    const CornerData& topLeft, const CornerData& topRight,
    const CornerData& bottomLeft, const CornerData& bottomRight,
    int tileRow, Fixed tileX, Fixed tileZ,
    int clipFlags, Fixed clipLeftX, Fixed clipRightX,
    Fixed clipNearZ, Fixed clipFarZ)
{
    // Calculate tile color (before clipping, using original corners)
    // Use average altitude for color determination
    Fixed avgAltitude = Fixed::fromRaw(
        (topLeft.altitude.raw + topRight.altitude.raw +
         bottomLeft.altitude.raw + bottomRight.altitude.raw) / 4);

    // Calculate slope (difference between left and right sides)
    // Positive slope = left side higher = faces light source
    int32_t leftAvg = (topLeft.altitude.raw + bottomLeft.altitude.raw) / 2;
    int32_t rightAvg = (topRight.altitude.raw + bottomRight.altitude.raw) / 2;
    int32_t slope = leftAvg - rightAvg;
    if (slope < 0) slope = 0;

    // Determine tile type
    TileType type = getTileType(tileX, tileZ, avgAltitude);

    // Get color
    Color color = getLandscapeTileColor(avgAltitude.raw, tileRow, slope, type);


    // If no clipping needed, use the fast path
    if (clipFlags == CLIP_NONE) {
        // All corners must be valid to draw
        if (!topLeft.valid || !topRight.valid ||
            !bottomLeft.valid || !bottomRight.valid) {
            return;
        }

        // Draw as two triangles
        // Triangle 1: topLeft, topRight, bottomLeft
        screen.drawTriangle(
            topLeft.screenX, topLeft.screenY,
            topRight.screenX, topRight.screenY,
            bottomLeft.screenX, bottomLeft.screenY,
            color);

        // Triangle 2: topRight, bottomRight, bottomLeft
        screen.drawTriangle(
            topRight.screenX, topRight.screenY,
            bottomRight.screenX, bottomRight.screenY,
            bottomLeft.screenX, bottomLeft.screenY,
            color);
        return;
    }

    // Clipping path: build quad from 3D coordinates, clip, then project
    // Quad corners in order: topLeft, topRight, bottomRight, bottomLeft
    ClipVertex3D quad[4] = {
        {topLeft.relX, topLeft.relY, topLeft.relZ},
        {topRight.relX, topRight.relY, topRight.relZ},
        {bottomRight.relX, bottomRight.relY, bottomRight.relZ},
        {bottomLeft.relX, bottomLeft.relY, bottomLeft.relZ}
    };

    // Start with the quad as a polygon
    ClippedPolygon3D poly;
    poly.count = 4;
    for (int i = 0; i < 4; i++) {
        poly.vertices[i] = quad[i];
    }

    // Clip against each requested plane
    if (clipFlags & CLIP_LEFT) {
        poly = clipPolygonLeft(poly, clipLeftX);
        if (poly.count < 3) return;
    }
    if (clipFlags & CLIP_RIGHT) {
        poly = clipPolygonRight(poly, clipRightX);
        if (poly.count < 3) return;
    }
    if (clipFlags & CLIP_NEAR) {
        poly = clipPolygonNear(poly, clipNearZ);
        if (poly.count < 3) return;
    }
    if (clipFlags & CLIP_FAR) {
        poly = clipPolygonFar(poly, clipFarZ);
        if (poly.count < 3) return;
    }

    // Project clipped vertices to screen
    int screenX[MAX_CLIP_VERTICES_3D];
    int screenY[MAX_CLIP_VERTICES_3D];
    for (int i = 0; i < poly.count; i++) {
        ProjectedVertex proj = projectVertex(poly.vertices[i].x, poly.vertices[i].y, poly.vertices[i].z);
        if (!proj.visible) {
            // Vertex behind camera - skip this polygon
            return;
        }
        screenX[i] = proj.screenX;
        screenY[i] = proj.screenY;
    }

    // Triangulate and draw using fan triangulation
    for (int i = 1; i < poly.count - 1; i++) {
        screen.drawTriangle(
            screenX[0], screenY[0],
            screenX[i], screenY[i],
            screenX[i + 1], screenY[i + 1],
            color);
    }
}

// =============================================================================
// Main Render Function
// =============================================================================

void LandscapeRenderer::render(ScreenBuffer& screen, const Camera& camera)
{
    // The landscape is rendered as a grid of tiles that scrolls with the camera.
    // The tile grid follows the camera position, creating infinite scrolling terrain.
    //
    // Two coordinate systems are used:
    // 1. Screen-relative coordinates: used for projection (based on landscape offset)
    // 2. World coordinates: used for altitude lookup (based on camera tile position)
    //
    // The camera's fractional position provides smooth sub-tile scrolling.

    // Get camera position components
    Fixed camY = camera.getY();
    Fixed camXTile = camera.getXTile();  // Integer tile position
    Fixed camZTile = camera.getZTile();
    Fixed camXFrac = camera.getXFraction();  // Fractional position within tile
    Fixed camZFrac = camera.getZFraction();

    // Landscape offset for screen positioning (matches original Lander.arm)
    // X: centers the grid on screen (half the width)
    // Z: pushes landscape forward from camera (depth + offset)
    int32_t LANDSCAPE_X_RAW = (TILES_X - 2) * TILE_SIZE.raw / 2;
    int32_t LANDSCAPE_Z_RAW = ((TILES_Z - 1) + 10) * TILE_SIZE.raw;

    // Screen-relative starting position = landscapeOffset - camera_fraction
    // This creates smooth scrolling as camera moves within a tile
    int32_t startX = -LANDSCAPE_X_RAW - camXFrac.raw;
    int32_t startZ = LANDSCAPE_Z_RAW - camZFrac.raw;

    // World coordinate base - the tile grid follows the camera
    // Convert camera tile position to integer tile indices
    int camTileX = camXTile.toInt();
    int camTileZ = camZTile.toInt();

    // Center the grid on camera position
    int halfTilesX = TILES_X / 2;

    // When clipping is enabled, render 1 extra tile on each edge
    // These will be clipped against the visible boundary
    int extraTiles = ClippingConfig::enabled ? 1 : 0;
    int colStart = -extraTiles;           // Start 1 tile to the left
    int colEnd = TILES_X + extraTiles;    // End 1 tile to the right
    int rowStart = -extraTiles;           // Start 1 row further back
    int rowEnd = TILES_Z + extraTiles;    // End 1 row closer

    // Adjust starting position for extra tiles
    int32_t adjustedStartX = startX - (extraTiles * TILE_SIZE.raw);
    int32_t adjustedStartZ = startZ + (extraTiles * TILE_SIZE.raw);

    // Calculate clipping planes (in screen-relative/camera coordinates)
    // These are the FIXED screen boundaries - they don't move with camera fraction!
    // This is what creates the smooth scrolling effect: tiles slide past fixed clip planes.
    //
    // When camFrac = 0, the original grid fits exactly in the clip box.
    // When camFrac != 0, tiles slide partially outside the clip box and get clipped.
    //
    // Use the base landscape offset (without camera fraction) for clip planes:
    int32_t baseStartX = -LANDSCAPE_X_RAW;  // Without camXFrac
    int32_t baseStartZ = LANDSCAPE_Z_RAW;   // Without camZFrac

    // Left clipping plane: leftmost screen boundary
    Fixed clipLeftX = Fixed::fromRaw(baseStartX);
    // Right clipping plane: rightmost screen boundary
    Fixed clipRightX = Fixed::fromRaw(baseStartX + (TILES_X - 1) * TILE_SIZE.raw);
    // Far clipping plane: furthest screen boundary
    Fixed clipFarZ = Fixed::fromRaw(baseStartZ);
    // Near clipping plane: nearest screen boundary
    Fixed clipNearZ = Fixed::fromRaw(baseStartZ - (TILES_Z - 1) * TILE_SIZE.raw);

    // Process rows from back (far) to front (near)
    // Row 0 is furthest, Row TILES_Z-1 is nearest
    for (int row = rowStart; row < rowEnd; row++) {
        // Calculate screen-relative Z for this row of corners
        int32_t relZRaw = adjustedStartZ - ((row + extraTiles) * TILE_SIZE.raw);
        Fixed relZ = Fixed::fromRaw(relZRaw);

        // World Z for this row - tiles extend forward from camera
        // Row 0 = back (camTileZ + TILES_Z - 1), Row TILES_Z-1 = front (camTileZ)
        int worldZInt = camTileZ + (TILES_Z - 1 - row);

        // Array index (offset by extraTiles to handle negative row indices)
        int rowIdx = row + extraTiles;

        // Process each corner in this row
        for (int col = colStart; col < colEnd; col++) {
            // Array index (offset by extraTiles to handle negative col indices)
            int colIdx = col + extraTiles;

            // Calculate screen-relative X for this corner
            int32_t relXRaw = adjustedStartX + (colIdx * TILE_SIZE.raw);
            Fixed relX = Fixed::fromRaw(relXRaw);

            // World X for this corner - centered on camera tile position
            int worldXInt = camTileX - halfTilesX + col;

            // Convert to Fixed for altitude lookup
            Fixed worldX = Fixed::fromInt(worldXInt);
            Fixed worldZ = Fixed::fromInt(worldZInt);

            // Get altitude at this corner
            Fixed altitude = getLandscapeAltitude(worldX, worldZ);

            // The relative Y is altitude minus camera Y
            Fixed relY = Fixed::fromRaw(altitude.raw - camY.raw);

            // Project corner to screen (coordinates are already camera-relative)
            currentRow[colIdx] = projectCornerRelative(relX, relY, relZ);
            currentRow[colIdx].altitude = altitude;
            // Store 3D coordinates for clipping
            currentRow[colIdx].relX = relX;
            currentRow[colIdx].relY = relY;
            currentRow[colIdx].relZ = relZ;
        }

        // Draw tiles (need at least one previous row)
        if (row > rowStart) {
            for (int col = colStart; col < colEnd - 1; col++) {
                int colIdx = col + extraTiles;

                // World tile coordinates for color/type lookup
                int worldXInt = camTileX - halfTilesX + col;
                int worldZInt = camTileZ + (TILES_Z - row);
                Fixed tileX = Fixed::fromInt(worldXInt);
                Fixed tileZ = Fixed::fromInt(worldZInt);

                // Determine clip flags for edge tiles
                // We need to clip both:
                // - Extra tiles sliding INTO view (col=-1, col=TILES_X-1, row=0, row=TILES_Z)
                // - Original edge tiles sliding OUT of view (col=0, col=TILES_X-2, row=1, row=TILES_Z-1)
                int clipFlags = CLIP_NONE;
                if (ClippingConfig::enabled) {
                    // Left edge: extra tile (col=-1) and first original tile (col=0)
                    if (col <= 0) clipFlags |= CLIP_LEFT;
                    // Right edge: last original tile (col=TILES_X-2) and extra tile (col=TILES_X-1)
                    if (col >= TILES_X - 2) clipFlags |= CLIP_RIGHT;
                    // Far edge: extra row (row=0) and first original row (row=1)
                    if (row <= 1) clipFlags |= CLIP_FAR;
                    // Near edge: last original row (row=TILES_Z-1) and extra row (row=TILES_Z)
                    if (row >= TILES_Z - 1) clipFlags |= CLIP_NEAR;
                }

                drawTile(screen,
                         previousRow[colIdx], previousRow[colIdx + 1],
                         currentRow[colIdx], currentRow[colIdx + 1],
                         row, tileX, tileZ,
                         clipFlags, clipLeftX, clipRightX, clipNearZ, clipFarZ);
            }

            // Draw objects for this row (buffered by renderObjects())
            // Row mapping: render row R draws tiles for object buffer row R-1
            // Only draw object buffers for valid row indices
            if (row > 0 && row <= TILES_Z) {
                graphicsBuffers.drawAndClearRow(row - 1, screen);
            }
        }

        // Swap rows: current becomes previous for next iteration
        for (int col = colStart; col < colEnd; col++) {
            int colIdx = col + extraTiles;
            previousRow[colIdx] = currentRow[colIdx];
        }
    }

    // Flush the final row buffer (nearest objects, in front of all landscape)
    graphicsBuffers.drawAndClearRow(TILES_Z - 1, screen);
}

// =============================================================================
// Object Rendering
// =============================================================================
//
// Port of DrawObjects from Lander.arm (lines 4679-4948).
//
// Iterates through the visible tiles from back to front, checking the object
// map for each tile. If an object is found, calculates its camera-relative
// position and draws it using drawObject().
//
// Static objects use an identity rotation matrix. The original game stores
// this at rotationMatrix (3x3 identity) for non-rotating objects.
//
// =============================================================================

void LandscapeRenderer::renderObjects(ScreenBuffer& screen, const Camera& camera)
{
    // Clear object buffers at start - they will be flushed during landscape rendering
    graphicsBuffers.clearAll();

    // Increment frame counter for smoke spawning
    // Original spawns smoke every 4 frames at 15fps = 3.75 smoke/sec per object
    // At 120fps, every 32 frames gives the same rate
    // We use every 96 frames (~1.25 smoke/sec) for a more subtle effect
    smokeFrameCounter++;

    // Get camera position for relative coordinate calculation
    Fixed camX = camera.getX();
    Fixed camY = camera.getY();
    Fixed camZ = camera.getZ();

    // Get tile position of camera (integer tile coordinate)
    Fixed camXTile = camera.getXTile();
    Fixed camZTile = camera.getZTile();

    int camTileX = camXTile.toInt();
    int camTileZ = camZTile.toInt();

    // Center the grid on camera position
    int halfTilesX = TILES_X / 2;

    // Identity matrix for static objects
    static const Mat3x3 identityMatrix = Mat3x3::identity();

    // Calculate clipping boundaries for object culling (same as landscape rendering)
    // These are fixed screen boundaries that don't move with camera fraction
    int32_t LANDSCAPE_X_RAW = (TILES_X - 2) * TILE_SIZE.raw / 2;
    int32_t LANDSCAPE_Z_RAW = ((TILES_Z - 1) + 10) * TILE_SIZE.raw;
    Fixed camXFrac = camera.getXFraction();
    Fixed camZFrac = camera.getZFraction();

    // Screen-relative starting position (includes camera fraction for smooth scrolling)
    int32_t startX = -LANDSCAPE_X_RAW - camXFrac.raw;
    int32_t startZ = LANDSCAPE_Z_RAW - camZFrac.raw;

    // Clip boundaries are FIXED (don't include camera fraction)
    // These define the visible area for object culling, matching landscape clip planes
    int32_t baseStartX = -LANDSCAPE_X_RAW;
    int32_t baseStartZ = LANDSCAPE_Z_RAW;
    Fixed clipLeftX = Fixed::fromRaw(baseStartX);
    Fixed clipRightX = Fixed::fromRaw(baseStartX + (TILES_X - 1) * TILE_SIZE.raw);
    Fixed clipFarZ = Fixed::fromRaw(baseStartZ);
    Fixed clipNearZ = Fixed::fromRaw(baseStartZ - (TILES_Z - 1) * TILE_SIZE.raw);

    // Process rows from back (far) to front (near) - same order as DrawObjects
    // This ensures distant objects are drawn before closer ones
    for (int row = 0; row < TILES_Z; row++) {
        // World Z for this row - tiles extend forward from camera
        // Row 0 = back (camTileZ + TILES_Z - 1), Row TILES_Z-1 = front (camTileZ)
        int worldZInt = camTileZ + (TILES_Z - 1 - row);

        // Process each column left to right
        // Start from col=1 to prevent objects extending past the left landscape edge
        // (col=0 would position objects 0.5 tiles left of the leftmost landscape tile)
        // When clipping is enabled, extend iteration to include extra tiles on edges
        int extraTiles = ClippingConfig::enabled ? 1 : 0;
        for (int col = 1 - extraTiles; col < TILES_X + extraTiles; col++) {
            // World X for this tile - centered on camera
            int worldXInt = camTileX - halfTilesX + col;

            // Get object at this tile location
            // The object map wraps around at 256 tiles (8-bit coordinates)
            uint8_t tileX = static_cast<uint8_t>(worldXInt);
            uint8_t tileZ = static_cast<uint8_t>(worldZInt);

            uint8_t objectType = objectMap.getObjectAt(tileX, tileZ);

            // Skip if no object at this tile
            if (objectType == ObjectType::NONE) {
                continue;
            }

            // =================================================================
            // Smoke from Destroyed Objects
            // =================================================================
            // Port of DrawObjects Part 3 from Lander.arm (lines 4910-4947).
            //
            // Destroyed objects (type >= 12) emit smoke particles every ~32
            // frames at 120fps (equivalent to every 4 frames at 15fps).
            // Smoke spawns at SMOKE_HEIGHT (3/4 tile) above the object base.
            //
            if (ObjectMap::isDestroyedType(objectType) && (smokeFrameCounter & 0x5F) == 0)
            {
                // Calculate object's world position for smoke spawning
                Fixed smokeWorldX = Fixed::fromInt(worldXInt);
                Fixed smokeWorldZ = Fixed::fromInt(worldZInt);
                Fixed groundY = getLandscapeAltitude(smokeWorldX, smokeWorldZ);

                // Smoke spawns at SMOKE_HEIGHT above ground (negative Y = upward)
                Vec3 smokePos;
                smokePos.x = smokeWorldX;
                smokePos.y = Fixed::fromRaw(groundY.raw - SMOKE_HEIGHT.raw);
                smokePos.z = smokeWorldZ;

                spawnSmokeParticle(smokePos);
            }

            // Get the blueprint for this object type
            const ObjectBlueprint* blueprint = getObjectBlueprint(objectType);
            if (blueprint == nullptr) {
                continue;
            }

            // Calculate object's world position (center of tile)
            Fixed worldX = Fixed::fromInt(worldXInt);
            Fixed worldZ = Fixed::fromInt(worldZInt);

            // When smooth clipping is enabled, cull objects outside the clip boundaries
            // Check if the object's position (tile center) is within the visible area
            if (ClippingConfig::enabled) {
                // Calculate screen-relative position of the object (tile center)
                // Objects at col sit on the tile between corners (col-1) and col
                // Tile center X = startX + (col-1)*TILE_SIZE + TILE_SIZE/2
                //               = startX + col*TILE_SIZE - TILE_SIZE/2
                // Tile center Z = startZ - row*TILE_SIZE - TILE_SIZE/2
                int32_t objX = startX + col * TILE_SIZE.raw - TILE_SIZE.raw / 2;
                int32_t objZ = startZ - row * TILE_SIZE.raw - TILE_SIZE.raw / 2;

                // Skip if object center is outside clip boundaries
                // Extend boundaries to spawn objects slightly earlier
                if (objX < clipLeftX.raw - TILE_SIZE.raw / 4 || objX > clipRightX.raw + TILE_SIZE.raw / 4 ||
                    objZ > clipFarZ.raw + TILE_SIZE.raw * 3 / 4 || objZ < clipNearZ.raw - TILE_SIZE.raw * 3 / 4) {
                    continue;
                }
            }

            // Get terrain altitude at this position
            Fixed altitude = getLandscapeAltitude(worldX, worldZ);

            // Skip objects on sea level (original does this check)
            if (altitude == SEA_LEVEL) {
                continue;
            }

            // Calculate camera-relative position
            // Z: Use LANDSCAPE_Z_FRONT offset to match landscape projection.
            Vec3 cameraRelPos;
            cameraRelPos.x = Fixed::fromRaw(worldX.raw - camX.raw);
            cameraRelPos.y = Fixed::fromRaw(altitude.raw - camY.raw);
            cameraRelPos.z = Fixed::fromRaw(worldZ.raw - camZ.raw + LANDSCAPE_Z_FRONT.raw);

            // Get world position for shadow calculation
            Vec3 worldPos;
            worldPos.x = worldX;
            worldPos.y = altitude;
            worldPos.z = worldZ;

            Vec3 cameraWorldPos;
            cameraWorldPos.x = camX;
            cameraWorldPos.y = camY;
            cameraWorldPos.z = camZ;

            // Buffer shadow first (so it appears under the object)
            bufferObjectShadow(*blueprint, cameraRelPos, identityMatrix,
                           worldPos, cameraWorldPos, row);

            // Buffer the object (will be drawn after landscape row)
            bufferObject(*blueprint, cameraRelPos, identityMatrix, row);
        }
    }
}
