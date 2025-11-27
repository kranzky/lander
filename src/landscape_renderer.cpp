// landscape_renderer.cpp
// Renders the 3D landscape terrain

#include "landscape_renderer.h"

using namespace GameConstants;

// =============================================================================
// Constructor
// =============================================================================

LandscapeRenderer::LandscapeRenderer()
    : cameraY(Fixed::fromInt(0))
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

void LandscapeRenderer::render(ScreenBuffer& screen, Fixed cameraX, Fixed cameraZ)
{
    // The landscape is a grid of TILES_X-1 by TILES_Z-1 tiles
    // (TILES_X and TILES_Z are the number of corners)

    // Camera position:
    // - X: centered on landscape (LANDSCAPE_X_HALF)
    // - Y: cameraY (height above ground)
    // - Z: at the back of the visible area

    // The original positions the camera so that:
    // - The landscape extends from z=1 to z=TILES_Z in camera space
    // - X is centered (landscape from -HALF_TILES_X to +HALF_TILES_X)

    // For now, use a fixed camera position for testing
    Fixed camX = cameraX;
    Fixed camY = cameraY;
    Fixed camZ = cameraZ;

    // Process rows from back (far) to front (near)
    // Row 0 is furthest, Row TILES_Z-1 is nearest
    for (int row = 0; row < TILES_Z; row++) {
        // Calculate world Z for this row of corners
        // Landscape starts at the back and comes toward camera
        Fixed worldZ = Fixed::fromRaw((TILES_Z - 1 - row) * TILE_SIZE.raw);

        // Process each corner in this row
        for (int col = 0; col < TILES_X; col++) {
            // Calculate world X for this corner
            // Centered: col 0 is left edge, col TILES_X-1 is right edge
            Fixed worldX = Fixed::fromRaw(col * TILE_SIZE.raw);

            // Get altitude at this corner
            Fixed altitude = getLandscapeAltitude(worldX, worldZ);

            // Project corner to screen
            currentRow[col] = projectCorner(worldX, altitude, worldZ,
                                            camX, camY, camZ);
        }

        // Draw tiles (need at least one previous row)
        if (row > 0) {
            for (int col = 0; col < TILES_X - 1; col++) {
                // Four corners of this tile:
                // topLeft = previousRow[col]     (back-left)
                // topRight = previousRow[col+1]  (back-right)
                // bottomLeft = currentRow[col]   (front-left)
                // bottomRight = currentRow[col+1] (front-right)

                Fixed tileX = Fixed::fromRaw(col * TILE_SIZE.raw);
                Fixed tileZ = Fixed::fromRaw((TILES_Z - row) * TILE_SIZE.raw);

                drawTile(screen,
                         previousRow[col], previousRow[col + 1],
                         currentRow[col], currentRow[col + 1],
                         row, tileX, tileZ);
            }
        }

        // Swap rows: current becomes previous for next iteration
        for (int col = 0; col < TILES_X; col++) {
            previousRow[col] = currentRow[col];
        }
    }
}
