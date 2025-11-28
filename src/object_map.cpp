#include "object_map.h"
#include <cstring>

// Global instance
ObjectMap objectMap;

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
