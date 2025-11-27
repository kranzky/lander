#include "object3d.h"

// =============================================================================
// Ship Model Data
// =============================================================================
//
// Direct port of objectPlayer from Lander.arm lines 12803-12839.
//
// The ship is a triangular craft with:
// - A pointed nose (vertex 5, at negative Y = top)
// - Wing tips on either side
// - Landing legs at the bottom
// - A small cockpit detail (vertices 6, 7, 8)
//
// Coordinates are in 8.24 fixed-point format.
// Y axis is inverted: negative Y = up, positive Y = down.
//
// =============================================================================

// Ship vertices (9 vertices)
// Original: Lander.arm lines 12818-12826
static const ObjectVertex shipVertices[] = {
    // Vertex 0: Right wing front
    { 0x01000000, 0x00500000, 0x00800000 },

    // Vertex 1: Right wing back
    { 0x01000000, 0x00500000, static_cast<int32_t>(0xFF800000) },

    // Vertex 2: Back center top
    { 0x00000000, 0x000A0000, static_cast<int32_t>(0xFECCCCCD) },

    // Vertex 3: Left wing center
    { static_cast<int32_t>(0xFF19999A), 0x00500000, 0x00000000 },

    // Vertex 4: Front center top
    { 0x00000000, 0x000A0000, 0x01333333 },

    // Vertex 5: Nose (top point)
    { static_cast<int32_t>(0xFFE66667), static_cast<int32_t>(0xFF880000), 0x00000000 },

    // Vertex 6: Cockpit right front
    { 0x00555555, 0x00500000, 0x00400000 },

    // Vertex 7: Cockpit right back
    { 0x00555555, 0x00500000, static_cast<int32_t>(0xFFC00000) },

    // Vertex 8: Cockpit left
    { static_cast<int32_t>(0xFFCCCCCD), 0x00500000, 0x00000000 },
};

// Ship faces (9 triangular faces)
// Original: Lander.arm lines 12831-12839
// Format: normalX, normalY, normalZ, vertex0, vertex1, vertex2, color
static const ObjectFace shipFaces[] = {
    // Face 0: Right side panel
    { 0x457C441A, static_cast<int32_t>(0x9E2A1F4C), 0x00000000, 0, 1, 5, 0x080 },

    // Face 1: Right back panel
    { 0x35F5D83B, static_cast<int32_t>(0x9BC03EC1), static_cast<int32_t>(0xDA12D71D), 1, 2, 5, 0x040 },

    // Face 2: Right front panel
    { 0x35F5D83B, static_cast<int32_t>(0x9BC03EC1), 0x25ED28E3, 0, 5, 4, 0x040 },

    // Face 3: Left back panel
    { static_cast<int32_t>(0xB123D51C), static_cast<int32_t>(0xAF3F50EE), static_cast<int32_t>(0xD7417278), 2, 3, 5, 0x040 },

    // Face 4: Left front panel
    { static_cast<int32_t>(0xB123D51D), static_cast<int32_t>(0xAF3F50EE), 0x28BE8D88, 3, 4, 5, 0x040 },

    // Face 5: Bottom back panel
    { static_cast<int32_t>(0xF765D8CD), 0x73242236, static_cast<int32_t>(0xDF4FD176), 1, 2, 3, 0x088 },

    // Face 6: Bottom front panel
    { static_cast<int32_t>(0xF765D8CD), 0x73242236, 0x20B02E8A, 0, 3, 4, 0x088 },

    // Face 7: Wing connection
    { 0x00000000, 0x78000000, 0x00000000, 0, 1, 3, 0x044 },

    // Face 8: Cockpit detail (red)
    { 0x00000000, 0x78000000, 0x00000000, 6, 7, 8, 0xC80 },
};

// Ship blueprint
const ObjectBlueprint shipBlueprint = {
    9,                      // vertexCount
    9,                      // faceCount
    ObjectFlags::ROTATES,   // flags: rotates, has shadow
    shipVertices,
    shipFaces,
};
