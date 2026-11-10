// M7 reference demo: a full single-screen platformer driven by phone
// controllers (or two keyboard players). Two players, coins with scoring,
// patrolling saws, a pit, a staircase and a goal flag — the vertical slice
// that exercises the whole engine end to end.
//
// Controls: P1 = arrows + Space, P2 = A / D / W, P or START = pause,
// R = restart. Phones join the room and drive slots 0/1.
//
// --verify plays the level with a scripted Autopilot *through the real
// CompanionInput queue* and asserts gameplay outcomes: coins collected,
// deaths to the pit and the saw, player 2 moving independently, and the
// game reaching a win for player 1. Not just "pixels were drawn".
//
// --dump-frames <dir> writes every Nth rendered frame as a PPM sequence, so a
// scripted run can be encoded into the docs/announcements hero animation. The
// capture is labelled wherever it is published as a *scripted four-slot run*
// driven through the phone input path — not as four physical phones.

#include "plyEngine/App.hpp"
#include "plyEngine/AudioSystem.hpp"
#include "plyEngine/CompanionServer.hpp"
#include "plyEngine/Font.hpp"
#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Sprite.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/TextRenderer.hpp"
#include "plyEngine/Tilemap.hpp"
#include "plyEngine/Transform.hpp"
#include "plyEngine/Version.hpp"

#include "platformer.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* kFontCandidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
};

double monotonicSeconds() {
    using Clock = std::chrono::steady_clock;
    static const Clock::time_point epoch = Clock::now();
    return std::chrono::duration<double>(Clock::now() - epoch).count();
}

// 8 cells: p1..p4, tile, coin, saw, flag.
std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 8;
    std::vector<unsigned char> pixels(static_cast<size_t>(outSize) * outSize * 4, 0);

    auto fill = [&](int index, int r, int g, int b) {
        const int cx = index * cell;
        for (int y = 0; y < cell; ++y) {
            for (int x = 0; x < cell; ++x) {
                const int i = (y * outSize + (cx + x)) * 4;
                const bool edge = x < 2 || y < 2 || x >= cell - 2 || y >= cell - 2;
                const float shade = 0.75f + 0.25f * (1.0f - static_cast<float>(y) / cell);
                pixels[i + 0] = static_cast<unsigned char>((edge ? 30 : r) * (edge ? 1.0f : shade));
                pixels[i + 1] = static_cast<unsigned char>((edge ? 30 : g) * (edge ? 1.0f : shade));
                pixels[i + 2] = static_cast<unsigned char>((edge ? 30 : b) * (edge ? 1.0f : shade));
                pixels[i + 3] = 255;
            }
        }
    };

    fill(0, 90, 200, 255);    // p1 blue
    fill(1, 255, 160, 60);    // p2 orange
    fill(2, 235, 90, 190);    // p3 magenta
    fill(3, 90, 220, 120);    // p4 green
    fill(4, 90, 110, 120);    // tile
    fill(5, 250, 210, 60);    // coin
    fill(6, 220, 70, 70);     // saw
    fill(7, 140, 200, 255);   // flag
    return pixels;
}

// --- Audio ------------------------------------------------------------------

