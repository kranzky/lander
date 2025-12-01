#include "sound.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// =============================================================================
// SoundSystem Implementation
// =============================================================================

SoundSystem::SoundSystem() {
    std::memset(&audioSpec, 0, sizeof(audioSpec));
}

SoundSystem::~SoundSystem() {
    shutdown();
}

bool SoundSystem::init() {
    if (initialized) return true;

    // Initialize SDL audio subsystem
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    // Set up desired audio format
    SDL_AudioSpec desired;
    std::memset(&desired, 0, sizeof(desired));
    desired.freq = 22050;           // Match original Amiga sample rate
    desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    desired.channels = 1;           // Mono
    desired.samples = 512;          // Buffer size (low latency)
    desired.callback = audioCallback;
    desired.userdata = this;

    // Open audio device
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpec, 0);
    if (audioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    // Load all sound effects
    bool allLoaded = true;
    allLoaded &= loadWav("sounds/boom.wav", sounds[static_cast<int>(SoundId::BOOM)]);
    allLoaded &= loadWav("sounds/dead.wav", sounds[static_cast<int>(SoundId::DEAD)]);
    allLoaded &= loadWav("sounds/shoot.wav", sounds[static_cast<int>(SoundId::SHOOT)]);
    allLoaded &= loadWav("sounds/splash.wav", sounds[static_cast<int>(SoundId::SPLASH)]);
    allLoaded &= loadWav("sounds/thrust.wav", sounds[static_cast<int>(SoundId::THRUST)]);
    allLoaded &= loadWav("sounds/water.wav", sounds[static_cast<int>(SoundId::WATER)]);

    // Create pitched variants
    if (sounds[static_cast<int>(SoundId::SHOOT)].loaded) {
        // Pitched down shoot for bullet ground impact (lower pitch = 0.4)
        createPitchedVersion(sounds[static_cast<int>(SoundId::SHOOT)],
                            sounds[static_cast<int>(SoundId::SHOOT_IMPACT)], 0.4f);
    }

    if (sounds[static_cast<int>(SoundId::THRUST)].loaded) {
        // Pitched down thrust for hover
        createPitchedVersion(sounds[static_cast<int>(SoundId::THRUST)],
                            sounds[static_cast<int>(SoundId::HOVER)], 0.7f);
    }

    if (!allLoaded) {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Some sound files failed to load");
    }

    // Start audio playback
    SDL_PauseAudioDevice(audioDevice, 0);

    initialized = true;
    SDL_Log("Sound system initialized: %d Hz, %d channels, %d samples",
            audioSpec.freq, audioSpec.channels, audioSpec.samples);

    return true;
}

void SoundSystem::shutdown() {
    if (!initialized) return;

    if (audioDevice != 0) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }

    // Free sound data
    for (int i = 0; i < static_cast<int>(SoundId::COUNT); i++) {
        sounds[i].samples.clear();
        sounds[i].loaded = false;
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized = false;
}

bool SoundSystem::loadWav(const std::string& path, SoundData& sound) {
    SDL_AudioSpec wavSpec;
    Uint8* wavBuffer = nullptr;
    Uint32 wavLength = 0;

    if (SDL_LoadWAV(path.c_str(), &wavSpec, &wavBuffer, &wavLength) == nullptr) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to load %s: %s", path.c_str(), SDL_GetError());
        return false;
    }

    // Convert to our target format if needed
    SDL_AudioCVT cvt;
    int result = SDL_BuildAudioCVT(&cvt,
                                   wavSpec.format, wavSpec.channels, wavSpec.freq,
                                   AUDIO_S16SYS, 1, audioSpec.freq);

    if (result < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to build audio converter: %s", SDL_GetError());
        SDL_FreeWAV(wavBuffer);
        return false;
    }

    if (result == 0) {
        // No conversion needed
        sound.samples.resize(wavLength / sizeof(int16_t));
        std::memcpy(sound.samples.data(), wavBuffer, wavLength);
    } else {
        // Conversion needed
        cvt.len = wavLength;
        cvt.buf = new Uint8[wavLength * cvt.len_mult];
        std::memcpy(cvt.buf, wavBuffer, wavLength);

        if (SDL_ConvertAudio(&cvt) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to convert audio: %s", SDL_GetError());
            delete[] cvt.buf;
            SDL_FreeWAV(wavBuffer);
            return false;
        }

        sound.samples.resize(cvt.len_cvt / sizeof(int16_t));
        std::memcpy(sound.samples.data(), cvt.buf, cvt.len_cvt);
        delete[] cvt.buf;
    }

    SDL_FreeWAV(wavBuffer);

    sound.sampleRate = audioSpec.freq;
    sound.loaded = true;

    SDL_Log("Loaded %s: %zu samples", path.c_str(), sound.samples.size());
    return true;
}

