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

#include "plyEngine/App.hpp"
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
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// 6 cells: p1, p2, tile, coin, saw, flag.
std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 6;
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

    fill(0, 90, 200, 255);    // p1
    fill(1, 255, 160, 60);    // p2
    fill(2, 90, 110, 120);    // tile
    fill(3, 250, 210, 60);    // coin
    fill(4, 220, 70, 70);     // saw
    fill(5, 90, 220, 120);    // flag
    return pixels;
}

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    const char* screenshot = nullptr;
    bool verify = false;
    bool enableMdns = true;
    int port = 0;
    std::string webRoot = "companion/dist";

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
    atlas->addRegion("tile", 64, 0, 32, 32);
    atlas->addRegion("coin", 96, 0, 32, 32);
    atlas->addRegion("saw", 128, 0, 32, 32);
    atlas->addRegion("flag", 160, 0, 32, 32);
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
    std::array<uint32_t, 2> synthSeq{0, 0};

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
            addSprite(game.players[i].entity, i == 0 ? "p1" : "p2",
                      glm::vec2{28.0f, 28.0f});
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
            text->draw(batch,
                       "players: " + std::to_string(server.connectedCount()) + "/" +
                           std::to_string(serverConfig.slotCount) +
                           (server.mdnsActive() ? "  (mDNS discoverable)" : ""),
                       topLeft - glm::vec2{0.0f, 76.0f},
                       glm::vec4{0.6f, 1.0f, 0.75f, 1.0f}, 1.0f);

            // Scores, one coloured run per player.
            const std::string s1 = "P1  " + std::to_string(game.players[0].score);
            const std::string s2 = "P2  " + std::to_string(game.players[1].score);
            const glm::vec2 scorePos = topLeft - glm::vec2{0.0f, 124.0f};
            text->draw(batch, s1, scorePos, glm::vec4{0.45f, 0.78f, 1.0f, 1.0f}, 1.4f);
            text->draw(batch, s2, scorePos + glm::vec2{text->measure(s1, 1.4f) + 40.0f, 0.0f},
                       glm::vec4{1.0f, 0.75f, 0.4f, 1.0f}, 1.4f);

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
    };

    app.onRender(renderScene);

    // The scripted run needs a full playthrough; default higher under verify.
    const int frameCount = frames > 0 ? frames : (verify ? 1800 : 120);
    app.runFrames(frameCount);

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
        const demo::Player& p2 = game.players[1];
        const ply::Transform* p2Transform = app.world().get<ply::Transform>(p2.entity);
        const float p2x = p2Transform ? p2Transform->position.x : p2.spawn.x;
        float sawTravel = 0.0f;
        for (ply::Entity saw : game.hazards) {
            if (const demo::Hazard* hazard = app.world().get<demo::Hazard>(saw)) {
                sawTravel += hazard->travelled;
            }
        }

        std::printf(
            "companion demo: %d frames, room %s, %ld lit pixels\n"
            "  outcome: state=%s winner=%d p1.score=%d p1.deaths=%d p2.x=%.0f "
            "(spawn %.0f) saw.travel=%.0f coins=%d/%d autopilot=%s\n",
            app.framesRendered(), server.roomCode().c_str(), lit,
            game.state == demo::GameState::Won ? "Won"
                : game.state == demo::GameState::Paused ? "Paused" : "Playing",
            game.winner, p1.score, p1.deaths, p2x, p2.spawn.x, sawTravel,
            game.coinsCollected, game.initialCoinCount,
            autopilot.stuck ? "STUCK" : "done");

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
            check(p2x < p2.spawn.x - 60.0f, "player 2 moved independently");
            check(sawTravel > 100.0f, "hazards patrolled");
            check(game.coinsCollected >= 2, "coins were removed from play");
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

    server.stop();
    return exitCode;
}
