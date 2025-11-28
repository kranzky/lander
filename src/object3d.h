#ifndef LANDER_OBJECT3D_H
#define LANDER_OBJECT3D_H

#include "fixed.h"
#include "math3d.h"
#include <cstdint>

// =============================================================================
// 3D Object Data Structures
// =============================================================================
//
// Based on the original Lander object blueprint format (Lander.arm lines 12803+).
//
// Each object consists of:
// - Header: vertex count, face count, face offset, flags
// - Vertices: array of (x, y, z) coordinates in 8.24 fixed-point
// - Faces: array of face data including normal, vertex indices, and color
//
// Flags:
//   Bit 0: Object rotates (1) or is static (0)
//   Bit 1: Object has no shadow (1) or has shadow (0)
//
// =============================================================================

// Object flags
namespace ObjectFlags {
    constexpr uint32_t ROTATES = 0x01;      // Object can rotate
    constexpr uint32_t NO_SHADOW = 0x02;    // Object casts no shadow
    constexpr uint32_t HAS_SHADOW = 0x00;   // Object casts shadow (default, NO_SHADOW not set)
}

// A single vertex in model space
struct ObjectVertex {
    int32_t x;  // 8.24 fixed-point
    int32_t y;
    int32_t z;
};

// A single face (triangle) with normal and color
struct ObjectFace {
    // Face normal vector (8.24 fixed-point, used for backface culling/lighting)
    int32_t normalX;
    int32_t normalY;
    int32_t normalZ;

    // Vertex indices (0-based)
    uint8_t vertex0;
    uint8_t vertex1;
    uint8_t vertex2;

    // Color in original 12-bit RGB format (0xRGB)
    // Each component is 4 bits (0-15)
    uint16_t color;
};

// Object blueprint - defines a 3D model
struct ObjectBlueprint {
    uint32_t vertexCount;
    uint32_t faceCount;
    uint32_t flags;
    const ObjectVertex* vertices;
    const ObjectFace* faces;
};

// =============================================================================
// Ship Model Data
// =============================================================================
//
// The player's ship model from Lander.arm lines 12803-12839.
// 9 vertices, 9 faces forming a spaceship shape.
//
// =============================================================================

extern const ObjectBlueprint shipBlueprint;

#endif // LANDER_OBJECT3D_H