void SoundSystem::createPitchedVersion(const SoundData& source, SoundData& dest, float pitchFactor) {
    if (!source.loaded) return;

    // Resampling: lower pitch = more samples (stretch), higher pitch = fewer samples (compress)
    size_t newLength = static_cast<size_t>(source.samples.size() / pitchFactor);
    dest.samples.resize(newLength);

    for (size_t i = 0; i < newLength; i++) {
        float srcIndex = i * pitchFactor;
        size_t idx0 = static_cast<size_t>(srcIndex);
        size_t idx1 = std::min(idx0 + 1, source.samples.size() - 1);
        float frac = srcIndex - idx0;

        // Linear interpolation between samples
        dest.samples[i] = static_cast<int16_t>(
            source.samples[idx0] * (1.0f - frac) + source.samples[idx1] * frac);
    }

    dest.sampleRate = source.sampleRate;
    dest.loaded = true;
}

int SoundSystem::play(SoundId id, float volume) {
    if (!enabled || !initialized) return -1;

    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SoundId::COUNT)) return -1;
    if (!sounds[idx].loaded) return -1;

    // Find a free channel
    SDL_LockAudioDevice(audioDevice);

    int channel = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data == nullptr) {
            channel = i;
            break;
        }
    }

    if (channel >= 0) {
        channels[channel].data = sounds[idx].samples.data();
        channels[channel].length = static_cast<uint32_t>(sounds[idx].samples.size());
        channels[channel].position = 0.0f;
        channels[channel].volume = volume;
        channels[channel].pitch = 1.0f;
        channels[channel].looping = false;
        channels[channel].soundId = id;
        channels[channel].filterCutoff = 1.0f;  // No filter by default
        channels[channel].filterState = 0.0f;   // Reset filter state
    }

    SDL_UnlockAudioDevice(audioDevice);
    return channel;
}

int SoundSystem::playLoop(SoundId id, float volume) {
    if (!enabled || !initialized) return -1;

    // Don't start a new loop if already playing
    if (isPlaying(id)) return -1;

    int idx = static_cast<int>(id);
    if (idx < 0 || idx >= static_cast<int>(SoundId::COUNT)) return -1;
    if (!sounds[idx].loaded) return -1;

    // Find a free channel
    SDL_LockAudioDevice(audioDevice);

    int channel = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data == nullptr) {
            channel = i;
            break;
        }
    }

    if (channel >= 0) {
        channels[channel].data = sounds[idx].samples.data();
        channels[channel].length = static_cast<uint32_t>(sounds[idx].samples.size());
        channels[channel].position = 0.0f;
        channels[channel].volume = volume;
        channels[channel].pitch = 1.0f;
        channels[channel].looping = true;
        channels[channel].soundId = id;
        channels[channel].filterCutoff = 1.0f;  // No filter by default
        channels[channel].filterState = 0.0f;   // Reset filter state
    }

    SDL_UnlockAudioDevice(audioDevice);
    return channel;
}

void SoundSystem::stopChannel(int channel) {
    if (channel < 0 || channel >= MAX_CHANNELS) return;

    SDL_LockAudioDevice(audioDevice);
    channels[channel].data = nullptr;
    SDL_UnlockAudioDevice(audioDevice);
}

