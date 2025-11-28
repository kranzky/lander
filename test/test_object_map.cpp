// test_object_map.cpp
// Tests for object map system

#include <cstdio>
#include <cstdlib>
#include "object_map.h"

// Test counter
static int testsPassed = 0;
static int testsFailed = 0;

void test(bool condition, const char* name) {
    if (condition) {
        printf("  PASS: %s\n", name);
        testsPassed++;
    } else {
        printf("  FAIL: %s\n", name);
        testsFailed++;
    }
}

// =============================================================================
// Test: Initial state is empty
// =============================================================================
void testInitialState() {
    printf("\nTesting initial state...\n");

    ObjectMap map;

    // Check a few positions are empty
    test(map.getObjectAt(0, 0) == ObjectType::NONE, "Origin is empty");
    test(map.getObjectAt(128, 128) == ObjectType::NONE, "Center is empty");
    test(map.getObjectAt(255, 255) == ObjectType::NONE, "Far corner is empty");

    // Check hasObject returns false for empty positions
    test(!map.hasObject(50, 50), "hasObject returns false for empty position");
}

// =============================================================================
// Test: Set and get objects
// =============================================================================
void testSetAndGet() {
    printf("\nTesting set and get...\n");

    ObjectMap map;

    // Place various object types
    map.setObjectAt(10, 20, ObjectType::SMALL_LEAFY_TREE);
    map.setObjectAt(30, 40, ObjectType::BUILDING);
    map.setObjectAt(100, 150, ObjectType::ROCKET);
    map.setObjectAt(0, 0, ObjectType::GAZEBO);
    map.setObjectAt(255, 255, ObjectType::FIR_TREE);

    // Verify placement
    test(map.getObjectAt(10, 20) == ObjectType::SMALL_LEAFY_TREE, "Small tree placed correctly");
    test(map.getObjectAt(30, 40) == ObjectType::BUILDING, "Building placed correctly");
    test(map.getObjectAt(100, 150) == ObjectType::ROCKET, "Rocket placed correctly");
    test(map.getObjectAt(0, 0) == ObjectType::GAZEBO, "Gazebo at origin placed correctly");
    test(map.getObjectAt(255, 255) == ObjectType::FIR_TREE, "Fir tree at far corner placed correctly");

    // Verify hasObject
    test(map.hasObject(10, 20), "hasObject returns true for tree");
    test(map.hasObject(30, 40), "hasObject returns true for building");

    // Verify empty positions are still empty
    test(map.getObjectAt(50, 50) == ObjectType::NONE, "Empty position still empty");
    test(!map.hasObject(50, 50), "hasObject returns false for empty position");
}

// =============================================================================
// Test: World coordinate access
// =============================================================================
void testWorldCoordinates() {
    printf("\nTesting world coordinate access...\n");

    ObjectMap map;

    // World coordinates are 8.24 fixed-point
    // Tile coordinate = worldCoord >> 24
    // So world coord 0x05000000 = tile 5
    // And world coord 0x0A800000 = tile 10 (0x0A = 10)

    int32_t worldX = 0x05000000;  // Tile 5
    int32_t worldZ = 0x0A000000;  // Tile 10

    map.setObjectAtWorld(worldX, worldZ, ObjectType::TALL_LEAFY_TREE);

    // Verify via world coordinates
    test(map.getObjectAtWorld(worldX, worldZ) == ObjectType::TALL_LEAFY_TREE,
         "Object set via world coords can be retrieved");

    // Verify via tile coordinates
    test(map.getObjectAt(5, 10) == ObjectType::TALL_LEAFY_TREE,
         "Object set via world coords appears at correct tile");

    // Test fractional world coordinates (should map to same tile)
    int32_t worldXFrac = 0x05800000;  // Tile 5.5 -> still tile 5
    int32_t worldZFrac = 0x0AFFFFFF;  // Tile 10.999... -> still tile 10

    test(map.getObjectAtWorld(worldXFrac, worldZFrac) == ObjectType::TALL_LEAFY_TREE,
         "Fractional world coords map to correct tile");
}

