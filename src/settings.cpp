// settings.cpp
// Save and load game settings

#include "settings.h"
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>

#ifdef __APPLE__
#include <pwd.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include <shlobj.h>
#include <direct.h>
#endif

// =============================================================================
// Settings path - use platform-appropriate location
// =============================================================================

std::string getSettingsPath() {
#ifdef _WIN32
    // Windows: %APPDATA%\Lander\settings.cfg
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
        std::string dir = std::string(appDataPath) + "\\Lander";
        // Create directory if it doesn't exist
        _mkdir(dir.c_str());
        return dir + "\\settings.cfg";
    }
#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/Lander/settings.cfg
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    if (home) {
        std::string dir = std::string(home) + "/Library/Application Support/Lander";
        // Create directory if it doesn't exist
        mkdir(dir.c_str(), 0755);
        return dir + "/settings.cfg";
    }
#endif
    // Fallback to current directory
    return "settings.cfg";
}

// =============================================================================
// Save settings
// =============================================================================

bool saveSettings(const GameSettings& settings) {
    std::string path = getSettingsPath();
    std::ofstream file(path);

    if (!file.is_open()) {
        return false;
    }

    // Write settings in simple key=value format
    file << "# Lander Settings\n";
    file << "scale=" << settings.scale << "\n";
    file << "fpsIndex=" << settings.fpsIndex << "\n";
    file << "fullscreen=" << (settings.fullscreen ? 1 : 0) << "\n";
    file << "smoothClipping=" << (settings.smoothClipping ? 1 : 0) << "\n";
    file << "soundEnabled=" << (settings.soundEnabled ? 1 : 0) << "\n";
    file << "landscapeScale=" << settings.landscapeScale << "\n";
    file << "starsEnabled=" << (settings.starsEnabled ? 1 : 0) << "\n";

    file.close();
    return true;
}

// =============================================================================
// Load settings
// =============================================================================

GameSettings loadSettings() {
    GameSettings settings;  // Start with defaults

    std::string path = getSettingsPath();
    std::ifstream file(path);

    if (!file.is_open()) {
        // File doesn't exist, return defaults
        return settings;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse key=value
        size_t pos = line.find('=');
        if (pos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        // Parse each setting
        if (key == "scale") {
            int v = std::atoi(value.c_str());
            if (v == 1 || v == 2 || v == 4) {
                settings.scale = v;
            }
        } else if (key == "fpsIndex") {
            int v = std::atoi(value.c_str());
            if (v >= 0 && v <= 4) {  // Valid FPS indices: 0-4
                settings.fpsIndex = v;
            }
        } else if (key == "fullscreen") {
            settings.fullscreen = (std::atoi(value.c_str()) != 0);
        } else if (key == "smoothClipping") {
            settings.smoothClipping = (std::atoi(value.c_str()) != 0);
        } else if (key == "soundEnabled") {
            settings.soundEnabled = (std::atoi(value.c_str()) != 0);
        } else if (key == "landscapeScale") {
            int v = std::atoi(value.c_str());
            if (v == 1 || v == 2 || v == 4 || v == 8) {
                settings.landscapeScale = v;
            }
        } else if (key == "starsEnabled") {
            settings.starsEnabled = (std::atoi(value.c_str()) != 0);
        }
    }

    file.close();
    return settings;
}
