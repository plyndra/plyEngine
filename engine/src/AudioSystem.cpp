#include "plyEngine/AudioSystem.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <miniaudio.h>

namespace ply {

struct AudioSystem::Impl {
    ma_engine engine{};
    bool engineReady = false;
    bool deviceReady = false;
    std::string error;

    uint32_t nextId = 1;
    std::unordered_map<uint32_t, ma_sound*> sounds;
    std::unordered_map<uint32_t, ma_sound*> voices;
    float masterVolume = 1.0f;

    ~Impl() { releaseAll(); }

    void releaseAll() {
        for (auto& entry : voices) {
            ma_sound_uninit(entry.second);
            delete entry.second;
        }
        voices.clear();
        for (auto& entry : sounds) {
            ma_sound_uninit(entry.second);
            delete entry.second;
        }
        sounds.clear();
    }

    void pruneFinishedVoices() {
        for (auto it = voices.begin(); it != voices.end();) {
            if (ma_sound_at_end(it->second)) {
                ma_sound_uninit(it->second);
                delete it->second;
                it = voices.erase(it);
            } else {
                ++it;
            }
        }
    }
};

AudioSystem::AudioSystem() : impl_(std::make_unique<Impl>()) {}

AudioSystem::~AudioSystem() { shutdown(); }

bool AudioSystem::init() { return init(Config{}); }

bool AudioSystem::init(const Config& config) {
    shutdown();

    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.sampleRate = static_cast<ma_uint32>(
        config.sampleRate > 0 ? config.sampleRate : 48000);
    engineConfig.channels = static_cast<ma_uint32>(
        config.channels > 0 ? config.channels : 2);

    ma_backend backends[1];
    if (config.forceNullDevice) {
        backends[0] = ma_backend_null;
        engineConfig.pContext = nullptr;
        engineConfig.noDevice = MA_TRUE;
    }

    ma_result result = ma_engine_init(&engineConfig, &impl_->engine);
    if (result != MA_SUCCESS) {
        impl_->error = std::string("ma_engine_init failed: ") +
                       ma_result_description(result);
        impl_->engineReady = false;
        impl_->deviceReady = false;
        return false;
    }

    impl_->engineReady = true;
    impl_->deviceReady = true;
    if (config.forceNullDevice) {
        ma_engine_set_volume(&impl_->engine, impl_->masterVolume);
    }
    return true;
}

void AudioSystem::shutdown() {
    if (!impl_) {
        return;
    }
    impl_->releaseAll();
    if (impl_->engineReady) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineReady = false;
        impl_->deviceReady = false;
    }
}

bool AudioSystem::ready() const { return impl_ && impl_->engineReady; }

uint32_t AudioSystem::loadSound(const std::string& path) {
    if (!ready()) {
        return 0;
    }
    auto* sound = new ma_sound();
    const ma_result result = ma_sound_init_from_file(
        &impl_->engine, path.c_str(), MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC,
        nullptr, nullptr, sound);
    if (result != MA_SUCCESS) {
        impl_->error = "ma_sound_init_from_file failed: " +
                       std::string(ma_result_description(result));
        delete sound;
        return 0;
    }
    const uint32_t id = impl_->nextId++;
    impl_->sounds[id] = sound;
    return id;
}

void AudioSystem::unloadSound(uint32_t sound) {
    if (!impl_) {
        return;
    }
    auto it = impl_->sounds.find(sound);
    if (it == impl_->sounds.end()) {
        return;
    }
    ma_sound_uninit(it->second);
    delete it->second;
    impl_->sounds.erase(it);
}

uint32_t AudioSystem::play(uint32_t sound, float volume, float pitch, float pan,
                           bool loop) {
    if (!ready()) {
        return 0;
    }
    auto it = impl_->sounds.find(sound);
    if (it == impl_->sounds.end()) {
        return 0;
    }

    impl_->pruneFinishedVoices();

    // Duplicate so the same decoded sound can play overlapping instances.
    auto* voice = new ma_sound();
    ma_sound* source = it->second;
    if (!ma_sound_is_looping(source) && loop) {
        ma_sound_set_looping(source, MA_TRUE);
    }
    if (ma_sound_init_copy(&impl_->engine, source, 0, nullptr, voice) != MA_SUCCESS) {
        delete voice;
        return 0;
    }

    ma_sound_set_volume(voice, volume);
    ma_sound_set_pitch(voice, pitch);
    ma_sound_set_pan(voice, pan);
    ma_sound_set_looping(voice, loop ? MA_TRUE : MA_FALSE);
    ma_sound_start(voice);

    const uint32_t id = impl_->nextId++;
    impl_->voices[id] = voice;
    return id;
}

void AudioSystem::stop(uint32_t voice) {
    if (!impl_) {
        return;
    }
    auto it = impl_->voices.find(voice);
    if (it == impl_->voices.end()) {
        return;
    }
    ma_sound_stop(it->second);
    ma_sound_uninit(it->second);
    delete it->second;
    impl_->voices.erase(it);
}

void AudioSystem::stopAll() {
    if (!impl_) {
        return;
    }
    for (auto& entry : impl_->voices) {
        ma_sound_stop(entry.second);
        ma_sound_uninit(entry.second);
        delete entry.second;
    }
    impl_->voices.clear();
}

void AudioSystem::setMasterVolume(float volume) {
    impl_->masterVolume = volume;
    if (ready()) {
        ma_engine_set_volume(&impl_->engine, volume);
    }
}

float AudioSystem::masterVolume() const { return impl_->masterVolume; }

size_t AudioSystem::activeVoiceCount() const {
    if (!impl_) {
        return 0;
    }
    size_t count = 0;
    for (const auto& entry : impl_->voices) {
        if (!ma_sound_at_end(entry.second)) {
            ++count;
        }
    }
    return count;
}

const std::string& AudioSystem::error() const { return impl_->error; }

}  // namespace ply
