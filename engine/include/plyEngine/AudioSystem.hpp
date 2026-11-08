#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace ply {

/// Fire-and-forget audio: decode a file once, then play many overlapping
/// instances with per-voice volume, pitch and pan.
///
/// Backed by miniaudio, which owns its own output device alongside SDL3's
/// window/input. If no device can be opened (headless CI), the system reports
/// not-ready and every call becomes a no-op, so games need no special casing.
class AudioSystem {
public:
    struct Config {
        int sampleRate = 48000;
        int channels = 2;
        /// Force the silent null backend (deterministic tests, headless CI).
        bool forceNullDevice = false;
    };

    AudioSystem();
    ~AudioSystem();

    AudioSystem(const AudioSystem&) = delete;
    AudioSystem& operator=(const AudioSystem&) = delete;

    /// Initializes the audio device. Returns false when no device is available;
    /// the system then stays in a harmless no-op state.
    bool init();
    bool init(const Config& config);
    void shutdown();

    bool ready() const;

    /// Decodes an audio file (WAV/MP3/FLAC, whatever miniaudio supports).
    /// Returns an invalid id (0) on failure.
    uint32_t loadSound(const std::string& path);

    /// Frees a decoded sound. Stopping its voices is the caller's concern.
    void unloadSound(uint32_t sound);

    /// Plays a sound. Returns a voice id (0 when the system is not ready).
    uint32_t play(uint32_t sound, float volume = 1.0f, float pitch = 1.0f,
                  float pan = 0.0f, bool loop = false);

    void stop(uint32_t voice);
    void stopAll();

    void setMasterVolume(float volume);
    float masterVolume() const;

    /// Number of voices currently playing (diagnostics/tests).
    size_t activeVoiceCount() const;

    const std::string& error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ply
