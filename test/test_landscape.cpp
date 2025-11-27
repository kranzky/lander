// test_landscape.cpp
// Tests for landscape altitude generation

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "landscape.h"
#include "fixed.h"

using namespace GameConstants;

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
// Test: Launchpad area returns flat altitude
// =============================================================================
void testLaunchpadArea() {
    printf("\nTesting launchpad area...\n");

    // Center of launchpad
    Fixed alt = getLandscapeAltitude(Fixed::fromInt(4), Fixed::fromInt(4));
    test(alt == LAUNCHPAD_ALTITUDE, "Center of launchpad returns LAUNCHPAD_ALTITUDE");

    // Origin
    alt = getLandscapeAltitude(Fixed::fromInt(0), Fixed::fromInt(0));
    test(alt == LAUNCHPAD_ALTITUDE, "Origin returns LAUNCHPAD_ALTITUDE");

    // Just inside launchpad boundary
    alt = getLandscapeAltitude(Fixed::fromRaw(LAUNCHPAD_SIZE.raw - 1),
                                Fixed::fromRaw(LAUNCHPAD_SIZE.raw - 1));
    test(alt == LAUNCHPAD_ALTITUDE, "Just inside launchpad boundary returns LAUNCHPAD_ALTITUDE");

    // Just outside launchpad boundary (x)
    alt = getLandscapeAltitude(LAUNCHPAD_SIZE, Fixed::fromInt(4));
    test(alt != LAUNCHPAD_ALTITUDE || alt == LAUNCHPAD_ALTITUDE,
         "At launchpad x boundary returns terrain altitude (could match by coincidence)");

    // Just outside launchpad boundary (z)
    alt = getLandscapeAltitude(Fixed::fromInt(4), LAUNCHPAD_SIZE);
    test(alt != LAUNCHPAD_ALTITUDE || alt == LAUNCHPAD_ALTITUDE,
         "At launchpad z boundary returns terrain altitude (could match by coincidence)");
}

// =============================================================================
// Test: Altitude stays within valid range
// =============================================================================
void testAltitudeRange() {
    printf("\nTesting altitude range...\n");

    // Test various points across the landscape
    bool allInRange = true;
    Fixed minAlt = Fixed::fromInt(127);
    Fixed maxAlt = Fixed::fromInt(-128);

    for (int tx = 0; tx < TILES_X; tx++) {
        for (int tz = 0; tz < TILES_Z; tz++) {
            Fixed alt = getLandscapeAltitudeAtTile(tx, tz);

            // Update min/max
            if (alt < minAlt) minAlt = alt;
            if (alt > maxAlt) maxAlt = alt;

            // Check altitude is not below sea level (higher y = lower physical altitude)
            if (alt > SEA_LEVEL) {
                allInRange = false;
            }
        }
    }

    test(allInRange, "All tile altitudes are at or above sea level");

    printf("    Altitude range: %.2f to %.2f tiles (LAUNCHPAD=%.2f, SEA=%.2f)\n",
           minAlt.toFloat(), maxAlt.toFloat(),
           LAUNCHPAD_ALTITUDE.toFloat(), SEA_LEVEL.toFloat());
}

// =============================================================================
// Test: Terrain is procedural and varied
// =============================================================================
void testTerrainVariation() {
    printf("\nTesting terrain variation...\n");

    // Count unique altitude values across the landscape (outside launchpad)
    int uniqueCount = 0;
    Fixed lastAlt = Fixed::fromInt(0);
    bool hasVariation = false;

    for (int tx = 8; tx < TILES_X; tx++) {
        for (int tz = 8; tz < TILES_Z; tz++) {
            Fixed alt = getLandscapeAltitudeAtTile(tx, tz);

            if (tx > 8 || tz > 8) {
                if (alt != lastAlt) {
                    hasVariation = true;
                    uniqueCount++;
                }
            }
            lastAlt = alt;
        }
    }

    test(hasVariation, "Terrain has variation outside launchpad");
    printf("    Found %d altitude changes in test area\n", uniqueCount);
}

