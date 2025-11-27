// landscape.h
// Landscape altitude generation using Fourier synthesis
// Port of the original Lander terrain generation algorithm

#ifndef LANDSCAPE_H
#define LANDSCAPE_H

#include "fixed.h"

// Get the landscape altitude at world coordinates (x, z)
// Uses Fourier synthesis with 6 sine wave terms to generate procedural terrain
// Returns altitude in fixed-point format (lower y = higher physical altitude)
// Special cases:
//   - Launchpad area (x,z both < LAUNCHPAD_SIZE): returns LAUNCHPAD_ALTITUDE
//   - Below sea level: clamps to SEA_LEVEL
Fixed getLandscapeAltitude(Fixed x, Fixed z);

// Get the landscape altitude at tile coordinates (tileX, tileZ)
// Each tile is TILE_SIZE units in world coordinates
// tileX range: 0 to TILES_X-1 (0 to 12)
// tileZ range: 0 to TILES_Z-1 (0 to 10)
Fixed getLandscapeAltitudeAtTile(int tileX, int tileZ);

#endif // LANDSCAPE_H
