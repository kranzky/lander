// landscape_renderer.h
// Renders the 3D landscape terrain using the painter's algorithm

#ifndef LANDSCAPE_RENDERER_H
#define LANDSCAPE_RENDERER_H

#include "screen.h"
#include "projection.h"
#include "landscape.h"
#include "palette.h"
#include "camera.h"
#include "object_map.h"
#include "object3d.h"
#include "object_renderer.h"

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
    // Takes a Camera object for camera position
    void render(ScreenBuffer& screen, const Camera& camera);

    // Render all visible objects on the landscape
    // Called separately from render() so it can be called after particles
    // for proper depth layering
    void renderObjects(ScreenBuffer& screen, const Camera& camera);

private:

    // Storage for projected corner coordinates
    // We need two rows: current and previous
    struct CornerData {
        int screenX;
        int screenY;
        Fixed altitude;
        bool valid;
        // 3D coordinates for clipping (camera-relative)
        Fixed relX, relY, relZ;
    };

    // Extra corners for edge clipping (1 extra on each side)
    static constexpr int MAX_CORNERS = GameConstants::MAX_TILES_X + 2;
    CornerData currentRow[MAX_CORNERS];
    CornerData previousRow[MAX_CORNERS];

    // Project a landscape corner to screen coordinates (world coordinates)
    CornerData projectCorner(Fixed worldX, Fixed worldY, Fixed worldZ,
                             Fixed cameraX, Fixed cameraY, Fixed cameraZ);

    // Project a corner using camera-relative coordinates directly
    CornerData projectCornerRelative(Fixed relX, Fixed relY, Fixed relZ);

    // Edge flags for clipping
    static constexpr int CLIP_NONE = 0;
    static constexpr int CLIP_LEFT = 1;
    static constexpr int CLIP_RIGHT = 2;
    static constexpr int CLIP_NEAR = 4;
    static constexpr int CLIP_FAR = 8;

    // Draw a single tile (quadrilateral) given 4 corners
    // clipFlags indicates which edges to clip against
    // clipLeft/Right/Near/Far are the clipping plane positions
    void drawTile(ScreenBuffer& screen,
                  const CornerData& topLeft, const CornerData& topRight,
                  const CornerData& bottomLeft, const CornerData& bottomRight,
                  int tileRow, Fixed tileX, Fixed tileZ,
                  int clipFlags = CLIP_NONE,
                  Fixed clipLeft = Fixed(), Fixed clipRight = Fixed(),
                  Fixed clipNear = Fixed(), Fixed clipFar = Fixed());

    // Determine tile type based on position
    TileType getTileType(Fixed x, Fixed z, Fixed altitude);
};

#endif // LANDSCAPE_RENDERER_H
