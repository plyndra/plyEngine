// Headless audio tests using the null backend. No GL context required.

#include "plyEngine/AudioSystem.hpp"

#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

}  // namespace

int main() {
    const std::string beep = std::string(PLY_TEST_DATA_DIR) + "/beep.wav";

    // --- a system that was never initialised is inert, not crashing -------
    {
        ply::AudioSystem audio;
        check(!audio.ready(), "uninitialised system is not ready");
        check(audio.loadSound(beep) == 0, "load on a dead system returns 0");
        check(audio.play(1) == 0, "play on a dead system returns 0");
        check(audio.activeVoiceCount() == 0, "no voices on a dead system");
        audio.stop(42);      // must not crash
        audio.stopAll();     // must not crash
        audio.setMasterVolume(0.5f);
        check(audio.masterVolume() == 0.5f, "master volume stored without a device");
    }

    // --- null-device init ------------------------------------------------
    {
        ply::AudioSystem audio;
        ply::AudioSystem::Config config;
        config.forceNullDevice = true;

        const bool ready = audio.init(config);
        if (!ready) {
            std::printf("  SKIP: null audio device unavailable (%s)\n",
                        audio.error().c_str());
        } else {
            check(audio.ready(), "null device reports ready");
            check(audio.activeVoiceCount() == 0, "no voices initially");

            // Decode a real WAV fixture.
            const uint32_t sound = audio.loadSound(beep);
            check(sound != 0, "WAV fixture decodes");
            if (sound == 0) {
                std::printf("    error: %s\n", audio.error().c_str());
            }

            if (sound != 0) {
                const uint32_t voice = audio.play(sound, 0.5f, 1.0f, 0.0f, false);
                check(voice != 0, "play returns a voice id");

                // The same decoded sound can be played more than once.
                const uint32_t voice2 = audio.play(sound, 0.25f, 1.5f, 0.5f, false);
                check(voice2 != 0, "second voice for the same sound");
                check(voice != voice2, "voice ids are distinct");

                audio.stop(voice);
                audio.stopAll();
                check(audio.activeVoiceCount() == 0, "stopAll clears every voice");

                audio.unloadSound(sound);
            }

            // A missing file fails cleanly.
            const uint32_t missing = audio.loadSound(
                std::string(PLY_TEST_DATA_DIR) + "/not_a_sound.wav");
            check(missing == 0, "missing audio file returns 0");
            check(!audio.error().empty(), "missing audio file sets an error");

            // A non-audio file fails cleanly.
            const uint32_t notAudio = audio.loadSound(
                std::string(PLY_TEST_DATA_DIR) + "/test_sheet.json");
            check(notAudio == 0, "non-audio file returns 0");

            audio.setMasterVolume(0.25f);
            check(audio.masterVolume() == 0.25f, "master volume round-trips");
        }

        audio.shutdown();
        check(!audio.ready(), "shutdown clears readiness");
    }

    // --- shutdown is idempotent ------------------------------------------
    {
        ply::AudioSystem audio;
        ply::AudioSystem::Config config;
        config.forceNullDevice = true;
        audio.init(config);
        audio.shutdown();
        audio.shutdown();
        audio.shutdown();
        check(!audio.ready(), "repeated shutdown stays safe");
    }

    if (failures == 0) {
        std::printf("audio tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "audio tests: %d failure(s)\n", failures);
    return 1;
}
