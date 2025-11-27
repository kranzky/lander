#include "object3d.h"
#include <cstdio>
#include <cstdlib>

// Test helper
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(condition, name) do { \
    if (condition) { \
        printf("  PASS: %s\n", name); \
        testsPassed++; \
    } else { \
        printf("  FAIL: %s\n", name); \
        testsFailed++; \
    } \
} while(0)

void testShipBlueprint() {
    printf("Ship Blueprint Tests:\n");

    // Verify vertex and face counts match original
    TEST(shipBlueprint.vertexCount == 9, "Ship has 9 vertices");
    TEST(shipBlueprint.faceCount == 9, "Ship has 9 faces");

    // Verify flags (original has 0x03: rotates + has shadow)
    TEST((shipBlueprint.flags & ObjectFlags::ROTATES) != 0, "Ship rotates flag set");

    // Verify pointers are valid
    TEST(shipBlueprint.vertices != nullptr, "Vertices pointer valid");
    TEST(shipBlueprint.faces != nullptr, "Faces pointer valid");
}

void testShipVertices() {
    printf("\nShip Vertex Tests:\n");

    // Check that all vertices have reasonable values (within a few tiles)
    // In 8.24 format, 1 tile = 0x01000000
    constexpr int32_t MAX_COORD = 0x02000000;  // 2 tiles
    constexpr int32_t MIN_COORD = -0x02000000;

    bool allInRange = true;
    for (uint32_t i = 0; i < shipBlueprint.vertexCount; i++) {
        const ObjectVertex& v = shipBlueprint.vertices[i];
        if (v.x < MIN_COORD || v.x > MAX_COORD ||
            v.y < MIN_COORD || v.y > MAX_COORD ||
            v.z < MIN_COORD || v.z > MAX_COORD) {
            allInRange = false;
            printf("    Vertex %u out of range: (%d, %d, %d)\n", i, v.x, v.y, v.z);
        }
    }
    TEST(allInRange, "All vertices within reasonable bounds");

    // Check specific key vertices from original
    // Vertex 5 is the nose (top point) at approximately (-0.1, -0.47, 0)
    const ObjectVertex& nose = shipBlueprint.vertices[5];
    TEST(nose.y < 0, "Nose vertex has negative Y (points up)");
    TEST(nose.z == 0, "Nose vertex centered on Z axis");
}

void testShipFaces() {
    printf("\nShip Face Tests:\n");

    // Check that all face vertex indices are valid
    bool allIndicesValid = true;
    for (uint32_t i = 0; i < shipBlueprint.faceCount; i++) {
        const ObjectFace& f = shipBlueprint.faces[i];
        if (f.vertex0 >= shipBlueprint.vertexCount ||
            f.vertex1 >= shipBlueprint.vertexCount ||
            f.vertex2 >= shipBlueprint.vertexCount) {
            allIndicesValid = false;
            printf("    Face %u has invalid indices: (%u, %u, %u)\n",
                   i, f.vertex0, f.vertex1, f.vertex2);
        }
    }
    TEST(allIndicesValid, "All face vertex indices valid");

    // Check that all faces have non-zero normals (except flat faces)
    int nonZeroNormals = 0;
    for (uint32_t i = 0; i < shipBlueprint.faceCount; i++) {
        const ObjectFace& f = shipBlueprint.faces[i];
        if (f.normalX != 0 || f.normalY != 0 || f.normalZ != 0) {
            nonZeroNormals++;
        }
    }
    TEST(nonZeroNormals == 9, "All faces have normals defined");

    // Check colors are in valid 12-bit range (0x000 to 0xFFF)
    bool allColorsValid = true;
    for (uint32_t i = 0; i < shipBlueprint.faceCount; i++) {
        const ObjectFace& f = shipBlueprint.faces[i];
        if (f.color > 0xFFF) {
            allColorsValid = false;
            printf("    Face %u has invalid color: 0x%03X\n", i, f.color);
        }
    }
    TEST(allColorsValid, "All face colors in valid 12-bit range");

    // Check that cockpit face (face 8) is red
    const ObjectFace& cockpit = shipBlueprint.faces[8];
    uint16_t red = (cockpit.color >> 8) & 0xF;
    uint16_t green = (cockpit.color >> 4) & 0xF;
    uint16_t blue = cockpit.color & 0xF;
    TEST(red > green && red > blue, "Cockpit face is red-tinted (0xC80)");
}

int main() {
    printf("=== Object3D Tests ===\n\n");

    testShipBlueprint();
    testShipVertices();
    testShipFaces();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", testsPassed);
    printf("Failed: %d\n", testsFailed);

    return testsFailed > 0 ? 1 : 0;
}