// =============================================================================
// Test: Clear map
// =============================================================================
void testClear() {
    printf("\nTesting clear...\n");

    ObjectMap map;

    // Place some objects
    map.setObjectAt(10, 20, ObjectType::SMALL_LEAFY_TREE);
    map.setObjectAt(30, 40, ObjectType::BUILDING);
    map.setObjectAt(100, 150, ObjectType::ROCKET);

    // Verify they exist
    test(map.hasObject(10, 20), "Object exists before clear");

    // Clear the map
    map.clear();

    // Verify all are gone
    test(!map.hasObject(10, 20), "Object removed after clear");
    test(!map.hasObject(30, 40), "Second object removed after clear");
    test(!map.hasObject(100, 150), "Third object removed after clear");
    test(map.getObjectAt(10, 20) == ObjectType::NONE, "Position returns NONE after clear");
}

// =============================================================================
// Test: Object type constants
// =============================================================================
void testObjectTypes() {
    printf("\nTesting object type constants...\n");

    // Verify type values match original
    test(ObjectType::NONE == 0xFF, "NONE is 0xFF");
    test(ObjectType::PYRAMID == 0, "PYRAMID is 0");
    test(ObjectType::SMALL_LEAFY_TREE == 1, "SMALL_LEAFY_TREE is 1");
    test(ObjectType::TALL_LEAFY_TREE == 2, "TALL_LEAFY_TREE is 2");
    test(ObjectType::GAZEBO == 5, "GAZEBO is 5");
    test(ObjectType::FIR_TREE == 7, "FIR_TREE is 7");
    test(ObjectType::BUILDING == 8, "BUILDING is 8");
    test(ObjectType::ROCKET == 9, "ROCKET is 9");
    test(ObjectType::LAUNCHPAD_OBJECT == 9, "LAUNCHPAD_OBJECT is 9 (rocket)");

    // Destroyed types start at 12
    test(ObjectType::SMOKING_ROCKET == 12, "SMOKING_ROCKET is 12");
    test(ObjectType::SMOKING_BUILDING == 20, "SMOKING_BUILDING is 20");
}

// =============================================================================
// Test: Destroyed type mapping
// =============================================================================
void testDestroyedTypes() {
    printf("\nTesting destroyed type mapping...\n");

    // Trees become smoking remains
    test(ObjectMap::getDestroyedType(ObjectType::SMALL_LEAFY_TREE) >= 12,
         "Small tree becomes destroyed type");
    test(ObjectMap::getDestroyedType(ObjectType::TALL_LEAFY_TREE) >= 12,
         "Tall tree becomes destroyed type");
    test(ObjectMap::getDestroyedType(ObjectType::FIR_TREE) >= 12,
         "Fir tree becomes destroyed type");

    // Gazebo becomes smoking gazebo
    test(ObjectMap::getDestroyedType(ObjectType::GAZEBO) == ObjectType::SMOKING_GAZEBO,
         "Gazebo becomes smoking gazebo");

    // Building becomes smoking building
    test(ObjectMap::getDestroyedType(ObjectType::BUILDING) == ObjectType::SMOKING_BUILDING,
         "Building becomes smoking building");

    // Already destroyed types stay the same
    test(ObjectMap::getDestroyedType(ObjectType::SMOKING_BUILDING) == ObjectType::SMOKING_BUILDING,
         "Already destroyed type unchanged");

    // isDestroyedType checks
    test(!ObjectMap::isDestroyedType(ObjectType::SMALL_LEAFY_TREE),
         "Tree is not destroyed type");
    test(!ObjectMap::isDestroyedType(ObjectType::BUILDING),
         "Building is not destroyed type");
    test(ObjectMap::isDestroyedType(ObjectType::SMOKING_BUILDING),
         "Smoking building is destroyed type");
    test(ObjectMap::isDestroyedType(ObjectType::SMOKING_REMAINS_L),
         "Smoking remains is destroyed type");
    test(!ObjectMap::isDestroyedType(ObjectType::NONE),
         "NONE is not destroyed type");
}