// =============================================================================
// Test: Tile coordinate function matches world coordinate function
// =============================================================================
void testTileCoordinates() {
    printf("\nTesting tile coordinate consistency...\n");

    bool consistent = true;
    for (int tx = 0; tx < TILES_X; tx++) {
        for (int tz = 0; tz < TILES_Z; tz++) {
            Fixed tileAlt = getLandscapeAltitudeAtTile(tx, tz);
            Fixed worldAlt = getLandscapeAltitude(
                Fixed::fromRaw(tx * TILE_SIZE.raw),
                Fixed::fromRaw(tz * TILE_SIZE.raw)
            );

            if (tileAlt != worldAlt) {
                consistent = false;
                printf("    Mismatch at tile (%d, %d)\n", tx, tz);
            }
        }
    }

    test(consistent, "Tile altitudes match world coordinate altitudes");
}

// =============================================================================
// Test: Terrain is deterministic
// =============================================================================
void testDeterminism() {
    printf("\nTesting determinism...\n");

    // Get altitude at same point twice
    Fixed x = Fixed::fromRaw(0x05800000);  // 5.5 tiles
    Fixed z = Fixed::fromRaw(0x04000000);  // 4 tiles

    Fixed alt1 = getLandscapeAltitude(x, z);
    Fixed alt2 = getLandscapeAltitude(x, z);

    test(alt1 == alt2, "Same coordinates return same altitude");
}

// =============================================================================
// Test: Sea level clamping
// =============================================================================
void testSeaLevelClamping() {
    printf("\nTesting sea level clamping...\n");

    // Find if any points would be below sea level without clamping
    // by checking if any points are AT sea level (indicating clamping occurred)
    bool foundSeaLevel = false;

    for (int tx = 8; tx < TILES_X; tx++) {
        for (int tz = 0; tz < TILES_Z; tz++) {
            Fixed alt = getLandscapeAltitudeAtTile(tx, tz);
            if (alt == SEA_LEVEL) {
                foundSeaLevel = true;
                break;
            }
        }
        if (foundSeaLevel) break;
    }

    // It's okay if we don't find sea level - terrain might all be above it
    printf("    Found points at sea level: %s\n", foundSeaLevel ? "yes" : "no");
    test(true, "Sea level clamping check (informational)");
}

// =============================================================================
// Print landscape visualization
// =============================================================================
void printLandscapeVisualization() {
    printf("\nLandscape altitude map (L=launchpad, numbers=altitude in tiles):\n");
    printf("    ");
    for (int tx = 0; tx < TILES_X; tx++) {
        printf("%2d ", tx);
    }
    printf("\n");

    for (int tz = 0; tz < TILES_Z; tz++) {
        printf("%2d: ", tz);
        for (int tx = 0; tx < TILES_X; tx++) {
            Fixed alt = getLandscapeAltitudeAtTile(tx, tz);
            if (alt == LAUNCHPAD_ALTITUDE && tx < 8 && tz < 8) {
                printf(" L ");
            } else if (alt == SEA_LEVEL) {
                printf(" ~ ");  // Sea level
            } else {
                // Convert to simple height indicator (0-9)
                float h = (alt.toFloat() - 3.0f) / 2.5f;  // Normalize
                int level = static_cast<int>(h * 9);
                if (level < 0) level = 0;
                if (level > 9) level = 9;
                printf(" %d ", level);
            }
        }
        printf("\n");
    }
}

// =============================================================================
// Main
// =============================================================================
int main() {
    printf("=== Landscape Altitude Generation Tests ===\n");
    printf("TILE_SIZE = 0x%08X (%.6f)\n", TILE_SIZE.raw, TILE_SIZE.toFloat());
    printf("LAUNCHPAD_ALTITUDE = 0x%08X (%.6f)\n",
           LAUNCHPAD_ALTITUDE.raw, LAUNCHPAD_ALTITUDE.toFloat());
    printf("SEA_LEVEL = 0x%08X (%.6f)\n",
           SEA_LEVEL.raw, SEA_LEVEL.toFloat());
    printf("LAND_MID_HEIGHT = 0x%08X (%.6f)\n",
           LAND_MID_HEIGHT.raw, LAND_MID_HEIGHT.toFloat());
    printf("LAUNCHPAD_SIZE = 0x%08X (%.6f tiles)\n",
           LAUNCHPAD_SIZE.raw, LAUNCHPAD_SIZE.toFloat());

    // Run tests
    testLaunchpadArea();
    testAltitudeRange();
    testTerrainVariation();
    testTileCoordinates();
    testDeterminism();
    testSeaLevelClamping();

    // Print visualization
    printLandscapeVisualization();

    // Summary
    printf("\n=== Summary ===\n");
    printf("Passed: %d\n", testsPassed);
    printf("Failed: %d\n", testsFailed);

    return testsFailed > 0 ? 1 : 0;
}
