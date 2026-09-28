#ifndef LANDER_SOUND_H
#define LANDER_SOUND_H

#include <SDL.h>
#include <string>
#include <vector>

// =============================================================================
// Sound System
// =============================================================================
//
// Handles loading and playing WAV sound effects with support for:
// - Pitch shifting (for variants like hover thrust, bullet impact)
// - Looping (for continuous sounds like engine thrust)
// - Volume control (for spatial audio based on distance)
//
// Channels are mixed on SDL's audio thread, so every access to them (and to
// `enabled`, which the mixer reads) happens with the audio device locked.
//
// =============================================================================

// Sound effect identifiers
enum class SoundId {
    BOOM,           // Object explosions
    DEAD,           // Player ship crash
    SHOOT,          // Bullet firing
    SHOOT_IMPACT,   // Bullet hitting ground (pitched down shoot)
    SPLASH,         // Bullet hitting water
    THRUST,         // Engine thrust (looped)
    HOVER,          // Hover thrust (looped, pitched down)
    WATER,          // Ship thrusting over water
    COUNT
};

// A single audio channel for mixing
struct AudioChannel {
    const int16_t* data = nullptr;  // Sample data (nullptr if channel is free)
    uint32_t length = 0;            // Length in samples
    float position = 0.0f;          // Current playback position (float for pitch shifting)
    float volume = 1.0f;            // Volume (0.0 to 1.0)
    float pitch = 1.0f;             // Pitch multiplier (1.0 = normal)
    bool looping = false;           // Whether to loop
    SoundId soundId = SoundId::COUNT;  // Which sound is playing (for managing loops)

    // Low-pass filter state (single-pole IIR)
    float filterCutoff = 1.0f;      // 0.0 = fully filtered, 1.0 = no filter
    float filterState = 0.0f;       // Previous output sample for IIR filter
};

// Loaded sound effect data
struct SoundData {
    std::vector<int16_t> samples;   // Audio samples (mono, 16-bit, at the device rate)
    bool loaded = false;
};

class SoundSystem {
public:
    SoundSystem() = default;
    ~SoundSystem();

    // The audio callback holds a pointer to this instance
    SoundSystem(const SoundSystem&) = delete;
    SoundSystem& operator=(const SoundSystem&) = delete;

    // Initialize SDL audio and load all sounds
    bool init();

    // Shutdown and free resources
    void shutdown();

    // Play a sound effect once
    // volume: 0.0 to 1.0
    void play(SoundId id, float volume = 1.0f);

    // Play a looping sound (does nothing if it is already playing)
    void playLoop(SoundId id, float volume = 1.0f);

    // Stop all instances of a specific sound
    void stopSound(SoundId id);

    // Check if a sound is currently playing
    bool isPlaying(SoundId id) const;

    // Set low-pass filter cutoff for a looping sound
    // cutoff: 1.0 = no filter (bright), 0.0 = fully filtered (muffled)
    void setLoopFilter(SoundId id, float cutoff);

    // Set pitch for a looping sound
    // pitch: 1.0 = normal, <1.0 = lower, >1.0 = higher
    void setLoopPitch(SoundId id, float pitch);

    // Enable/disable all sound (disabling also stops anything playing)
    void setEnabled(bool enable);

private:
    // Load a WAV file into a SoundData structure
    bool loadWav(const std::string& path, SoundData& sound);

    // Create a pitched version of a sound
    void createPitchedVersion(const SoundData& source, SoundData& dest, float pitchFactor);

    // Start a sound on a free channel (if there is one)
    void startChannel(SoundId id, float volume, bool looping);

    // Apply fn to every playing loop of a sound, with the device locked
    template <typename Fn>
    void forEachLoop(SoundId id, Fn fn);

    // SDL audio callback (static, calls instance method)
    static void audioCallback(void* userdata, Uint8* stream, int len);
    void mixAudio(int16_t* stream, int samples);

    // Audio device
    SDL_AudioDeviceID audioDevice = 0;
    SDL_AudioSpec audioSpec{};

    // Sound data for each sound ID
    SoundData sounds[static_cast<int>(SoundId::COUNT)];

    // Mixing channels
    static constexpr int MAX_CHANNELS = 16;
    AudioChannel channels[MAX_CHANNELS];

    bool enabled = true;
    bool initialized = false;
};

#endif // LANDER_SOUND_H