// The demo ships no binary assets: the three sound effects are synthesised as
// tiny WAV files at startup (written to the temp directory and loaded back
// through ply::AudioSystem). Keeps --verify hermetic and the archive small.
bool writeToneWav(const std::filesystem::path& path, float frequencyHz,
                  float seconds, float decay) {
    constexpr int kRate = 22050;
    const int frames = static_cast<int>(kRate * seconds);
    std::vector<int16_t> pcm(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / kRate;
        const float envelope = std::exp(-decay * t);
        const float sample = std::sin(2.0f * 3.14159265f * frequencyHz * t) * envelope;
        pcm[static_cast<size_t>(i)] = static_cast<int16_t>(sample * 9000.0f);
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    const uint32_t dataBytes = static_cast<uint32_t>(pcm.size() * sizeof(int16_t));
    auto u32 = [&](uint32_t v) {
        char b[4] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF),
                     static_cast<char>((v >> 16) & 0xFF), static_cast<char>((v >> 24) & 0xFF)};
        out.write(b, 4);
    };
    auto u16 = [&](uint16_t v) {
        char b[2] = {static_cast<char>(v & 0xFF), static_cast<char>((v >> 8) & 0xFF)};
        out.write(b, 2);
    };
    out.write("RIFF", 4);
    u32(36 + dataBytes);
    out.write("WAVEfmt ", 8);
    u32(16);            // PCM chunk size
    u16(1);             // format = PCM
    u16(1);             // channels = mono
    u32(kRate);         // sample rate
    u32(kRate * 2);     // byte rate
    u16(2);             // block align
    u16(16);            // bits per sample
    out.write("data", 4);
    u32(dataBytes);
    out.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
    return out.good();
}

