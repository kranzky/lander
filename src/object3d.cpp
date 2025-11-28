#include "object3d.h"
#include "object_map.h"  // For ObjectType constants

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

// =============================================================================
// Pyramid Model Data (unused in game, but defined for completeness)
// =============================================================================
// From Lander.arm lines 12763-12791

static const ObjectVertex pyramidVertices[] = {
    { 0x00000000, 0x01000000, 0x00000000 },  // Vertex 0: Top point
    { 0x00C00000, static_cast<int32_t>(0xFF800000), 0x00C00000 },  // Vertex 1
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000), 0x00C00000 },  // Vertex 2
    { 0x00C00000, static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFF400000) },  // Vertex 3
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFF400000) },  // Vertex 4
};

static const ObjectFace pyramidFaces[] = {
    { 0x00000000, 0x35AA66D2, 0x6B54CDA5, 0, 1, 2, 0x800 },  // Face 0
    { 0x6B54CDA5, 0x35AA66D2, 0x00000000, 0, 3, 1, 0x088 },  // Face 1
    { 0x00000000, 0x35AA66D2, static_cast<int32_t>(0x94AB325B), 0, 4, 3, 0x880 },  // Face 2
    { static_cast<int32_t>(0x94AB325B), 0x35AA66D2, 0x00000000, 0, 2, 4, 0x808 },  // Face 3
    { 0x00000000, static_cast<int32_t>(0x88000000), 0x00000000, 1, 2, 3, 0x444 },  // Face 4
    { 0x00000000, static_cast<int32_t>(0x88000000), 0x00000000, 2, 3, 4, 0x008 },  // Face 5
};

const ObjectBlueprint pyramidBlueprint = {
    5, 6,
    ObjectFlags::ROTATES,  // Rotates, no shadow
    pyramidVertices,
    pyramidFaces,
};

// =============================================================================
// Small Leafy Tree Model Data
// =============================================================================
// From Lander.arm lines 12851-12884

static const ObjectVertex smallLeafyTreeVertices[] = {
    { 0x00300000, static_cast<int32_t>(0xFE800000), 0x00300000 },  // Vertex 0
    { static_cast<int32_t>(0xFFD9999A), 0x00000000, 0x00000000 },  // Vertex 1: Trunk base left
    { 0x00266666, 0x00000000, 0x00000000 },  // Vertex 2: Trunk base right
    { 0x00000000, static_cast<int32_t>(0xFEF33334), static_cast<int32_t>(0xFF400000) },  // Vertex 3
    { 0x00800000, static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000) },  // Vertex 4
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFECCCCCD), static_cast<int32_t>(0xFFD55556) },  // Vertex 5
    { static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFEA66667), 0x00400000 },  // Vertex 6
    { 0x00800000, static_cast<int32_t>(0xFE59999A), 0x002AAAAA },  // Vertex 7
    { 0x00C00000, static_cast<int32_t>(0xFEA66667), static_cast<int32_t>(0xFFC00000) },  // Vertex 8
    { static_cast<int32_t>(0xFFA00000), static_cast<int32_t>(0xFECCCCCD), 0x00999999 },  // Vertex 9
    { 0x00C00000, static_cast<int32_t>(0xFF400000), 0x00C00000 },  // Vertex 10
};

static const ObjectFace smallLeafyTreeFaces[] = {
    { 0x14A01873, static_cast<int32_t>(0xAF8F9F93), 0x56A0681E, 0, 9, 10, 0x040 },  // Face 0: Leaf
    { 0x00000000, 0x00000000, 0x00000000, 0, 1, 2, 0x400 },  // Face 1: Trunk
    { 0x499A254E, static_cast<int32_t>(0xB123FC2C), static_cast<int32_t>(0xCB6D5299), 0, 3, 4, 0x080 },  // Face 2: Leaf
    { static_cast<int32_t>(0xE4D2EEBE), static_cast<int32_t>(0x8DC82837), static_cast<int32_t>(0xE72FE5E9), 0, 5, 6, 0x080 },  // Face 3: Leaf
    { static_cast<int32_t>(0xD5710585), static_cast<int32_t>(0xB29EF364), static_cast<int32_t>(0xAEC07EB3), 0, 7, 8, 0x080 },  // Face 4: Leaf
};

