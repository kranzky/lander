// landscape_renderer.h
// Renders the 3D landscape terrain using the painter's algorithm

#ifndef LANDSCAPE_RENDERER_H
#define LANDSCAPE_RENDERER_H

#include "screen.h"
#include "projection.h"
#include "landscape.h"
#include "palette.h"

// =============================================================================
// Landscape Renderer
// =============================================================================
//
// Renders the procedural terrain as a grid of quadrilateral tiles using the
// painter's algorithm (back-to-front). Each tile is drawn as two triangles.
//
// The rendering process:
// 1. Position camera at back of landscape, looking forward
// 2. For each row from back to front:
//    a. For each column left to right:
//       - Get altitude at this corner
//       - Project to screen coordinates
//    b. Draw tiles using corners from current and previous rows
// 3. Color tiles based on altitude, distance, and slope
//
// =============================================================================

class LandscapeRenderer {
public:
    LandscapeRenderer();

    // Render the landscape to the screen buffer
    // cameraX, cameraZ: World position of camera (player position)
    void render(ScreenBuffer& screen, Fixed cameraX, Fixed cameraZ);

    // Set camera Y position (altitude above ground)
    void setCameraY(Fixed y) { cameraY = y; }

private:
    // Camera height above the landscape
    Fixed cameraY;

    // Storage for projected corner coordinates
    // We need two rows: current and previous
    struct CornerData {
        int screenX;
        int screenY;
        Fixed altitude;
        bool valid;
    };

    static constexpr int MAX_CORNERS = GameConstants::TILES_X;
    CornerData currentRow[MAX_CORNERS];
    CornerData previousRow[MAX_CORNERS];

    // Project a landscape corner to screen coordinates
    CornerData projectCorner(Fixed worldX, Fixed worldY, Fixed worldZ,
                             Fixed cameraX, Fixed cameraY, Fixed cameraZ);

    // Draw a single tile (quadrilateral) given 4 corners
    void drawTile(ScreenBuffer& screen,
                  const CornerData& topLeft, const CornerData& topRight,
                  const CornerData& bottomLeft, const CornerData& bottomRight,
                  int tileRow, Fixed tileX, Fixed tileZ);

    // Determine tile type based on position
    TileType getTileType(Fixed x, Fixed z, Fixed altitude);
};

#endif // LANDSCAPE_RENDERER_H
