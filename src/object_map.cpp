#include "object_map.h"
#include "landscape.h"
#include <cstring>

// Global instances
ObjectMap objectMap;
RandomNumberGenerator gameRng;

ObjectMap::ObjectMap() {
    clear();
}

void ObjectMap::clear() {
    // Original initializes to 0xFF (no object)
    memset(map, ObjectType::NONE, sizeof(map));
}

uint8_t ObjectMap::getObjectAt(uint8_t tileX, uint8_t tileZ) const {
    return map[tileZ][tileX];
}

void ObjectMap::setObjectAt(uint8_t tileX, uint8_t tileZ, uint8_t objectType) {
    map[tileZ][tileX] = objectType;
}

uint8_t ObjectMap::getObjectAtWorld(int32_t worldX, int32_t worldZ) const {
    // Original: address = objectMap + (x >> 24) + ((z >> 24) << 8)
    // The >> 24 extracts the integer part of 8.24 fixed-point
    uint8_t tileX = static_cast<uint8_t>(worldX >> 24);
    uint8_t tileZ = static_cast<uint8_t>(worldZ >> 24);
    return getObjectAt(tileX, tileZ);
}

void ObjectMap::setObjectAtWorld(int32_t worldX, int32_t worldZ, uint8_t objectType) {
    uint8_t tileX = static_cast<uint8_t>(worldX >> 24);
    uint8_t tileZ = static_cast<uint8_t>(worldZ >> 24);
    setObjectAt(tileX, tileZ, objectType);
}

bool ObjectMap::hasObject(uint8_t tileX, uint8_t tileZ) const {
    return getObjectAt(tileX, tileZ) != ObjectType::NONE;
}

uint8_t ObjectMap::getDestroyedType(uint8_t objectType) {
    // Map object types to their destroyed variants
    // From the objectTypes table, destroyed types start at 12
    // The mapping is: original type + 12 = destroyed type
    // But there are special cases for some objects

    if (objectType >= 12) {
        // Already destroyed
        return objectType;
    }

    switch (objectType) {
        case ObjectType::SMALL_LEAFY_TREE:
        case ObjectType::SMALL_LEAFY_TREE_2:
        case ObjectType::SMALL_LEAFY_TREE_3:
            return ObjectType::SMOKING_REMAINS_L;  // 14
        case ObjectType::TALL_LEAFY_TREE:
        case ObjectType::TALL_LEAFY_TREE_2:
            return ObjectType::SMOKING_REMAINS_R;  // 13
        case ObjectType::GAZEBO:
            return ObjectType::SMOKING_GAZEBO;     // 17
        case ObjectType::FIR_TREE:
            return ObjectType::SMOKING_REMAINS_R2; // 18
        case ObjectType::BUILDING:
            return ObjectType::SMOKING_BUILDING;   // 20
        case ObjectType::ROCKET:
        case ObjectType::ROCKET_2:
        case ObjectType::ROCKET_3:
            return ObjectType::SMOKING_REMAINS_R4; // 21
        default:
            return ObjectType::SMOKING_REMAINS_L;  // Default to smoking remains
    }
}

bool ObjectMap::isDestroyedType(uint8_t objectType) {
    return objectType >= 12 && objectType != ObjectType::NONE;
}

// =============================================================================
// Random Number Generator Implementation
// =============================================================================

RandomNumberGenerator::RandomNumberGenerator() {
    // Default seeds - can be changed with seed()
    seed1 = 0x12345678;
    seed2 = 0x87654321;
}

void RandomNumberGenerator::seed(uint32_t s1, uint32_t s2) {
    seed1 = s1;
    seed2 = s2;
}