const ObjectBlueprint smallLeafyTreeBlueprint = {
    11, 5,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow (flags = 2 = bit 1 set? No, bit 1 = no shadow)
    smallLeafyTreeVertices,
    smallLeafyTreeFaces,
};

// =============================================================================
// Tall Leafy Tree Model Data
// =============================================================================
// From Lander.arm lines 12896-12933

static const ObjectVertex tallLeafyTreeVertices[] = {
    { 0x0036DB6D, static_cast<int32_t>(0xFD733334), 0x00300000 },  // Vertex 0: Top
    { static_cast<int32_t>(0xFFD00000), 0x00000000, 0x00000000 },  // Vertex 1: Trunk base left
    { 0x00300000, 0x00000000, 0x00000000 },  // Vertex 2: Trunk base right
    { 0x00000000, static_cast<int32_t>(0xFE0CCCCD), static_cast<int32_t>(0xFF400000) },  // Vertex 3
    { 0x00800000, static_cast<int32_t>(0xFE59999A), static_cast<int32_t>(0xFF800000) },  // Vertex 4
    { static_cast<int32_t>(0xFF533334), static_cast<int32_t>(0xFE333334), static_cast<int32_t>(0xFFC92493) },  // Vertex 5
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFEA66667), 0x00600000 },  // Vertex 6
    { 0x00000000, static_cast<int32_t>(0xFF19999A), static_cast<int32_t>(0xFF666667) },  // Vertex 7
    { static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFFA00000) },  // Vertex 8
    { static_cast<int32_t>(0xFFA00000), static_cast<int32_t>(0xFE800000), 0x00999999 },  // Vertex 9
    { 0x00C00000, static_cast<int32_t>(0xFECCCCCD), 0x00C00000 },  // Vertex 10
    { static_cast<int32_t>(0xFFB33334), static_cast<int32_t>(0xFF19999A), 0x00E66666 },  // Vertex 11
    { 0x00800000, static_cast<int32_t>(0xFF400000), 0x00C00000 },  // Vertex 12
    { 0x00300000, static_cast<int32_t>(0xFE59999A), 0x00300000 },  // Vertex 13
};

static const ObjectFace tallLeafyTreeFaces[] = {
    { static_cast<int32_t>(0xFD3D01DD), static_cast<int32_t>(0xD2CB371E), 0x6F20024E, 0, 9, 10, 0x040 },  // Face 0
    { 0x1E6F981A, static_cast<int32_t>(0xBB105ECE), 0x5D638B16, 13, 11, 12, 0x080 },  // Face 1
    { 0x00000000, 0x00000000, 0x00000000, 0, 1, 2, 0x400 },  // Face 2: Trunk
    { 0x49D96509, static_cast<int32_t>(0xB8E72762), static_cast<int32_t>(0xC19E3A19), 0, 3, 4, 0x080 },  // Face 3
    { static_cast<int32_t>(0xAD213B74), static_cast<int32_t>(0xB641CA5D), 0x2DC40650, 0, 5, 6, 0x040 },  // Face 4
    { static_cast<int32_t>(0xC9102051), static_cast<int32_t>(0xAC846CAD), static_cast<int32_t>(0xBD92A8C1), 13, 7, 8, 0x040 },  // Face 5
};

const ObjectBlueprint tallLeafyTreeBlueprint = {
    14, 6,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow
    tallLeafyTreeVertices,
    tallLeafyTreeFaces,
};

// =============================================================================
// Fir Tree Model Data
// =============================================================================
// From Lander.arm lines 13017-13041

