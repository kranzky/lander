// clipping.cpp
// 3D clipping for smooth landscape edge rendering

#include "clipping.h"

namespace ClippingConfig {
    bool enabled = true;  // Enabled by default, toggled with key 4
}

// =============================================================================
// Helper: Interpolate between two vertices
// =============================================================================
// Returns the point on the edge from v1 to v2 at parameter t (0 = v1, 1 = v2)

static ClipVertex3D interpolate(const ClipVertex3D& v1, const ClipVertex3D& v2, Fixed t) {
    ClipVertex3D result;
    // Linear interpolation: result = v1 + t * (v2 - v1)
    result.x = Fixed::fromRaw(v1.x.raw + (int64_t)(v2.x.raw - v1.x.raw) * t.raw / Fixed::ONE);
    result.y = Fixed::fromRaw(v1.y.raw + (int64_t)(v2.y.raw - v1.y.raw) * t.raw / Fixed::ONE);
    result.z = Fixed::fromRaw(v1.z.raw + (int64_t)(v2.z.raw - v1.z.raw) * t.raw / Fixed::ONE);
    return result;
}

// =============================================================================
// Clip polygon against left plane (x = clipX, keep x >= clipX)
// =============================================================================

ClippedPolygon3D clipPolygonLeft(const ClippedPolygon3D& poly, Fixed clipX) {
    ClippedPolygon3D result;
    result.count = 0;

    if (poly.count < 3) return result;

    for (int i = 0; i < poly.count; i++) {
        const ClipVertex3D& current = poly.vertices[i];
        const ClipVertex3D& next = poly.vertices[(i + 1) % poly.count];

        bool currentInside = current.x.raw >= clipX.raw;
        bool nextInside = next.x.raw >= clipX.raw;

        if (currentInside) {
            // Current vertex is inside, add it
            if (result.count < MAX_CLIP_VERTICES_3D) {
                result.vertices[result.count++] = current;
            }

            if (!nextInside) {
                // Edge goes from inside to outside, add intersection
                int32_t dx = next.x.raw - current.x.raw;
                if (dx != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipX.raw - current.x.raw) * Fixed::ONE / dx);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        } else {
            if (nextInside) {
                // Edge goes from outside to inside, add intersection
                int32_t dx = next.x.raw - current.x.raw;
                if (dx != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipX.raw - current.x.raw) * Fixed::ONE / dx);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
            // Current outside, next outside: add nothing
        }
    }

    return result;
}

// =============================================================================
// Clip polygon against right plane (x = clipX, keep x <= clipX)
// =============================================================================

ClippedPolygon3D clipPolygonRight(const ClippedPolygon3D& poly, Fixed clipX) {
    ClippedPolygon3D result;
    result.count = 0;

    if (poly.count < 3) return result;

    for (int i = 0; i < poly.count; i++) {
        const ClipVertex3D& current = poly.vertices[i];
        const ClipVertex3D& next = poly.vertices[(i + 1) % poly.count];

        bool currentInside = current.x.raw <= clipX.raw;
        bool nextInside = next.x.raw <= clipX.raw;

        if (currentInside) {
            if (result.count < MAX_CLIP_VERTICES_3D) {
                result.vertices[result.count++] = current;
            }

            if (!nextInside) {
                int32_t dx = next.x.raw - current.x.raw;
                if (dx != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipX.raw - current.x.raw) * Fixed::ONE / dx);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        } else {
            if (nextInside) {
                int32_t dx = next.x.raw - current.x.raw;
                if (dx != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipX.raw - current.x.raw) * Fixed::ONE / dx);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        }
    }

    return result;
}

// =============================================================================
// Clip polygon against near plane (z = clipZ, keep z >= clipZ)
// =============================================================================

ClippedPolygon3D clipPolygonNear(const ClippedPolygon3D& poly, Fixed clipZ) {
    ClippedPolygon3D result;
    result.count = 0;

    if (poly.count < 3) return result;

    for (int i = 0; i < poly.count; i++) {
        const ClipVertex3D& current = poly.vertices[i];
        const ClipVertex3D& next = poly.vertices[(i + 1) % poly.count];

        bool currentInside = current.z.raw >= clipZ.raw;
        bool nextInside = next.z.raw >= clipZ.raw;

        if (currentInside) {
            if (result.count < MAX_CLIP_VERTICES_3D) {
                result.vertices[result.count++] = current;
            }

            if (!nextInside) {
                int32_t dz = next.z.raw - current.z.raw;
                if (dz != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipZ.raw - current.z.raw) * Fixed::ONE / dz);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        } else {
            if (nextInside) {
                int32_t dz = next.z.raw - current.z.raw;
                if (dz != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipZ.raw - current.z.raw) * Fixed::ONE / dz);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        }
    }

    return result;
}

// =============================================================================
// Clip polygon against far plane (z = clipZ, keep z <= clipZ)
// =============================================================================

ClippedPolygon3D clipPolygonFar(const ClippedPolygon3D& poly, Fixed clipZ) {
    ClippedPolygon3D result;
    result.count = 0;

    if (poly.count < 3) return result;

    for (int i = 0; i < poly.count; i++) {
        const ClipVertex3D& current = poly.vertices[i];
        const ClipVertex3D& next = poly.vertices[(i + 1) % poly.count];

        bool currentInside = current.z.raw <= clipZ.raw;
        bool nextInside = next.z.raw <= clipZ.raw;

        if (currentInside) {
            if (result.count < MAX_CLIP_VERTICES_3D) {
                result.vertices[result.count++] = current;
            }

            if (!nextInside) {
                int32_t dz = next.z.raw - current.z.raw;
                if (dz != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipZ.raw - current.z.raw) * Fixed::ONE / dz);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        } else {
            if (nextInside) {
                int32_t dz = next.z.raw - current.z.raw;
                if (dz != 0) {
                    Fixed t = Fixed::fromRaw((int64_t)(clipZ.raw - current.z.raw) * Fixed::ONE / dz);
                    if (result.count < MAX_CLIP_VERTICES_3D) {
                        result.vertices[result.count++] = interpolate(current, next, t);
                    }
                }
            }
        }
    }

    return result;
}

// =============================================================================
// Convenience functions for clipping quads
// =============================================================================

ClippedPolygon3D clipQuadLeft(const ClipVertex3D quad[4], Fixed clipX) {
    ClippedPolygon3D poly;
    poly.count = 4;
    for (int i = 0; i < 4; i++) {
        poly.vertices[i] = quad[i];
    }
    return clipPolygonLeft(poly, clipX);
}

ClippedPolygon3D clipQuadRight(const ClipVertex3D quad[4], Fixed clipX) {
    ClippedPolygon3D poly;
    poly.count = 4;
    for (int i = 0; i < 4; i++) {
        poly.vertices[i] = quad[i];
    }
    return clipPolygonRight(poly, clipX);
}

ClippedPolygon3D clipQuadNear(const ClipVertex3D quad[4], Fixed clipZ) {
    ClippedPolygon3D poly;
    poly.count = 4;
    for (int i = 0; i < 4; i++) {
        poly.vertices[i] = quad[i];
    }
    return clipPolygonNear(poly, clipZ);
}

ClippedPolygon3D clipQuadFar(const ClipVertex3D quad[4], Fixed clipZ) {
    ClippedPolygon3D poly;
    poly.count = 4;
    for (int i = 0; i < 4; i++) {
        poly.vertices[i] = quad[i];
    }
    return clipPolygonFar(poly, clipZ);
}