void RandomNumberGenerator::getRandomNumbers(uint32_t& r0, uint32_t& r1) {
    // Port of GetRandomNumbers from Lander.arm (lines 7832-7856)
    // This is a 33-bit LFSR from the ARM Assembler manual
    //
    // Original ARM code:
    //   LDR R0, randomSeed1
    //   LDR R1, randomSeed2
    //   TST R1, R1, LSR #1        ; Set flags on R1 AND (R1 >> 1)
    //   MOVS R14, R0, RRX         ; Rotate R0 right through carry
    //   ADC R1, R1, R1            ; R1 = R1 + R1 + C
    //   EOR R14, R14, R0, LSL #12 ; R14 = R14 EOR (R0 << 12)
    //   EOR R0, R14, R14, LSR #20 ; R0 = R14 EOR (R14 >> 20)
    //   STR R1, randomSeed1
    //   STR R0, randomSeed2

    uint32_t s1 = seed1;
    uint32_t s2 = seed2;

    // TST R1, R1, LSR #1 - sets carry if bit 0 of (R1 >> 1) is set
    // Actually: carry is set if bit 1 of original R1 is set AND bit 0 is set
    // The TST sets N, Z, C flags. C is set from the shifter output's bit 0
    // For LSR #1, the carry out is the original bit 0 of the operand
    uint32_t carry = (s2 & 1) ? 1 : 0;

    // MOVS R14, R0, RRX - rotate R0 right through carry
    // RRX: shift right by 1, carry in at top, carry out from bottom
    uint32_t newCarry = s1 & 1;
    uint32_t r14 = (s1 >> 1) | (carry << 31);
    carry = newCarry;

    // ADC R1, R1, R1 - R1 = R1 + R1 + C (shift left and add carry)
    s2 = s2 + s2 + carry;

    // EOR R14, R14, R0, LSL #12
    r14 = r14 ^ (s1 << 12);

    // EOR R0, R14, R14, LSR #20
    s1 = r14 ^ (r14 >> 20);

    // Store back
    seed1 = s2;
    seed2 = s1;

    r0 = s1;
    r1 = s2;
}

uint32_t RandomNumberGenerator::getNext() {
    uint32_t r0, r1;
    getRandomNumbers(r0, r1);
    return r0;
}

// =============================================================================
// Object Placement Implementation
// =============================================================================

void placeObjectsOnMap() {
    using namespace GameConstants;
    using namespace ObjectMapConstants;

    // Clear the map first (set all to 0xFF = no object)
    objectMap.clear();

    // Place 2048 random objects
    for (int i = 0; i < OBJECT_COUNT; i++) {
        uint32_t r0, r1;
        gameRng.getRandomNumbers(r0, r1);

        // Original uses top byte of random number for coordinates
        // R8 = R0 (x-coordinate in top byte)
        // R9 = R0 << 8 (z-coordinate in top byte, different from x)
        uint32_t worldX = r0;
        uint32_t worldZ = r0 << 8;

        // Get tile coordinates (top byte of each)
        uint8_t tileX = static_cast<uint8_t>(worldX >> 24);
        uint8_t tileZ = static_cast<uint8_t>(worldZ >> 24);

        // Get landscape altitude at this position
        Fixed altitude = getLandscapeAltitude(
            Fixed::fromRaw(static_cast<int32_t>(worldX)),
            Fixed::fromRaw(static_cast<int32_t>(worldZ))
        );

        // Skip if on sea level or launchpad
        if (altitude == SEA_LEVEL || altitude == LAUNCHPAD_ALTITUDE) {
            continue;
        }

        // Determine object type (1-8, weighted towards trees)
        // Original: AND R0, R0, #7; ADD R0, R0, #1
        // This gives types 1-8:
        //   1 = small leafy tree
        //   2 = tall leafy tree
        //   3 = small leafy tree
        //   4 = small leafy tree
        //   5 = gazebo
        //   6 = tall leafy tree
        //   7 = fir tree
        //   8 = building
        uint8_t objectType = static_cast<uint8_t>((r0 & 7) + 1);

        // Place the object
        objectMap.setObjectAt(tileX, tileZ, objectType);
    }

    // Place 3 rockets along the right edge of the launchpad
    // At (7, 1), (7, 3), (7, 5) - x=7, z=1,3,5
    // Original: STRB R0, [R6, #&0107] etc.
    // The offset &0107 = z*256 + x = 1*256 + 7 = (7, 1)
    objectMap.setObjectAt(7, 1, ObjectType::LAUNCHPAD_OBJECT);
    objectMap.setObjectAt(7, 3, ObjectType::LAUNCHPAD_OBJECT);
    objectMap.setObjectAt(7, 5, ObjectType::LAUNCHPAD_OBJECT);
}