static const ObjectVertex firTreeVertices[] = {
    { static_cast<int32_t>(0xFFA00000), static_cast<int32_t>(0xFFC92493), static_cast<int32_t>(0xFFC92493) },  // Vertex 0
    { 0x00600000, static_cast<int32_t>(0xFFC92493), static_cast<int32_t>(0xFFC92493) },  // Vertex 1
    { 0x00000000, static_cast<int32_t>(0xFE333334), 0x0036DB6D },  // Vertex 2: Top
    { 0x00266666, 0x00000000, 0x00000000 },  // Vertex 3: Trunk base right
    { static_cast<int32_t>(0xFFD9999A), 0x00000000, 0x00000000 },  // Vertex 4: Trunk base left
};

static const ObjectFace firTreeFaces[] = {
    { 0x00000000, 0x00000000, 0x00000000, 2, 3, 4, 0x400 },  // Face 0: Trunk
    { 0x00000000, static_cast<int32_t>(0xE0B0E050), static_cast<int32_t>(0x8C280943), 0, 1, 2, 0x040 },  // Face 1: Canopy
};

const ObjectBlueprint firTreeBlueprint = {
    5, 2,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow
    firTreeVertices,
    firTreeFaces,
};

// =============================================================================
// Gazebo Model Data
// =============================================================================
// From Lander.arm lines 13053-13091

static const ObjectVertex gazeboVertices[] = {
    { 0x00000000, static_cast<int32_t>(0xFF000000), 0x00000000 },  // Vertex 0: Roof peak
    { static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFF400000), 0x00800000 },  // Vertex 1
    { static_cast<int32_t>(0xFF800000), static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000) },  // Vertex 2
    { 0x00800000, static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000) },  // Vertex 3
    { 0x00800000, static_cast<int32_t>(0xFF400000), 0x00800000 },  // Vertex 4
    { static_cast<int32_t>(0xFF800000), 0x00000000, 0x00800000 },  // Vertex 5
    { static_cast<int32_t>(0xFF800000), 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 6
    { 0x00800000, 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 7
    { 0x00800000, 0x00000000, 0x00800000 },  // Vertex 8
    { static_cast<int32_t>(0xFF99999A), static_cast<int32_t>(0xFF400000), 0x00800000 },  // Vertex 9: Pillar top
    { static_cast<int32_t>(0xFF99999A), static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000) },  // Vertex 10
    { 0x00666666, static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF800000) },  // Vertex 11
    { 0x00666666, static_cast<int32_t>(0xFF400000), 0x00800000 },  // Vertex 12
};

static const ObjectFace gazeboFaces[] = {
    { 0x00000000, 0x00000000, 0x78000000, 1, 5, 9, 0x444 },  // Face 0: Pillar
    { 0x00000000, 0x00000000, static_cast<int32_t>(0x88000000), 2, 6, 10, 0x444 },  // Face 1: Pillar
    { 0x00000000, static_cast<int32_t>(0x94AB325B), 0x35AA66D2, 0, 1, 4, 0x400 },  // Face 2: Roof front
    { 0x00000000, 0x00000000, static_cast<int32_t>(0x88000000), 3, 7, 11, 0x444 },  // Face 3: Pillar
    { 0x00000000, 0x00000000, 0x78000000, 4, 8, 12, 0x444 },  // Face 4: Pillar
    { static_cast<int32_t>(0xCA55992E), static_cast<int32_t>(0x94AB325B), 0x00000000, 0, 1, 2, 0x840 },  // Face 5: Roof left
    { 0x35AA66D2, static_cast<int32_t>(0x94AB325B), 0x00000000, 0, 3, 4, 0x840 },  // Face 6: Roof right
    { 0x00000000, static_cast<int32_t>(0x94AB325B), static_cast<int32_t>(0xCA55992E), 0, 2, 3, 0x400 },  // Face 7: Roof back
};

const ObjectBlueprint gazeboBlueprint = {
    13, 8,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow
    gazeboVertices,
    gazeboFaces,
};

// =============================================================================
// Building Model Data
// =============================================================================
// From Lander.arm lines 13103-13148

