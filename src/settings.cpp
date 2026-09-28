// settings.cpp
// Save and load game settings

#include "settings.h"
#include "constants.h"
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <shlobj.h>
#include <direct.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

// =============================================================================
// Settings path - use platform-appropriate location
// =============================================================================

namespace {
    // Settings directory, created if necessary, or "" to use the working directory
    std::string settingsDirectory() {
#ifdef _WIN32
        // %APPDATA%\Lander
        char appDataPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appDataPath))) {
            std::string dir = std::string(appDataPath) + "\\Lander";
            _mkdir(dir.c_str());
            return dir + "\\";
        }
#else
        const char* home = std::getenv("HOME");
        if (!home) {
            if (const passwd* pw = getpwuid(getuid())) {
                home = pw->pw_dir;
            }
        }
        if (home) {
#ifdef __APPLE__
            // ~/Library/Application Support/Lander
            std::string dir = std::string(home) + "/Library/Application Support/Lander";
#else
            // $XDG_CONFIG_HOME/Lander, defaulting to ~/.config/Lander
            const char* xdg = std::getenv("XDG_CONFIG_HOME");
            std::string config = (xdg && *xdg) ? xdg : std::string(home) + "/.config";
            mkdir(config.c_str(), 0755);
            std::string dir = config + "/Lander";
#endif
            mkdir(dir.c_str(), 0755);
            return dir + "/";
        }
#endif
        return "";
    }

    // Parse a whole-string decimal integer
    bool parseInt(const std::string& text, int& out) {
        if (text.empty()) {
            return false;
        }
        char* end = nullptr;
        errno = 0;
        long value = std::strtol(text.c_str(), &end, 10);
        if (errno != 0 || *end != '\0' || value < INT_MIN || value > INT_MAX) {
            return false;
        }
        out = static_cast<int>(value);
        return true;
    }

    // Replace `to` with `from`, so a crash mid-save never leaves a partial file
    bool replaceFile(const std::string& from, const std::string& to) {
#ifdef _WIN32
        return MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
        return std::rename(from.c_str(), to.c_str()) == 0;
#endif
    }
}

std::string getSettingsPath() {
    static const std::string path = settingsDirectory() + "settings.cfg";
    return path;
}

// =============================================================================
// Save settings
// =============================================================================

bool saveSettings(const GameSettings& settings) {
    const std::string path = getSettingsPath();
    const std::string tempPath = path + ".tmp";

    {
        std::ofstream file(tempPath);

        // Write settings in simple key=value format
        file << "# Lander Settings\n";
        file << "scale=" << settings.scale << "\n";
        file << "fpsIndex=" << settings.fpsIndex << "\n";
        file << "fullscreen=" << (settings.fullscreen ? 1 : 0) << "\n";
        file << "smoothClipping=" << (settings.smoothClipping ? 1 : 0) << "\n";
        file << "soundEnabled=" << (settings.soundEnabled ? 1 : 0) << "\n";
        file << "landscapeScale=" << settings.landscapeScale << "\n";
        file << "starsEnabled=" << (settings.starsEnabled ? 1 : 0) << "\n";
        file << "highScore=" << settings.highScore << "\n";

        file.close();
        if (file.fail()) {
            std::remove(tempPath.c_str());
            return false;
        }
    }

    return replaceFile(tempPath, path);
}

// =============================================================================
// Load settings
// =============================================================================

GameSettings loadSettings() {
    GameSettings settings;  // Start with defaults

    std::ifstream file(getSettingsPath());

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Parse key=value, ignoring anything malformed
        size_t pos = line.find('=');
        int v = 0;
        if (pos == std::string::npos || !parseInt(line.substr(pos + 1), v)) {
            continue;
        }
        std::string key = line.substr(0, pos);

        if (key == "scale") {
            if (v == 1 || v == 2 || v == 4) {
                settings.scale = v;
            }
        } else if (key == "fpsIndex") {
            if (v >= 0 && v < FPS_OPTION_COUNT) {
                settings.fpsIndex = v;
            }
        } else if (key == "fullscreen") {
            settings.fullscreen = (v != 0);
        } else if (key == "smoothClipping") {
            settings.smoothClipping = (v != 0);
        } else if (key == "soundEnabled") {
            settings.soundEnabled = (v != 0);
        } else if (key == "landscapeScale") {
            if (v == 1 || v == 2 || v == 4 || v == 8) {
                settings.landscapeScale = v;
            }
        } else if (key == "starsEnabled") {
            settings.starsEnabled = (v != 0);
        } else if (key == "highScore") {
            // High score must be at least 500 (initial value)
            if (v >= 500) {
                settings.highScore = v;
            }
        }
    }

    return settings;
}