// =============================================================================
// Test: Overwrite behavior
// =============================================================================
void testOverwrite() {
    printf("\nTesting overwrite behavior...\n");

    ObjectMap map;

    // Place an object
    map.setObjectAt(50, 50, ObjectType::SMALL_LEAFY_TREE);
    test(map.getObjectAt(50, 50) == ObjectType::SMALL_LEAFY_TREE, "Initial object placed");

    // Overwrite with different object
    map.setObjectAt(50, 50, ObjectType::BUILDING);
    test(map.getObjectAt(50, 50) == ObjectType::BUILDING, "Object overwritten");

    // Overwrite with destroyed type (simulating destruction)
    map.setObjectAt(50, 50, ObjectType::SMOKING_BUILDING);
    test(map.getObjectAt(50, 50) == ObjectType::SMOKING_BUILDING, "Object destroyed");

    // Clear position by setting to NONE
    map.setObjectAt(50, 50, ObjectType::NONE);
    test(map.getObjectAt(50, 50) == ObjectType::NONE, "Object cleared");
    test(!map.hasObject(50, 50), "Position shows as empty");
}

// =============================================================================
// Test: Edge cases and bounds
// =============================================================================
void testEdgeCases() {
    printf("\nTesting edge cases...\n");

    ObjectMap map;

    // Test all four corners
    map.setObjectAt(0, 0, ObjectType::SMALL_LEAFY_TREE);
    map.setObjectAt(255, 0, ObjectType::TALL_LEAFY_TREE);
    map.setObjectAt(0, 255, ObjectType::GAZEBO);
    map.setObjectAt(255, 255, ObjectType::BUILDING);

    test(map.getObjectAt(0, 0) == ObjectType::SMALL_LEAFY_TREE, "Corner (0,0) works");
    test(map.getObjectAt(255, 0) == ObjectType::TALL_LEAFY_TREE, "Corner (255,0) works");
    test(map.getObjectAt(0, 255) == ObjectType::GAZEBO, "Corner (0,255) works");
    test(map.getObjectAt(255, 255) == ObjectType::BUILDING, "Corner (255,255) works");

    // Test that adjacent positions are independent
    map.clear();
    map.setObjectAt(100, 100, ObjectType::ROCKET);
    test(map.getObjectAt(99, 100) == ObjectType::NONE, "Adjacent position (99,100) is empty");
    test(map.getObjectAt(101, 100) == ObjectType::NONE, "Adjacent position (101,100) is empty");
    test(map.getObjectAt(100, 99) == ObjectType::NONE, "Adjacent position (100,99) is empty");
    test(map.getObjectAt(100, 101) == ObjectType::NONE, "Adjacent position (100,101) is empty");
}

// =============================================================================
// Test: Map size
// =============================================================================
void testMapSize() {
    printf("\nTesting map size...\n");

    test(ObjectMapConstants::MAP_SIZE == 256, "Map size is 256");
    test(ObjectMapConstants::OBJECT_COUNT == 2048, "Object count is 2048");

    // Verify the map can hold the expected number of entries
    // 256 * 256 = 65536 entries
    ObjectMap map;
    int expectedSize = 256 * 256;

    // Fill a diagonal with objects
    int placed = 0;
    for (int i = 0; i < 256; i++) {
        map.setObjectAt(i, i, ObjectType::SMALL_LEAFY_TREE);
        if (map.hasObject(i, i)) placed++;
    }
    test(placed == 256, "Can place 256 objects along diagonal");

    printf("    Map capacity: %d tiles\n", expectedSize);
}

// =============================================================================
// Main
// =============================================================================
int main() {
    printf("=== Object Map Tests ===\n");
    printf("MAP_SIZE = %d\n", ObjectMapConstants::MAP_SIZE);
    printf("OBJECT_COUNT = %d (for random placement)\n", ObjectMapConstants::OBJECT_COUNT);

    // Run tests
    testInitialState();
    testSetAndGet();
    testWorldCoordinates();
    testClear();
    testObjectTypes();
    testDestroyedTypes();
    testOverwrite();
    testEdgeCases();
    testMapSize();

    // Summary
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", testsPassed);
    printf("Failed: %d\n", testsFailed);

    return testsFailed > 0 ? 1 : 0;
}
