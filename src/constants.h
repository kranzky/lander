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

// Scale factor for modern displays (can be changed at runtime)
// 4 = 1280x1024, 2 = 640x512, 1 = 320x256
constexpr int SCALE = 4;  // Default/maximum scale

// Runtime-adjustable scale (declared in constants.h, defined elsewhere)
namespace DisplayConfig {
    extern int scale;  // Current scale (1, 2, or 4)

    inline int getPhysicalWidth() { return ORIGINAL_WIDTH * scale; }
    inline int getPhysicalHeight() { return ORIGINAL_HEIGHT * scale; }
}

// Target resolution (4x original)
constexpr int SCREEN_WIDTH = ORIGINAL_WIDTH * SCALE;    // 1280
constexpr int SCREEN_HEIGHT = ORIGINAL_HEIGHT * SCALE;  // 1024

// Play area dimensions (scaled)
constexpr int PLAY_WIDTH = ORIGINAL_WIDTH * SCALE;      // 1280
constexpr int PLAY_HEIGHT = ORIGINAL_PLAY_HEIGHT * SCALE;  // 512

// Frame rate options (user-selectable at runtime)
// Original Lander ran at ~15fps. We support 15, 30, 60, 120.
// Physics values are tuned for 120fps and scaled down for lower frame rates.
constexpr int FPS_OPTIONS[] = {15, 30, 60, 120};
constexpr int FPS_OPTION_COUNT = 4;
constexpr int DEFAULT_FPS_INDEX = 2;  // Start at 60fps

// Frame time lookup table (milliseconds per frame)
constexpr int FRAME_TIME_MS_LOOKUP[] = {
    1000 / 15,   // 66ms at 15fps
    1000 / 30,   // 33ms at 30fps
    1000 / 60,   // 16ms at 60fps
    1000 / 120   // 8ms at 120fps
};

// Physics scale factor: how many 120fps frames to simulate per actual frame
// At 120fps we do 1x, at 60fps 2x, at 30fps 4x, at 15fps 8x
constexpr int PHYSICS_SCALE[] = {8, 4, 2, 1};

// For backward compatibility (used in init logging)
constexpr int TARGET_FPS = 120;
constexpr int FRAME_TIME_MS = 1000 / TARGET_FPS;

// =============================================================================
// Window Settings
// =============================================================================

constexpr const char* WINDOW_TITLE = "Lander";

#endif // LANDER_CONSTANTS_H
