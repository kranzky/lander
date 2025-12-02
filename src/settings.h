// settings.h
// Save and load game settings

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>

// Settings structure containing all persistent game options
struct GameSettings {
    int scale;           // Display scale (1, 2, or 4)
    int fpsIndex;        // Index into FPS_OPTIONS array
    bool fullscreen;     // Fullscreen mode
    bool smoothClipping; // Smooth edge clipping enabled
    bool soundEnabled;   // Sound effects enabled
    int landscapeScale;  // Landscape scale (1, 2, 4, or 8)

    // Default values
    GameSettings()
        : scale(4)
        , fpsIndex(3)        // 60fps
        , fullscreen(false)
        , smoothClipping(true)
        , soundEnabled(true)
        , landscapeScale(1)
    {}
};

// Get the path to the settings file
// On macOS: ~/Library/Application Support/Lander/settings.cfg
// On Windows: %APPDATA%/Lander/settings.cfg
// On Linux: ~/.config/Lander/settings.cfg
std::string getSettingsPath();

// Save settings to file
// Returns true on success
bool saveSettings(const GameSettings& settings);

// Load settings from file
// Returns default settings if file doesn't exist or can't be read
GameSettings loadSettings();

#endif // SETTINGS_H
