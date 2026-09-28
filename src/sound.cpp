#include "sound.h"
#include <cstring>
#include <cmath>
#include <algorithm>

// =============================================================================
// SoundSystem Implementation
// =============================================================================

namespace {
    // Holds the audio device lock for the lifetime of the object
    class AudioLock {
    public:
        explicit AudioLock(SDL_AudioDeviceID device) : device(device) { SDL_LockAudioDevice(device); }
        ~AudioLock() { SDL_UnlockAudioDevice(device); }
        AudioLock(const AudioLock&) = delete;
        AudioLock& operator=(const AudioLock&) = delete;
    private:
        SDL_AudioDeviceID device;
    };

    // Directory holding the sounds folder: next to the executable, or in
    // Contents/Resources inside a macOS app bundle (SDL_GetBasePath handles
    // both). Falls back to the working directory.
    std::string resourcePath() {
        char* base = SDL_GetBasePath();
        if (!base) {
            return "";
        }
        std::string path = base;
        SDL_free(base);
        return path;
    }
}

SoundSystem::~SoundSystem() {
    shutdown();
}

bool SoundSystem::init() {
    if (initialized) return true;

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "SDL_InitSubSystem(AUDIO) failed: %s", SDL_GetError());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq = 22050;           // Match original Amiga sample rate
    desired.format = AUDIO_S16SYS;  // 16-bit signed, system byte order
    desired.channels = 1;           // Mono
    desired.samples = 512;          // Buffer size (low latency)
    desired.callback = audioCallback;
    desired.userdata = this;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &audioSpec, 0);
    if (audioDevice == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    std::string soundDir = resourcePath() + "sounds/";
    auto load = [&](const char* file, SoundId id) {
        return loadWav(soundDir + file, sounds[static_cast<int>(id)]);
    };

    bool allLoaded = true;
    allLoaded &= load("boom.wav", SoundId::BOOM);
    allLoaded &= load("dead.wav", SoundId::DEAD);
    allLoaded &= load("shoot.wav", SoundId::SHOOT);
    allLoaded &= load("splash.wav", SoundId::SPLASH);
    allLoaded &= load("thrust.wav", SoundId::THRUST);
    allLoaded &= load("water.wav", SoundId::WATER);

    // Pitched down shoot for bullet ground impact
    createPitchedVersion(sounds[static_cast<int>(SoundId::SHOOT)],
                         sounds[static_cast<int>(SoundId::SHOOT_IMPACT)], 0.4f);

    // Pitched down thrust for hover
    createPitchedVersion(sounds[static_cast<int>(SoundId::THRUST)],
                         sounds[static_cast<int>(SoundId::HOVER)], 0.7f);

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

    SDL_CloseAudioDevice(audioDevice);
    audioDevice = 0;

    for (SoundData& sound : sounds) {
        sound = SoundData{};
    }
    for (AudioChannel& channel : channels) {
        channel = AudioChannel{};
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

    // Copy whole 16-bit samples out of a byte buffer
    auto copySamples = [&sound](const Uint8* bytes, size_t length) {
        sound.samples.resize(length / sizeof(int16_t));
        std::memcpy(sound.samples.data(), bytes, sound.samples.size() * sizeof(int16_t));
    };

    if (result == 0) {
        // No conversion needed
        copySamples(wavBuffer, wavLength);
    } else {
        std::vector<Uint8> buffer(static_cast<size_t>(wavLength) * cvt.len_mult);
        std::memcpy(buffer.data(), wavBuffer, wavLength);
        cvt.len = static_cast<int>(wavLength);
        cvt.buf = buffer.data();

        if (SDL_ConvertAudio(&cvt) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Failed to convert audio: %s", SDL_GetError());
            SDL_FreeWAV(wavBuffer);
            return false;
        }

        copySamples(buffer.data(), cvt.len_cvt);
    }

    SDL_FreeWAV(wavBuffer);
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

    dest.loaded = true;
}

void SoundSystem::startChannel(SoundId id, float volume, bool looping) {
    const SoundData& sound = sounds[static_cast<int>(id)];
    if (!enabled || !initialized || !sound.loaded) return;

    AudioLock lock(audioDevice);

    auto isFree = [](const AudioChannel& channel) { return channel.data == nullptr; };
    auto free = std::find_if(std::begin(channels), std::end(channels), isFree);
    if (free == std::end(channels)) return;

    *free = AudioChannel{};
    free->data = sound.samples.data();
    free->length = static_cast<uint32_t>(sound.samples.size());
    free->volume = volume;
    free->looping = looping;
    free->soundId = id;
}

void SoundSystem::play(SoundId id, float volume) {
    startChannel(id, volume, false);
}

void SoundSystem::playLoop(SoundId id, float volume) {
    if (!isPlaying(id)) {
        startChannel(id, volume, true);
    }
}

void SoundSystem::stopSound(SoundId id) {
    AudioLock lock(audioDevice);

    for (AudioChannel& channel : channels) {
        if (channel.soundId == id) {
            channel.data = nullptr;
        }
    }
}

bool SoundSystem::isPlaying(SoundId id) const {
    AudioLock lock(audioDevice);

    return std::any_of(std::begin(channels), std::end(channels), [id](const AudioChannel& channel) {
        return channel.data != nullptr && channel.soundId == id;
    });
}

template <typename Fn>
void SoundSystem::forEachLoop(SoundId id, Fn fn) {
    AudioLock lock(audioDevice);

    for (AudioChannel& channel : channels) {
        if (channel.data != nullptr && channel.soundId == id && channel.looping) {
            fn(channel);
        }
    }
}

void SoundSystem::setLoopFilter(SoundId id, float cutoff) {
    cutoff = std::clamp(cutoff, 0.0f, 1.0f);
    forEachLoop(id, [cutoff](AudioChannel& channel) { channel.filterCutoff = cutoff; });
}

void SoundSystem::setLoopPitch(SoundId id, float pitch) {
    pitch = std::clamp(pitch, 0.5f, 2.0f);
    forEachLoop(id, [pitch](AudioChannel& channel) { channel.pitch = pitch; });
}

void SoundSystem::setEnabled(bool enable) {
    AudioLock lock(audioDevice);

    enabled = enable;
    if (!enabled) {
        for (AudioChannel& channel : channels) {
            channel.data = nullptr;
        }
    }
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

        float vol = channel.volume;

        // Calculate filter coefficient from cutoff
        // cutoff=1.0 means no filtering (alpha=1.0, output=input)
        // cutoff=0.0 means maximum filtering (alpha≈0.01, very muffled)
        // Using exponential mapping for more natural feel
        float alpha = 0.01f + 0.99f * channel.filterCutoff * channel.filterCutoff;

        for (int i = 0; i < samples; i++) {
            if (channel.position >= channel.length) {
                if (channel.looping) {
                    // Keep the fractional overshoot so pitched loops stay seamless
                    channel.position -= channel.length;
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
