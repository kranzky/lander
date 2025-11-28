#ifndef LANDER_OBJECT_RENDERER_H
#define LANDER_OBJECT_RENDERER_H

#include "object3d.h"
#include "math3d.h"
#include "screen.h"
#include "projection.h"
#include "graphics_buffer.h"

// =============================================================================
// 3D Object Renderer
// =============================================================================
//
// Based on the original DrawObject routine (Lander.arm lines 4979-5640).
//
// The rendering pipeline:
// 1. Transform each vertex by the object's rotation matrix
// 2. Add the object's world position to get final 3D coordinates
// 3. Project each vertex to screen coordinates
// 4. For each face:
//    a. Transform the face normal by the rotation matrix
//    b. Check visibility using dot product (backface culling)
//    c. Calculate lighting based on normal direction
//    d. Draw the triangle with the lit color
//
// =============================================================================

// Maximum vertices per object (ship has 9, other objects have fewer)
constexpr int MAX_VERTICES = 16;

// Projected vertex data
struct ProjectedVertex2D {
    int x;
    int y;
    bool visible;
};

// Draw a 3D object
// Arguments:
//   blueprint - The object's model data
//   position  - Object position relative to camera (in world space)
//   rotation  - Object's rotation matrix (identity for static objects)
//   screen    - Screen buffer to draw to
void drawObject(
    const ObjectBlueprint& blueprint,
    const Vec3& position,
    const Mat3x3& rotation,
    ScreenBuffer& screen
);

// Calculate lit color from base color and face normal
// The light source is from above and slightly to the left
// Returns a Color struct
Color calculateLitColor(uint16_t baseColor, const Vec3& rotatedNormal);

// Draw a 3D object's shadow
// Projects vertices onto the terrain and draws black triangles for upward-facing faces
// Based on DrawObject Part 4 (Lander.arm lines 5385-5465)
//
// Arguments:
//   blueprint       - The object's model data
//   cameraRelPos    - Object position relative to camera (for screen projection)
//   rotation        - Object's rotation matrix
//   worldPos        - Object's actual world position (for terrain lookup)
//   cameraWorldPos  - Camera's world position (for terrain lookup)
//   screen          - Screen buffer to draw to
void drawObjectShadow(
    const ObjectBlueprint& blueprint,
    const Vec3& cameraRelPos,
    const Mat3x3& rotation,
    const Vec3& worldPos,
    const Vec3& cameraWorldPos,
    ScreenBuffer& screen
);

// =============================================================================
// Buffered Object Rendering (for depth-sorted drawing)
// =============================================================================
//
// These versions buffer triangles to a graphics buffer instead of drawing
// immediately. Used for proper depth sorting with the landscape.
//
// =============================================================================

// Draw a 3D object to a graphics buffer (deferred rendering)
// Arguments:
//   blueprint - The object's model data
//   position  - Object position relative to camera (in world space)
//   rotation  - Object's rotation matrix (identity for static objects)
//   row       - The tile row index (0 = back, TILES_Z-1 = front)
void bufferObject(
    const ObjectBlueprint& blueprint,
    const Vec3& position,
    const Mat3x3& rotation,
    int row
);

// Draw a 3D object's shadow to a graphics buffer (deferred rendering)
void bufferObjectShadow(
    const ObjectBlueprint& blueprint,
    const Vec3& cameraRelPos,
    const Mat3x3& rotation,
    const Vec3& worldPos,
    const Vec3& cameraWorldPos,
    int row
);

#endif // LANDER_OBJECT_RENDERER_H