static const ObjectVertex buildingVertices[] = {
    { static_cast<int32_t>(0xFF19999A), static_cast<int32_t>(0xFF266667), 0x00000000 },  // Vertex 0
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF266667), 0x00000000 },  // Vertex 1
    { 0x00C00000, static_cast<int32_t>(0xFF266667), 0x00000000 },  // Vertex 2
    { 0x00E66666, static_cast<int32_t>(0xFF266667), 0x00000000 },  // Vertex 3
    { static_cast<int32_t>(0xFF19999A), static_cast<int32_t>(0xFF8CCCCD), 0x00A66666 },  // Vertex 4
    { static_cast<int32_t>(0xFF19999A), static_cast<int32_t>(0xFF8CCCCD), static_cast<int32_t>(0xFF59999A) },  // Vertex 5
    { 0x00E66666, static_cast<int32_t>(0xFF8CCCCD), 0x00A66666 },  // Vertex 6
    { 0x00E66666, static_cast<int32_t>(0xFF8CCCCD), static_cast<int32_t>(0xFF59999A) },  // Vertex 7
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF666667), 0x00800000 },  // Vertex 8
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF666667), static_cast<int32_t>(0xFF800000) },  // Vertex 9
    { 0x00C00000, static_cast<int32_t>(0xFF666667), 0x00800000 },  // Vertex 10
    { 0x00C00000, static_cast<int32_t>(0xFF666667), static_cast<int32_t>(0xFF800000) },  // Vertex 11
    { static_cast<int32_t>(0xFF400000), 0x00000000, 0x00800000 },  // Vertex 12
    { static_cast<int32_t>(0xFF400000), 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 13
    { 0x00C00000, 0x00000000, 0x00800000 },  // Vertex 14
    { 0x00C00000, 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 15
};

static const ObjectFace buildingFaces[] = {
    { 0x00000000, static_cast<int32_t>(0x99CD0E6D), 0x3EE445CC, 0, 4, 6, 0x400 },  // Face 0: Roof front
    { 0x00000000, static_cast<int32_t>(0x99CD0E6D), 0x3EE445CC, 0, 3, 6, 0x400 },  // Face 1: Roof front
    { static_cast<int32_t>(0x88000000), 0x00000000, 0x00000000, 1, 8, 9, 0xDDD },  // Face 2: Wall left
    { 0x78000000, 0x00000000, 0x00000000, 2, 10, 11, 0x555 },  // Face 3: Wall right
    { static_cast<int32_t>(0x88000000), 0x00000000, 0x00000000, 8, 12, 13, 0xFFF },  // Face 4: Wall left lower
    { static_cast<int32_t>(0x88000000), 0x00000000, 0x00000000, 8, 9, 13, 0xFFF },  // Face 5: Wall left lower
    { 0x78000000, 0x00000000, 0x00000000, 10, 14, 15, 0x777 },  // Face 6: Wall right lower
    { 0x78000000, 0x00000000, 0x00000000, 10, 11, 15, 0x777 },  // Face 7: Wall right lower
    { 0x00000000, 0x00000000, static_cast<int32_t>(0x88000000), 9, 13, 15, 0xBBB },  // Face 8: Wall back
    { 0x00000000, 0x00000000, static_cast<int32_t>(0x88000000), 9, 11, 15, 0xBBB },  // Face 9: Wall back
    { 0x00000000, static_cast<int32_t>(0x99CD0E6D), static_cast<int32_t>(0xC11BBA34), 0, 5, 7, 0x800 },  // Face 10: Roof back
    { 0x00000000, static_cast<int32_t>(0x99CD0E6D), static_cast<int32_t>(0xC11BBA34), 0, 3, 7, 0x800 },  // Face 11: Roof back
};

const ObjectBlueprint buildingBlueprint = {
    16, 12,
    ObjectFlags::HAS_SHADOW,  // Static, no shadow (original has no shadow)
    buildingVertices,
    buildingFaces,
};

// =============================================================================
// Rocket Model Data
// =============================================================================
// From Lander.arm lines 13240-13278

static const ObjectVertex rocketVertices[] = {
    { 0x00000000, static_cast<int32_t>(0xFE400000), 0x00000000 },  // Vertex 0: Nose tip
    { static_cast<int32_t>(0xFFC80000), static_cast<int32_t>(0xFFD745D2), 0x00380000 },  // Vertex 1
    { static_cast<int32_t>(0xFFC80000), static_cast<int32_t>(0xFFD745D2), static_cast<int32_t>(0xFFC80000) },  // Vertex 2
    { 0x00380000, static_cast<int32_t>(0xFFD745D2), 0x00380000 },  // Vertex 3
    { 0x00380000, static_cast<int32_t>(0xFFD745D2), static_cast<int32_t>(0xFFC80000) },  // Vertex 4
    { static_cast<int32_t>(0xFF900000), 0x00000000, 0x00700000 },  // Vertex 5: Fin base
    { static_cast<int32_t>(0xFF900000), 0x00000000, static_cast<int32_t>(0xFF900000) },  // Vertex 6
    { 0x00700000, 0x00000000, 0x00700000 },  // Vertex 7
    { 0x00700000, 0x00000000, static_cast<int32_t>(0xFF900000) },  // Vertex 8
    { static_cast<int32_t>(0xFFE40000), static_cast<int32_t>(0xFF071C72), 0x001C0000 },  // Vertex 9: Mid body
    { static_cast<int32_t>(0xFFE40000), static_cast<int32_t>(0xFF071C72), static_cast<int32_t>(0xFFE40000) },  // Vertex 10
    { 0x001C0000, static_cast<int32_t>(0xFF071C72), 0x001C0000 },  // Vertex 11
    { 0x001C0000, static_cast<int32_t>(0xFF071C72), static_cast<int32_t>(0xFFE40000) },  // Vertex 12
};

static const ObjectFace rocketFaces[] = {
    { 0x00000000, 0x00000000, 0x00000000, 9, 1, 5, 0xCC0 },  // Face 0: Fin
    { 0x00000000, 0x00000000, 0x00000000, 11, 3, 7, 0xCC0 },  // Face 1: Fin
    { 0x00000000, static_cast<int32_t>(0xEFA75F67), 0x76E1A76B, 0, 1, 3, 0xC00 },  // Face 2: Nose front
    { static_cast<int32_t>(0x891E5895), static_cast<int32_t>(0xEFA75F67), 0x00000000, 0, 1, 2, 0x800 },  // Face 3: Nose left
    { 0x76E1A76B, static_cast<int32_t>(0xEFA75F67), 0x00000000, 3, 0, 4, 0x800 },  // Face 4: Nose right
    { 0x00000000, static_cast<int32_t>(0xEFA75F67), static_cast<int32_t>(0x891E5895), 0, 2, 4, 0xC00 },  // Face 5: Nose back
    { 0x00000000, 0x00000000, 0x00000000, 10, 2, 6, 0xCC0 },  // Face 6: Fin
    { 0x00000000, 0x00000000, 0x00000000, 12, 4, 8, 0xCC0 },  // Face 7: Fin
};

const ObjectBlueprint rocketBlueprint = {
    13, 8,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow
    rocketVertices,
    rocketFaces,
};

// =============================================================================
// Smoking Remains (Left Bend) Model Data
// =============================================================================
// From Lander.arm lines 12945-12969

static const ObjectVertex smokingRemainsLeftVertices[] = {
    { static_cast<int32_t>(0xFFD9999A), 0x00000000, 0x00000000 },  // Vertex 0
    { 0x00266666, 0x00000000, 0x00000000 },  // Vertex 1
    { 0x002B3333, static_cast<int32_t>(0xFFC00000), 0x00000000 },  // Vertex 2
    { 0x00300000, static_cast<int32_t>(0xFF800000), 0x00000000 },  // Vertex 3
    { static_cast<int32_t>(0xFFD55556), static_cast<int32_t>(0xFECCCCCD), 0x00000000 },  // Vertex 4: Bent tip
};

static const ObjectFace smokingRemainsLeftFaces[] = {
    { 0x00000000, 0x00000000, 0x00000000, 0, 1, 3, 0x000 },  // Face 0: Black
    { 0x00000000, 0x00000000, 0x00000000, 2, 3, 4, 0x000 },  // Face 1: Black
};

const ObjectBlueprint smokingRemainsLeftBlueprint = {
    5, 2,
    0,  // No shadow, static
    smokingRemainsLeftVertices,
    smokingRemainsLeftFaces,
};

// =============================================================================
// Smoking Remains (Right Bend) Model Data
// =============================================================================
// From Lander.arm lines 12981-13005

static const ObjectVertex smokingRemainsRightVertices[] = {
    { 0x002AAAAA, 0x00000000, 0x00000000 },  // Vertex 0
    { static_cast<int32_t>(0xFFD55556), 0x00000000, 0x00000000 },  // Vertex 1
    { static_cast<int32_t>(0xFFD4CCCD), static_cast<int32_t>(0xFFD00000), 0x00000000 },  // Vertex 2
    { static_cast<int32_t>(0xFFD00000), static_cast<int32_t>(0xFFA00000), 0x00000000 },  // Vertex 3
    { 0x002AAAAA, static_cast<int32_t>(0xFEA66667), 0x00000000 },  // Vertex 4: Bent tip
};

static const ObjectFace smokingRemainsRightFaces[] = {
    { 0x00000000, 0x00000000, 0x00000000, 0, 1, 3, 0x000 },  // Face 0: Black
    { 0x00000000, 0x00000000, 0x00000000, 2, 3, 4, 0x000 },  // Face 1: Black
};

const ObjectBlueprint smokingRemainsRightBlueprint = {
    5, 2,
    0,  // No shadow, static
    smokingRemainsRightVertices,
    smokingRemainsRightFaces,
};

// =============================================================================
// Smoking Gazebo Model Data
// =============================================================================
// From Lander.arm lines 13201-13228

static const ObjectVertex smokingGazeboVertices[] = {
    { 0x00000000, static_cast<int32_t>(0xFF8CCCCD), static_cast<int32_t>(0xFFF00000) },  // Vertex 0
    { 0x00199999, static_cast<int32_t>(0xFF8CCCCD), static_cast<int32_t>(0xFFF00000) },  // Vertex 1
    { 0x00800000, 0x00000000, 0x00800000 },  // Vertex 2
    { static_cast<int32_t>(0xFF800000), 0x00000000, 0x00800000 },  // Vertex 3
    { 0x00800000, 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 4
    { static_cast<int32_t>(0xFF800000), 0x00000000, static_cast<int32_t>(0xFF800000) },  // Vertex 5
};

static const ObjectFace smokingGazeboFaces[] = {
    { 0x00000000, static_cast<int32_t>(0xA24BB5BE), 0x4AF6A1AD, 0, 1, 2, 0x000 },  // Face 0: Black
    { 0x00000000, static_cast<int32_t>(0xA24BB5BE), 0x4AF6A1AD, 0, 1, 3, 0x333 },  // Face 1: Dark grey
    { 0x00000000, static_cast<int32_t>(0xAC59C060), static_cast<int32_t>(0xA9F5EA98), 0, 1, 4, 0x444 },  // Face 2: Grey
    { 0x00000000, static_cast<int32_t>(0xAC59C060), static_cast<int32_t>(0xA9F5EA98), 0, 1, 5, 0x000 },  // Face 3: Black
};

const ObjectBlueprint smokingGazeboBlueprint = {
    6, 4,
    ObjectFlags::HAS_SHADOW,  // Static, has shadow
    smokingGazeboVertices,
    smokingGazeboFaces,
};

// =============================================================================
// Smoking Building Model Data
// =============================================================================
// From Lander.arm lines 13160-13189

static const ObjectVertex smokingBuildingVertices[] = {
    { static_cast<int32_t>(0xFF400000), 0x00000001, 0x00800000 },  // Vertex 0
    { static_cast<int32_t>(0xFF400000), 0x00000001, static_cast<int32_t>(0xFF800000) },  // Vertex 1
    { 0x00C00000, 0x00000001, 0x00800000 },  // Vertex 2
    { 0x00C00000, 0x00000001, static_cast<int32_t>(0xFF800000) },  // Vertex 3
    { static_cast<int32_t>(0xFF400000), static_cast<int32_t>(0xFF99999A), 0x00800000 },  // Vertex 4
    { 0x00C00000, static_cast<int32_t>(0xFFB33334), static_cast<int32_t>(0xFF800000) },  // Vertex 5
};

static const ObjectFace smokingBuildingFaces[] = {
    { 0x00000000, 0x78000000, 0x00000000, 0, 1, 2, 0x000 },  // Face 0: Floor black
    { 0x00000000, 0x78000000, 0x00000000, 1, 2, 3, 0x000 },  // Face 1: Floor black
    { 0x00000000, 0x00000000, 0x78000000, 0, 2, 4, 0x333 },  // Face 2: Wall grey
    { static_cast<int32_t>(0x88000000), 0x00000000, 0x00000000, 0, 1, 4, 0x666 },  // Face 3: Wall grey
    { 0x78000000, 0x00000000, 0x00000000, 2, 3, 5, 0x555 },  // Face 4: Wall grey
    { 0x00000000, 0x00000000, static_cast<int32_t>(0x88000001), 1, 3, 5, 0x777 },  // Face 5: Wall grey
};

const ObjectBlueprint smokingBuildingBlueprint = {
    6, 6,
    0,  // No shadow, static
    smokingBuildingVertices,
    smokingBuildingFaces,
};

// =============================================================================
// Object Type to Blueprint Mapping
// =============================================================================

const ObjectBlueprint* getObjectBlueprint(uint8_t objectType) {
    // Map object type to blueprint based on objectTypes table
    // From Lander.arm lines 4638-4666
    switch (objectType) {
        case ObjectType::PYRAMID:
            return &pyramidBlueprint;

        case ObjectType::SMALL_LEAFY_TREE:
        case ObjectType::SMALL_LEAFY_TREE_2:
        case ObjectType::SMALL_LEAFY_TREE_3:
            return &smallLeafyTreeBlueprint;

        case ObjectType::TALL_LEAFY_TREE:
        case ObjectType::TALL_LEAFY_TREE_2:
            return &tallLeafyTreeBlueprint;

        case ObjectType::GAZEBO:
            return &gazeboBlueprint;

        case ObjectType::FIR_TREE:
            return &firTreeBlueprint;

        case ObjectType::BUILDING:
            return &buildingBlueprint;

        case ObjectType::ROCKET:
        case ObjectType::ROCKET_2:
        case ObjectType::ROCKET_3:
        case ObjectType::SMOKING_ROCKET:
            return &rocketBlueprint;

        case ObjectType::SMOKING_REMAINS_R:
        case ObjectType::SMOKING_REMAINS_R2:
        case ObjectType::SMOKING_REMAINS_R3:
        case ObjectType::SMOKING_REMAINS_R4:
            return &smokingRemainsRightBlueprint;

        case ObjectType::SMOKING_REMAINS_L:
        case ObjectType::SMOKING_REMAINS_L2:
        case ObjectType::SMOKING_REMAINS_L3:
        case ObjectType::SMOKING_REMAINS_L4:
        case ObjectType::SMOKING_REMAINS_L5:
            return &smokingRemainsLeftBlueprint;

        case ObjectType::SMOKING_GAZEBO:
            return &smokingGazeboBlueprint;

        case ObjectType::SMOKING_BUILDING:
            return &smokingBuildingBlueprint;

        default:
            return nullptr;  // Unknown or NONE type
    }
}
