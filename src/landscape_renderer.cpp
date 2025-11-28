// landscape_renderer.cpp
// Renders the 3D landscape terrain

#include "landscape_renderer.h"
#include "graphics_buffer.h"

using namespace GameConstants;

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
    int tileRow, Fixed tileX, Fixed tileZ)
{
    // All corners must be valid to draw
    if (!topLeft.valid || !topRight.valid ||
        !bottomLeft.valid || !bottomRight.valid) {
        return;
    }

    // Calculate tile color
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
    // X: -5.5 tiles (centers the grid on screen)
    // Z: 20 tiles (pushes landscape forward from camera)
    constexpr int32_t LANDSCAPE_X_RAW = (TILES_X - 2) * TILE_SIZE.raw / 2;  // 5.5 tiles
    constexpr int32_t LANDSCAPE_Z_RAW = ((TILES_Z - 1) + 10) * TILE_SIZE.raw;  // 20 tiles

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

    // Process rows from back (far) to front (near)
    // Row 0 is furthest, Row TILES_Z-1 is nearest
    for (int row = 0; row < TILES_Z; row++) {
        // Calculate screen-relative Z for this row of corners
        int32_t relZRaw = startZ - (row * TILE_SIZE.raw);
        Fixed relZ = Fixed::fromRaw(relZRaw);

        // World Z for this row - tiles extend forward from camera
        // Row 0 = back (camTileZ + TILES_Z - 1), Row TILES_Z-1 = front (camTileZ)
        int worldZInt = camTileZ + (TILES_Z - 1 - row);

        // Process each corner in this row
        for (int col = 0; col < TILES_X; col++) {
            // Calculate screen-relative X for this corner
            int32_t relXRaw = startX + (col * TILE_SIZE.raw);
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
            currentRow[col] = projectCornerRelative(relX, relY, relZ);
            currentRow[col].altitude = altitude;
        }

        // Draw tiles (need at least one previous row)
        if (row > 0) {
            for (int col = 0; col < TILES_X - 1; col++) {
                // World tile coordinates for color/type lookup
                int worldXInt = camTileX - halfTilesX + col;
                int worldZInt = camTileZ + (TILES_Z - row);
                Fixed tileX = Fixed::fromInt(worldXInt);
                Fixed tileZ = Fixed::fromInt(worldZInt);

                drawTile(screen,
                         previousRow[col], previousRow[col + 1],
                         currentRow[col], currentRow[col + 1],
                         row, tileX, tileZ);
            }

            // Draw objects for this row (buffered by renderObjects())
            // Row mapping: render row R draws tiles for object buffer row R-1
            graphicsBuffers.drawAndClearRow(row - 1, screen);
        }

        // Swap rows: current becomes previous for next iteration
        for (int col = 0; col < TILES_X; col++) {
            previousRow[col] = currentRow[col];
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

    // Process rows from back (far) to front (near) - same order as DrawObjects
    // This ensures distant objects are drawn before closer ones
    for (int row = 0; row < TILES_Z; row++) {
        // World Z for this row - tiles extend forward from camera
        // Row 0 = back (camTileZ + TILES_Z - 1), Row TILES_Z-1 = front (camTileZ)
        int worldZInt = camTileZ + (TILES_Z - 1 - row);

        // Process each column left to right
        // Start from col=1 to prevent objects extending past the left landscape edge
        // (col=0 would position objects 0.5 tiles left of the leftmost landscape tile)
        for (int col = 1; col < TILES_X; col++) {
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

            // Get the blueprint for this object type
            const ObjectBlueprint* blueprint = getObjectBlueprint(objectType);
            if (blueprint == nullptr) {
                continue;
            }

            // Calculate object's world position (center of tile)
            Fixed worldX = Fixed::fromInt(worldXInt);
            Fixed worldZ = Fixed::fromInt(worldZInt);

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
