#ifndef LANDER_OBJECT_MAP_H
#define LANDER_OBJECT_MAP_H

#include <cstdint>

// =============================================================================
// Object Map System
// =============================================================================
//
// Port of the object map from Lander.arm (PlaceObjectsOnMap, lines 12263-12398).
//
// The object map is a 256x256 byte array where each byte indicates what object
// (if any) exists at that tile coordinate. The original stores this at offset
// objectMap = &4400 + STORE * 2, with size 256 * 256 = &10000 bytes.
//
// Object addressing in original:
//   Address = objectMap + (x >> 24) + ((z >> 24) << 8)
// Where x and z are 8.24 fixed-point world coordinates.
// This means only the top 8 bits (integer part) of each coordinate are used.
//
// Object types (from objectTypes table, lines 4638-4666):
//   0  = pyramid (unused)
//   1  = small leafy tree
//   2  = tall leafy tree
//   3  = small leafy tree
//   4  = small leafy tree
//   5  = gazebo
//   6  = tall leafy tree
//   7  = fir tree
//   8  = building
//   9  = rocket
//   10 = rocket
//   11 = rocket
//   12+ = smoking/destroyed variants
//   0xFF = no object (empty)
//
// =============================================================================

namespace ObjectType {
    constexpr uint8_t NONE = 0xFF;           // No object at this location
    constexpr uint8_t PYRAMID = 0;           // Unused in original
    constexpr uint8_t SMALL_LEAFY_TREE = 1;
    constexpr uint8_t TALL_LEAFY_TREE = 2;
    constexpr uint8_t SMALL_LEAFY_TREE_2 = 3;  // Same model as type 1
    constexpr uint8_t SMALL_LEAFY_TREE_3 = 4;  // Same model as type 1
    constexpr uint8_t GAZEBO = 5;
    constexpr uint8_t TALL_LEAFY_TREE_2 = 6;   // Same model as type 2
    constexpr uint8_t FIR_TREE = 7;
    constexpr uint8_t BUILDING = 8;
    constexpr uint8_t ROCKET = 9;
    constexpr uint8_t ROCKET_2 = 10;           // Same model as type 9
    constexpr uint8_t ROCKET_3 = 11;           // Same model as type 9

    // Destroyed/smoking object types (12+)
    constexpr uint8_t SMOKING_ROCKET = 12;     // Unused
    constexpr uint8_t SMOKING_REMAINS_R = 13;  // Bends right
    constexpr uint8_t SMOKING_REMAINS_L = 14;  // Bends left
    constexpr uint8_t SMOKING_REMAINS_L2 = 15;
    constexpr uint8_t SMOKING_REMAINS_L3 = 16;
    constexpr uint8_t SMOKING_GAZEBO = 17;
    constexpr uint8_t SMOKING_REMAINS_R2 = 18;
    constexpr uint8_t SMOKING_REMAINS_R3 = 19;
    constexpr uint8_t SMOKING_BUILDING = 20;
    constexpr uint8_t SMOKING_REMAINS_R4 = 21;
    constexpr uint8_t SMOKING_REMAINS_L4 = 22;
    constexpr uint8_t SMOKING_REMAINS_L5 = 23;

    // Launchpad rocket type
    constexpr uint8_t LAUNCHPAD_OBJECT = 9;
}

namespace ObjectMapConstants {
    constexpr int MAP_SIZE = 256;  // 256x256 grid
    constexpr int OBJECT_COUNT = 2048;  // Number of random objects to place
}

// Object map class
class ObjectMap {
public:
    ObjectMap();

    // Clear the map (set all to NONE)
    void clear();

    // Get/set object at tile coordinates
    uint8_t getObjectAt(uint8_t tileX, uint8_t tileZ) const;
    void setObjectAt(uint8_t tileX, uint8_t tileZ, uint8_t objectType);

    // Get/set using world coordinates (extracts tile from 8.24 fixed-point)
    uint8_t getObjectAtWorld(int32_t worldX, int32_t worldZ) const;
    void setObjectAtWorld(int32_t worldX, int32_t worldZ, uint8_t objectType);

    // Check if an object exists at position
    bool hasObject(uint8_t tileX, uint8_t tileZ) const;

    // Convert object type to its destroyed/smoking variant
    static uint8_t getDestroyedType(uint8_t objectType);

    // Check if object type is a destroyed/smoking variant
    static bool isDestroyedType(uint8_t objectType);

private:
    uint8_t map[ObjectMapConstants::MAP_SIZE][ObjectMapConstants::MAP_SIZE];
};

// Global object map instance
extern ObjectMap objectMap;

// =============================================================================
// Random Number Generator
// =============================================================================
//
// Port of GetRandomNumbers from Lander.arm (lines 7797-7856).
//
// This is a 33-bit LFSR with taps at bits 20 and 33, as described in the
// original ARM Assembler manual (section 11.2, page 96).
//
// =============================================================================

class RandomNumberGenerator {
public:
    RandomNumberGenerator();

    // Seed the generator
    void seed(uint32_t seed1, uint32_t seed2);

    // Get next random numbers (returns two values like original)
    void getRandomNumbers(uint32_t& r0, uint32_t& r1);

    // Convenience: get a single random number
    uint32_t getNext();

private:
    uint32_t seed1;
    uint32_t seed2;
};

// Global RNG instance
extern RandomNumberGenerator gameRng;

// =============================================================================
// Object Placement
// =============================================================================
//
// Port of PlaceObjectsOnMap from Lander.arm (lines 12263-12415).
//
// Places 2048 random objects on the landscape, avoiding sea and launchpad.
// Also places 3 rockets along the right edge of the launchpad at (7,1), (7,3),
// and (7,5).
//
// =============================================================================

// Place objects randomly on the landscape
// Clears the map first, then places random objects and launchpad rockets
void placeObjectsOnMap();

#endif // LANDER_OBJECT_MAP_H