// One frame as a binary PPM (P6), top row first. PPM because it is the only
// image format that needs no library; the capture pipeline converts the
// sequence with ffmpeg.
bool writePpmFrame(const std::filesystem::path& path, int w, int h,
                   const std::vector<unsigned char>& rgb) {
    std::FILE* f = std::fopen(path.string().c_str(), "wb");
    if (!f) {
        return false;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) {
        std::fwrite(&rgb[static_cast<size_t>(y) * w * 3], 1,
                    static_cast<size_t>(w) * 3, f);
    }
    std::fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    const char* screenshot = nullptr;
    bool verify = false;
    bool enableMdns = true;
    int port = 0;
    // Capture state for --dump-frames (see the file header).
    const char* dumpFramesDir = nullptr;
    int dumpStride = 6;
    int dumpFrom = 0;
    int dumpCount = 0;  // 0 = no cap
    int dumpedFrames = 0;
    // Resolved after argument parsing: an explicit --web wins, otherwise prefer
    // the source-tree build output and fall back to the release archive layout.
    std::string webRoot;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--verify") == 0) {
            verify = true;
        } else if (std::strcmp(argv[i], "--no-mdns") == 0) {
            enableMdns = false;
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--web") == 0 && i + 1 < argc) {
            webRoot = argv[++i];
        } else if (std::strcmp(argv[i], "--dump-frames") == 0 && i + 1 < argc) {
            dumpFramesDir = argv[++i];
        } else if (std::strcmp(argv[i], "--dump-stride") == 0 && i + 1 < argc) {
            dumpStride = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--dump-from") == 0 && i + 1 < argc) {
            dumpFrom = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--dump-count") == 0 && i + 1 < argc) {
            dumpCount = std::atoi(argv[++i]);
        }
    }

    if (dumpFramesDir) {
        std::error_code ec;
        std::filesystem::create_directories(dumpFramesDir, ec);
        if (ec) {
            std::fprintf(stderr, "capture: cannot create %s: %s\n", dumpFramesDir,
                         ec.message().c_str());
            return 1;
        }
    }

    ply::App::Config config;
    config.window.title = "plyEngine - platformer";
    config.window.width = 1280;
    config.window.height = 720;

    ply::App app(config);
    if (!app.valid()) {
        std::fprintf(stderr, "app invalid: %s\n", app.error());
        return 1;
    }

    if (webRoot.empty()) {
        // A source checkout builds the PWA into companion/dist (Vite output,
        // gitignored); the release archive ships it directly in companion/.
        // Pick whichever actually has an index.html.
        webRoot = ply::CompanionServer::resolveWebRoot(
            {"companion/dist", "companion"});
    }

    ply::CompanionServer server;
    ply::CompanionServer::Config serverConfig;
    serverConfig.port = port;
    serverConfig.slotCount = 4;
    serverConfig.reconnectGrace = 20.0;
    serverConfig.webRoot = webRoot;
    serverConfig.enableMdns = enableMdns;
    serverConfig.instanceName = "plyEngine";

    if (!server.start(serverConfig)) {
        std::fprintf(stderr, "companion server failed to start\n");
        return 1;
    }

    std::printf("========================================\n");
    std::printf("  plyEngine %s - platformer demo\n", ply::kVersion);
    std::printf("  room code: %s\n", server.roomCode().c_str());
    std::printf("  open:      %s\n", server.url().c_str());
    std::printf("  mDNS:      %s\n", server.mdnsActive() ? "advertising" : "off");
    std::printf("========================================\n");

    int atlasSize = 0;
    std::vector<unsigned char> atlasPixels = buildAtlas(atlasSize);
    auto atlas = std::make_unique<ply::SpriteAtlas>(atlasSize, atlasSize,
                                                    atlasPixels.data(),
                                                    ply::TextureFilter::Nearest);
    atlas->addRegion("p1", 0, 0, 32, 32);
    atlas->addRegion("p2", 32, 0, 32, 32);
    atlas->addRegion("p3", 64, 0, 32, 32);
    atlas->addRegion("p4", 96, 0, 32, 32);
    atlas->addRegion("tile", 128, 0, 32, 32);
    atlas->addRegion("coin", 160, 0, 32, 32);
    atlas->addRegion("saw", 192, 0, 32, 32);
    atlas->addRegion("flag", 224, 0, 32, 32);
    auto texture = atlas->texturePtr();

    ply::Font font;
    bool haveFont = false;
    for (const char* path : kFontCandidates) {
        if (font.loadFromFile(path, 28)) {
            haveFont = true;
            break;
        }
    }
    std::unique_ptr<ply::TextRenderer> text;
    if (haveFont) {
        text = std::make_unique<ply::TextRenderer>(font);
    }

    ply::SpriteBatch batch;
    demo::Game game;
    demo::Autopilot autopilot;
    std::array<uint32_t, demo::kPlayerCount> synthSeq{};

    // --- Audio: synthesise three cues, then play them on gameplay events ----
    ply::AudioSystem::Config audioConfig;
    // Headless verify must not require (or fight over) a real device.
    audioConfig.forceNullDevice = verify;
    ply::AudioSystem audio;
    const bool audioReady = audio.init(audioConfig);

    std::array<uint32_t, 4> cueSound{};  // indexed by demo::Sfx
    {
        const std::filesystem::path dir =
            std::filesystem::temp_directory_path() / "ply_companion_sfx";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        struct Cue {
            const char* file;
            float hz;
            float seconds;
            float decay;
        };
        // Index order must match demo::Sfx {Jump, Coin, Death, Win}.
        const Cue cues[4] = {{"jump.wav", 660.0f, 0.10f, 18.0f},
                             {"coin.wav", 990.0f, 0.09f, 14.0f},
                             {"death.wav", 180.0f, 0.22f, 8.0f},
                             {"win.wav", 1320.0f, 0.28f, 6.0f}};
        if (audioReady) {
            for (int i = 0; i < 4; ++i) {
                const std::filesystem::path path = dir / cues[i].file;
                if (writeToneWav(path, cues[i].hz, cues[i].seconds, cues[i].decay)) {
                    cueSound[i] = audio.loadSound(path.string());
                }
            }
        }
    }
    size_t soundsLoaded = 0;
    for (uint32_t id : cueSound) {
        if (id != 0) {
            ++soundsLoaded;
        }
    }
    size_t voicesPlayed = 0;
    constexpr size_t kCueCount = 4;  // demo::Sfx {Jump, Coin, Death, Win}
    int cueCounts[kCueCount] = {0, 0, 0, 0};

    // Attaches render sprites to whatever entities the game currently has.
    // Called after build() and after every reset().
    auto decorate = [&](ply::World& world) {
        auto addSprite = [&](ply::Entity entity, const char* region, glm::vec2 size) {
            ply::Sprite sprite;
            sprite.texture = texture;
            sprite.region = atlas->region(region);
            sprite.size = size;
            world.add<ply::Sprite>(entity, sprite);
        };
        for (int i = 0; i < demo::kPlayerCount; ++i) {
            const char* regions[demo::kPlayerCount] = {"p1", "p2", "p3", "p4"};
            addSprite(game.players[i].entity, regions[i], glm::vec2{28.0f, 28.0f});
        }
        for (ply::Entity coin : game.coins) {
            addSprite(coin, "coin", glm::vec2{24.0f, 24.0f});
        }
        for (ply::Entity saw : game.hazards) {
            addSprite(saw, "saw", glm::vec2{26.0f, 26.0f});
        }
        addSprite(game.goal, "flag", glm::vec2{36.0f, 44.0f});
    };

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();
        game.build(world);
        decorate(world);

        a.camera().setPosition(glm::vec2{demo::kMapW * demo::kTile * 0.5f,
                                         demo::kMapH * demo::kTile * 0.5f});
        a.camera().setZoom(1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        ply::World& world = a.world();
        const float fdt = static_cast<float>(dt);

        // Verify mode: the Autopilot synthesises controller states and pushes
        // them through the real CompanionInput queue, so the scripted run
        // exercises the same slot-routing path a phone does.
        if (verify) {
            const std::array<demo::PlayerInput, demo::kPlayerCount> scripted =
                autopilot.update(game, world);
            for (int i = 0; i < demo::kPlayerCount; ++i) {
                ply::companion::ControllerState state;
                state.x = scripted[i].x;
                if (scripted[i].jumpPressed) {
                    state.buttons = ply::companion::kButtonA;
                }
                server.input().push(i, state, ++synthSeq[i]);
            }
        }

        // Drain network input at the fixed-step boundary.
        server.update(monotonicSeconds());

        // Assemble per-player input: phone slot i drives player i, with a
        // keyboard fallback so the demo is playable without any phones.
        std::array<demo::PlayerInput, demo::kPlayerCount> input{};
        for (int i = 0; i < demo::kPlayerCount; ++i) {
            const ply::companion::ControllerState state = server.input().state(i);
            if (std::fabs(state.x) > 0.01f) {
                input[i].x = state.x;
            }
            if (server.input().pressed(i, ply::companion::kButtonA)) {
                input[i].jumpPressed = true;
            }

            if (i == 0) {
                if (a.input().isKeyDown(ply::Key::Left)) input[i].x = -1.0f;
                if (a.input().isKeyDown(ply::Key::Right)) input[i].x = 1.0f;
                if (a.input().isKeyPressed(ply::Key::Space)) input[i].jumpPressed = true;
            } else {
                if (a.input().isKeyDown(ply::Key::A)) input[i].x = -1.0f;
                if (a.input().isKeyDown(ply::Key::D)) input[i].x = 1.0f;
                if (a.input().isKeyPressed(ply::Key::W)) input[i].jumpPressed = true;
            }
        }

        // Pause / restart: START on any phone, or P / R on the keyboard.
        bool startPressed = false;
        for (int i = 0; i < serverConfig.slotCount; ++i) {
            if (server.input().pressed(i, ply::companion::kButtonStart)) {
                startPressed = true;
            }
        }
        if (a.input().isKeyPressed(ply::Key::R)) {
            game.reset(world);
            decorate(world);
        } else if (game.state == demo::GameState::Won) {
            if (startPressed) {
                game.reset(world);
                decorate(world);
            }
        } else if (startPressed || a.input().isKeyPressed(ply::Key::P)) {
            game.togglePause();
        }

        game.step(world, input, fdt);

        // Drain the gameplay's sound cues into the audio system.
        for (demo::Sfx cue : game.sfx) {
            const size_t index = static_cast<size_t>(cue);
            if (index < kCueCount) {
                ++cueCounts[index];
                if (cueSound[index] != 0) {
                    if (audio.play(cueSound[index], 0.35f) != 0) {
                        ++voicesPlayed;
                    }
                }
            }
        }
        game.sfx.clear();
    });

    auto renderScene = [&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);
        ply::World& world = a.world();

        // Tiles + gameplay sprites share one atlas texture, so they batch
        // together: draw the static map, then let drawSprites append.
        batch.begin(*texture, a.camera().viewProj());
        for (int y = 0; y < demo::kMapH; ++y) {
            for (int x = 0; x < demo::kMapW; ++x) {
                if (!game.map.isSolid(x, y)) {
                    continue;
                }
                batch.draw(glm::vec2{(static_cast<float>(x) + 0.5f) * demo::kTile,
                                     (static_cast<float>(y) + 0.5f) * demo::kTile},
                           glm::vec2{demo::kTile, demo::kTile}, atlas->region("tile"));
            }
        }
        batch.end();

        // Invulnerable players blink.
        for (int i = 0; i < demo::kPlayerCount; ++i) {
            if (ply::Sprite* sprite =
                    world.get<ply::Sprite>(game.players[i].entity)) {
                const bool blink = game.players[i].invuln > 0.0f &&
                                   std::fmod(game.time * 10.0f, 1.0f) < 0.5f;
                sprite->color.a = blink ? 0.35f : 1.0f;
            }
        }

        ply::drawSprites(world, batch, a.camera().viewProj());

        if (text) {
            batch.begin(text->texture(), a.camera().viewProj());

            // Anchor the HUD to the visible top-left corner.
            const glm::vec2 cam = a.camera().position();
            const float zoom = a.camera().zoom();
            const float halfW = static_cast<float>(a.window().width()) / (2.0f * zoom);
            const float halfH = static_cast<float>(a.window().height()) / (2.0f * zoom);
            const glm::vec2 topLeft{cam.x - halfW + 24.0f, cam.y + halfH - 36.0f};

            text->draw(batch, "ROOM  " + server.roomCode(), topLeft,
                       glm::vec4{1.0f, 1.0f, 0.6f, 1.0f}, 1.3f);
            text->draw(batch, server.url(), topLeft - glm::vec2{0.0f, 40.0f},
                       glm::vec4{0.75f, 0.85f, 1.0f, 1.0f}, 1.0f);
            std::string playerLine =
                "players: " + std::to_string(server.connectedCount()) + "/" +
                std::to_string(serverConfig.slotCount) +
                (server.mdnsActive() ? "  (mDNS discoverable)" : "");
            if (dumpFramesDir) {
                // A capture run drives every slot from the autopilot through the
                // same input path a phone uses. Say that on screen rather than
                // let a viewer read four moving players as four joined phones.
                playerLine = "scripted capture: 4 slots via the phone input path";
            }
            text->draw(batch, playerLine, topLeft - glm::vec2{0.0f, 76.0f},
                       glm::vec4{0.6f, 1.0f, 0.75f, 1.0f}, 1.0f);

            // Scores: one coloured run per player, matching the sprite tint.
            const glm::vec4 kPlayerColors[demo::kPlayerCount] = {
                {0.45f, 0.78f, 1.0f, 1.0f},   // p1 blue
                {1.0f, 0.72f, 0.40f, 1.0f},   // p2 orange
                {0.95f, 0.55f, 0.82f, 1.0f},  // p3 magenta
                {0.55f, 0.92f, 0.60f, 1.0f},  // p4 green
            };
            glm::vec2 scorePos = topLeft - glm::vec2{0.0f, 122.0f};
            for (int i = 0; i < demo::kPlayerCount; ++i) {
                const std::string label =
                    "P" + std::to_string(i + 1) + "  " +
                    std::to_string(game.players[i].score);
                text->draw(batch, label, scorePos, kPlayerColors[i], 1.4f);
                scorePos.x += text->measure(label, 1.4f) + 34.0f;
            }

            // Game state line.
            std::string stateLine;
            glm::vec4 stateColor{0.85f, 0.9f, 1.0f, 1.0f};
            if (game.state == demo::GameState::Paused) {
                stateLine = "PAUSED - P or START to resume";
                stateColor = {1.0f, 0.85f, 0.5f, 1.0f};
            } else if (game.state == demo::GameState::Won) {
                stateLine = "P" + std::to_string(game.winner + 1) +
                            " WINS!  R or START to restart";
                stateColor = {0.55f, 1.0f, 0.65f, 1.0f};
            } else {
                stateLine = "reach the flag!   P1: arrows+space   P2: A/D/W"
 "   P: pause   R: restart";
            }
            text->draw(batch, stateLine, topLeft - glm::vec2{0.0f, 164.0f},
                       stateColor, 1.0f);

            batch.end();
        }

        // Optional capture: read back the finished frame and write it as PPM.
        // This lives inside the render callback because a frame only exists
        // here — the post-run screenshot path can only ever produce the last
        // one. Reads the back buffer that was just drawn, before the swap.
        if (dumpFramesDir) {
            const int frame = static_cast<int>(a.framesRendered());
            const int stride = dumpStride > 0 ? dumpStride : 1;
            // Stop at the winning frame: the level ends with a "P1 WINS" plate
            // that would otherwise hold for the rest of the run and pad the
            // animation with a still image.
            const bool inRange = game.state != demo::GameState::Won &&
                                 frame >= dumpFrom &&
                                 (dumpCount <= 0 || dumpedFrames < dumpCount);
            if (inRange && (frame - dumpFrom) % stride == 0) {
                const int w = a.window().width();
                const int h = a.window().height();
                std::vector<unsigned char> frameRgb(static_cast<size_t>(w) * h * 3);
                glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, frameRgb.data());
                char name[64];
                std::snprintf(name, sizeof(name), "frame_%05d.ppm", dumpedFrames);
                const std::filesystem::path out =
                    std::filesystem::path(dumpFramesDir) / name;
                if (writePpmFrame(out, w, h, frameRgb)) {
                    ++dumpedFrames;
                } else {
                    std::fprintf(stderr, "capture: cannot write %s\n",
                                 out.string().c_str());
                }
            }
        }
    };

    app.onRender(renderScene);

    // The scripted run needs a full playthrough. --verify therefore runs at
    // least kVerifyFrames regardless of --frames, so `--frames 60 --verify`
    // (the pre-M7 CI invocation) cannot fail for lack of simulated time.
    constexpr int kVerifyFrames = 1800;
    int frameCount = frames > 0 ? frames : 120;
    if (verify && frameCount < kVerifyFrames) {
        std::printf("verify: running %d frames (needs a full playthrough)\n",
                    kVerifyFrames);
        frameCount = kVerifyFrames;
    }
    // Interactive by default: this is the flagship path ("run it, open the URL
    // on your phone"), so no flags must mean "stay up until the window closes".
    // A bounded default here exited in under a second, which made every room
    // code printed on launch dead on arrival.
    const bool automated = verify || screenshot != nullptr || dumpFramesDir != nullptr;
    if (frames > 0 || automated) {
        app.runFrames(frameCount);
    } else {
        app.run();
    }

    if (dumpFramesDir) {
        std::printf("capture: wrote %d frame(s) to %s (stride %d)\n", dumpedFrames,
                    dumpFramesDir, dumpStride > 0 ? dumpStride : 1);
    }

    int exitCode = 0;
    if (verify || screenshot) {
        renderScene(app, 0.0);
        glFinish();

        const int w = app.window().width();
        const int h = app.window().height();
        std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

        const unsigned char* ref = rgb.data();
        long lit = 0;
        for (size_t i = 0; i < rgb.size(); i += 3) {
            if (rgb[i] != ref[0] || rgb[i + 1] != ref[1] || rgb[i + 2] != ref[2]) {
                ++lit;
            }
        }

        // Gameplay outcome measurements.
        const demo::Player& p1 = game.players[0];
        std::array<float, demo::kPlayerCount> displacement{};
        for (int i = 0; i < demo::kPlayerCount; ++i) {
            const ply::Transform* transform =
                app.world().get<ply::Transform>(game.players[i].entity);
            displacement[i] = transform
                ? std::fabs(transform->position.x - game.players[i].spawn.x)
                : 0.0f;
        }
        float sawTravel = 0.0f;
        for (ply::Entity saw : game.hazards) {
            if (const demo::Hazard* hazard = app.world().get<demo::Hazard>(saw)) {
                sawTravel += hazard->travelled;
            }
        }

        std::printf(
            "companion demo: %d frames, room %s, %ld lit pixels\n"
            "  outcome: state=%s winner=%d p1.score=%d p1.deaths=%d "
            "moved=[%.0f %.0f %.0f %.0f] saw.travel=%.0f coins=%d/%d autopilot=%s\n"
            "  audio: ready=%d device=%d loaded=%zu played=%zu cues=[%d %d %d %d]\n",
            app.framesRendered(), server.roomCode().c_str(), lit,
            game.state == demo::GameState::Won ? "Won"
                : game.state == demo::GameState::Paused ? "Paused" : "Playing",
            game.winner, p1.score, p1.deaths, displacement[0], displacement[1],
            displacement[2], displacement[3], sawTravel, game.coinsCollected,
            game.initialCoinCount, autopilot.stuck ? "STUCK" : "done",
            audio.ready() ? 1 : 0, audio.hasDevice() ? 1 : 0, soundsLoaded,
            voicesPlayed, cueCounts[0], cueCounts[1], cueCounts[2], cueCounts[3]);

        if (verify) {
            auto check = [&](bool ok, const char* what) {
                if (!ok) {
                    std::fprintf(stderr, "VERIFY FAIL: %s\n", what);
                    exitCode = 1;
                }
            };
            check(lit >= 10000, "scene rendered");
            check(server.roomCode().size() == 4, "room code present");
            check(server.running(), "server running");
            check(autopilot.phase == demo::Autopilot::Phase::Done && !autopilot.stuck,
                  "scripted run completed (not stuck)");
            check(game.state == demo::GameState::Won, "game reached a win state");
            check(game.winner == 0, "player 1 reached the goal first");
            check(p1.score >= 2, "player 1 collected coins");
            check(p1.deaths >= 2, "player 1 died to the pit and the saw");
            check(sawTravel > 100.0f, "hazards patrolled");
            check(game.coinsCollected >= 2, "coins were removed from play");

            // Four players, four independent input slots.
            check(demo::kPlayerCount == 4, "four players in the level");
            check(server.slots().size() == 4, "four controller slots");
            for (int i = 1; i < demo::kPlayerCount; ++i) {
                if (displacement[i] < 40.0f) {
                    std::fprintf(stderr,
                                 "VERIFY FAIL: player %d did not move independently "
                                 "(displaced %.1f)\n",
                                 i + 1, displacement[i]);
                    exitCode = 1;
                }
            }

            // Audio: the cues came from gameplay, and they were played.
            check(audio.ready(), "audio system initialised");
            check(soundsLoaded == 4, "all four sound cues decoded");
            check(cueCounts[0] > 0, "jump cue emitted");
            check(cueCounts[1] >= 2, "coin cues emitted");
            check(cueCounts[2] >= 2, "death cues emitted");
            check(cueCounts[3] == 1, "exactly one win cue");
            check(voicesPlayed >= 4, "audio voices actually played");
            if (exitCode == 0) {
                std::printf("verify: PASS\n");
            }
        }

        if (screenshot) {
            std::FILE* f = std::fopen(screenshot, "wb");
            if (f) {
                std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                for (int y = h - 1; y >= 0; --y) {
                    std::fwrite(&rgb[static_cast<size_t>(y) * w * 3], 1,
                                static_cast<size_t>(w) * 3, f);
                }
                std::fclose(f);
                std::printf("wrote %s\n", screenshot);
            }
        }
    }

    audio.shutdown();
    server.stop();
    return exitCode;
}
