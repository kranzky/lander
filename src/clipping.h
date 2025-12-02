// clipping.h
// 3D clipping for smooth landscape edge rendering

#ifndef CLIPPING_H
#define CLIPPING_H

#include "fixed.h"

// =============================================================================
// Smooth Edge Clipping Configuration
// =============================================================================
//
// When enabled, tiles at the edges of the visible landscape area are clipped
// in 3D world space against the clipping box boundaries. This allows partial
// tiles to be rendered, creating smooth scrolling instead of tiles popping
// in/out of existence.
//
// The clipping box is a rectangular region in world space centered on the
// player, with 4 vertical clipping planes (left, right, near, far).
//
// =============================================================================

namespace ClippingConfig {
    // Toggle for smooth edge clipping (key 4)
    extern bool enabled;
}

// =============================================================================
// 3D Vertex for Clipping
// =============================================================================

struct ClipVertex3D {
    Fixed x, y, z;  // Camera-relative 3D coordinates
};

// =============================================================================
// Clipped Polygon Result
// =============================================================================
// A quad clipped against 1-2 planes can produce 3-6 vertices

constexpr int MAX_CLIP_VERTICES_3D = 8;

struct ClippedPolygon3D {
    ClipVertex3D vertices[MAX_CLIP_VERTICES_3D];
    int count;  // Number of valid vertices (0 = fully clipped)
};

// =============================================================================
// Clipping Functions
// =============================================================================

// Clip a quad against the left clipping plane (x = clipX, keep x >= clipX)
ClippedPolygon3D clipQuadLeft(const ClipVertex3D quad[4], Fixed clipX);

// Clip a quad against the right clipping plane (x = clipX, keep x <= clipX)
ClippedPolygon3D clipQuadRight(const ClipVertex3D quad[4], Fixed clipX);

// Clip a quad against the near clipping plane (z = clipZ, keep z >= clipZ)
ClippedPolygon3D clipQuadNear(const ClipVertex3D quad[4], Fixed clipZ);

// Clip a quad against the far clipping plane (z = clipZ, keep z <= clipZ)
ClippedPolygon3D clipQuadFar(const ClipVertex3D quad[4], Fixed clipZ);

// Clip a polygon against the left clipping plane
ClippedPolygon3D clipPolygonLeft(const ClippedPolygon3D& poly, Fixed clipX);

// Clip a polygon against the right clipping plane
ClippedPolygon3D clipPolygonRight(const ClippedPolygon3D& poly, Fixed clipX);

// Clip a polygon against the near clipping plane
ClippedPolygon3D clipPolygonNear(const ClippedPolygon3D& poly, Fixed clipZ);

// Clip a polygon against the far clipping plane
ClippedPolygon3D clipPolygonFar(const ClippedPolygon3D& poly, Fixed clipZ);

#endif // CLIPPING_H
