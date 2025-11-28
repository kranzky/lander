#include "object3d.h"
#include "object_map.h"
#include <cstdio>
#include <cstdlib>
#include <string>

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

// =============================================================================
// Landscape Object Blueprint Tests
// =============================================================================

// Helper to validate a blueprint
void validateBlueprint(const ObjectBlueprint& bp, const char* name,
                       uint32_t expectedVerts, uint32_t expectedFaces) {
    printf("\n%s Blueprint Tests:\n", name);

    TEST(bp.vertexCount == expectedVerts,
         (std::string(name) + " has correct vertex count").c_str());
    TEST(bp.faceCount == expectedFaces,
         (std::string(name) + " has correct face count").c_str());
    TEST(bp.vertices != nullptr,
         (std::string(name) + " has valid vertex pointer").c_str());
    TEST(bp.faces != nullptr,
         (std::string(name) + " has valid face pointer").c_str());

    // Validate all face vertex indices
    bool allIndicesValid = true;
    for (uint32_t i = 0; i < bp.faceCount; i++) {
        const ObjectFace& f = bp.faces[i];
        if (f.vertex0 >= bp.vertexCount ||
            f.vertex1 >= bp.vertexCount ||
            f.vertex2 >= bp.vertexCount) {
            allIndicesValid = false;
        }
    }
    TEST(allIndicesValid,
         (std::string(name) + " all face indices valid").c_str());

    // Validate colors are in 12-bit range
    bool allColorsValid = true;
    for (uint32_t i = 0; i < bp.faceCount; i++) {
        if (bp.faces[i].color > 0xFFF) {
            allColorsValid = false;
        }
    }
    TEST(allColorsValid,
         (std::string(name) + " colors in valid range").c_str());
}

void testLandscapeObjects() {
    printf("\n=== Landscape Object Blueprints ===\n");

    // Test each blueprint with expected counts from original source
    validateBlueprint(pyramidBlueprint, "Pyramid", 5, 6);
    validateBlueprint(smallLeafyTreeBlueprint, "Small Leafy Tree", 11, 5);
    validateBlueprint(tallLeafyTreeBlueprint, "Tall Leafy Tree", 14, 6);
    validateBlueprint(firTreeBlueprint, "Fir Tree", 5, 2);
    validateBlueprint(gazeboBlueprint, "Gazebo", 13, 8);
    validateBlueprint(buildingBlueprint, "Building", 16, 12);
    validateBlueprint(rocketBlueprint, "Rocket", 13, 8);
    validateBlueprint(smokingRemainsLeftBlueprint, "Smoking Remains Left", 5, 2);
    validateBlueprint(smokingRemainsRightBlueprint, "Smoking Remains Right", 5, 2);
    validateBlueprint(smokingGazeboBlueprint, "Smoking Gazebo", 6, 4);
    validateBlueprint(smokingBuildingBlueprint, "Smoking Building", 6, 6);
}