void SoundSystem::stopSound(SoundId id) {
    SDL_LockAudioDevice(audioDevice);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data != nullptr && channels[i].soundId == id) {
            channels[i].data = nullptr;
        }
    }

    SDL_UnlockAudioDevice(audioDevice);
}

bool SoundSystem::isPlaying(SoundId id) const {
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data != nullptr && channels[i].soundId == id) {
            return true;
        }
    }
    return false;
}

void SoundSystem::setLoopVolume(SoundId id, float volume) {
    SDL_LockAudioDevice(audioDevice);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data != nullptr && channels[i].soundId == id && channels[i].looping) {
            channels[i].volume = volume;
        }
    }

    SDL_UnlockAudioDevice(audioDevice);
}

void SoundSystem::setLoopFilter(SoundId id, float cutoff) {
    SDL_LockAudioDevice(audioDevice);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data != nullptr && channels[i].soundId == id && channels[i].looping) {
            // Clamp cutoff to valid range
            channels[i].filterCutoff = (cutoff < 0.0f) ? 0.0f : (cutoff > 1.0f) ? 1.0f : cutoff;
        }
    }

    SDL_UnlockAudioDevice(audioDevice);
}

void SoundSystem::setLoopPitch(SoundId id, float pitch) {
    SDL_LockAudioDevice(audioDevice);

    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (channels[i].data != nullptr && channels[i].soundId == id && channels[i].looping) {
            // Clamp pitch to reasonable range
            channels[i].pitch = (pitch < 0.5f) ? 0.5f : (pitch > 2.0f) ? 2.0f : pitch;
        }
    }

    SDL_UnlockAudioDevice(audioDevice);
}

void SoundSystem::audioCallback(void* userdata, Uint8* stream, int len) {
    SoundSystem* self = static_cast<SoundSystem*>(userdata);
    self->mixAudio(reinterpret_cast<int16_t*>(stream), len / sizeof(int16_t));
}

void SoundSystem::mixAudio(int16_t* stream, int samples) {
    // Clear the buffer
    std::memset(stream, 0, samples * sizeof(int16_t));

    if (!enabled) return;

    // Mix all active channels
    for (int ch = 0; ch < MAX_CHANNELS; ch++) {
        AudioChannel& channel = channels[ch];
        if (channel.data == nullptr) continue;

        float vol = channel.volume * masterVolume;

        // Calculate filter coefficient from cutoff
        // cutoff=1.0 means no filtering (alpha=1.0, output=input)
        // cutoff=0.0 means maximum filtering (alpha≈0.01, very muffled)
        // Using exponential mapping for more natural feel
        float alpha = 0.01f + 0.99f * channel.filterCutoff * channel.filterCutoff;

        for (int i = 0; i < samples; i++) {
            if (channel.position >= channel.length) {
                if (channel.looping) {
                    channel.position = 0.0f;
                } else {
                    channel.data = nullptr;
                    break;
                }
            }

            // Get input sample with linear interpolation for pitch shifting
            uint32_t pos0 = static_cast<uint32_t>(channel.position);
            uint32_t pos1 = pos0 + 1;
            if (pos1 >= channel.length) {
                pos1 = channel.looping ? 0 : pos0;
            }
            float frac = channel.position - pos0;
            float inputSample = channel.data[pos0] * (1.0f - frac) + channel.data[pos1] * frac;

            // Apply single-pole low-pass filter: y[n] = alpha * x[n] + (1-alpha) * y[n-1]
            float filteredSample = alpha * inputSample + (1.0f - alpha) * channel.filterState;
            channel.filterState = filteredSample;

            // Mix sample with volume
            int32_t sample = stream[i] + static_cast<int32_t>(filteredSample * vol);

            // Clamp to prevent clipping
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;

            stream[i] = static_cast<int16_t>(sample);

            // Advance position by pitch (1.0 = normal speed, <1.0 = slower/lower, >1.0 = faster/higher)
            channel.position += channel.pitch;
        }
    }
}
