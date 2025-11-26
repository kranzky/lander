#ifndef LANDER_CONSTANTS_H
#define LANDER_CONSTANTS_H

#include <cstdint>

// =============================================================================
// Display Constants
// =============================================================================

// Original Acorn Archimedes resolution
constexpr int ORIGINAL_WIDTH = 320;
constexpr int ORIGINAL_HEIGHT = 256;
constexpr int ORIGINAL_PLAY_HEIGHT = 128;  // Top half is play area

// Scale factor for modern displays
constexpr int SCALE = 4;

// Target resolution (4x original)
constexpr int SCREEN_WIDTH = ORIGINAL_WIDTH * SCALE;    // 1280
constexpr int SCREEN_HEIGHT = ORIGINAL_HEIGHT * SCALE;  // 1024

// Play area dimensions (scaled)
constexpr int PLAY_WIDTH = ORIGINAL_WIDTH * SCALE;      // 1280
constexpr int PLAY_HEIGHT = ORIGINAL_PLAY_HEIGHT * SCALE;  // 512

// Frame rate
constexpr int TARGET_FPS = 60;
constexpr int FRAME_TIME_MS = 1000 / TARGET_FPS;  // ~16ms per frame

// =============================================================================
// Window Settings
// =============================================================================

constexpr const char* WINDOW_TITLE = "Lander";

#endif // LANDER_CONSTANTS_H