void testGetObjectBlueprint() {
    printf("\n=== getObjectBlueprint() Tests ===\n");

    // Test normal object types
    TEST(getObjectBlueprint(ObjectType::PYRAMID) == &pyramidBlueprint,
         "PYRAMID returns pyramid blueprint");
    TEST(getObjectBlueprint(ObjectType::SMALL_LEAFY_TREE) == &smallLeafyTreeBlueprint,
         "SMALL_LEAFY_TREE returns small leafy tree blueprint");
    TEST(getObjectBlueprint(ObjectType::SMALL_LEAFY_TREE_2) == &smallLeafyTreeBlueprint,
         "SMALL_LEAFY_TREE_2 returns small leafy tree blueprint");
    TEST(getObjectBlueprint(ObjectType::TALL_LEAFY_TREE) == &tallLeafyTreeBlueprint,
         "TALL_LEAFY_TREE returns tall leafy tree blueprint");
    TEST(getObjectBlueprint(ObjectType::GAZEBO) == &gazeboBlueprint,
         "GAZEBO returns gazebo blueprint");
    TEST(getObjectBlueprint(ObjectType::FIR_TREE) == &firTreeBlueprint,
         "FIR_TREE returns fir tree blueprint");
    TEST(getObjectBlueprint(ObjectType::BUILDING) == &buildingBlueprint,
         "BUILDING returns building blueprint");
    TEST(getObjectBlueprint(ObjectType::ROCKET) == &rocketBlueprint,
         "ROCKET returns rocket blueprint");

    // Test destroyed/smoking types
    TEST(getObjectBlueprint(ObjectType::SMOKING_REMAINS_L) == &smokingRemainsLeftBlueprint,
         "SMOKING_REMAINS_L returns smoking remains left blueprint");
    TEST(getObjectBlueprint(ObjectType::SMOKING_REMAINS_R) == &smokingRemainsRightBlueprint,
         "SMOKING_REMAINS_R returns smoking remains right blueprint");
    TEST(getObjectBlueprint(ObjectType::SMOKING_GAZEBO) == &smokingGazeboBlueprint,
         "SMOKING_GAZEBO returns smoking gazebo blueprint");
    TEST(getObjectBlueprint(ObjectType::SMOKING_BUILDING) == &smokingBuildingBlueprint,
         "SMOKING_BUILDING returns smoking building blueprint");

    // Test NONE type returns nullptr
    TEST(getObjectBlueprint(ObjectType::NONE) == nullptr,
         "NONE returns nullptr");

    // Test invalid type returns nullptr
    TEST(getObjectBlueprint(100) == nullptr,
         "Invalid type returns nullptr");
}

void testObjectColors() {
    printf("\n=== Object Color Tests ===\n");

    // Trees should have green faces (high G component)
    bool treeHasGreen = false;
    for (uint32_t i = 0; i < smallLeafyTreeBlueprint.faceCount; i++) {
        uint16_t g = (smallLeafyTreeBlueprint.faces[i].color >> 4) & 0xF;
        if (g >= 4) treeHasGreen = true;
    }
    TEST(treeHasGreen, "Small leafy tree has green faces");

    // Building should have variety of grays (walls) and colors (roof)
    bool hasGray = false;
    bool hasColor = false;
    for (uint32_t i = 0; i < buildingBlueprint.faceCount; i++) {
        uint16_t c = buildingBlueprint.faces[i].color;
        uint16_t r = (c >> 8) & 0xF;
        uint16_t g = (c >> 4) & 0xF;
        uint16_t b = c & 0xF;
        if (r == g && g == b && r > 0) hasGray = true;  // Gray (R=G=B)
        if (r != g || g != b) hasColor = true;  // Non-gray color
    }
    TEST(hasGray, "Building has gray faces (walls)");
    TEST(hasColor, "Building has colored faces (roof)");

    // Rocket should have yellow/orange colors (high R+G)
    bool hasYellow = false;
    for (uint32_t i = 0; i < rocketBlueprint.faceCount; i++) {
        uint16_t c = rocketBlueprint.faces[i].color;
        uint16_t r = (c >> 8) & 0xF;
        uint16_t g = (c >> 4) & 0xF;
        if (r >= 8 && g >= 8) hasYellow = true;  // Yellow (high R and G)
    }
    TEST(hasYellow, "Rocket has yellow/orange faces");

    // Smoking remains should be black (color = 0x000)
    bool allBlack = true;
    for (uint32_t i = 0; i < smokingRemainsLeftBlueprint.faceCount; i++) {
        if (smokingRemainsLeftBlueprint.faces[i].color != 0x000) {
            allBlack = false;
        }
    }
    TEST(allBlack, "Smoking remains left faces are black");
}

int main() {
    printf("=== Object3D Tests ===\n\n");

    testShipBlueprint();
    testShipVertices();
    testShipFaces();
    testLandscapeObjects();
    testGetObjectBlueprint();
    testObjectColors();

    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", testsPassed);
    printf("Failed: %d\n", testsFailed);

    return testsFailed > 0 ? 1 : 0;
}
