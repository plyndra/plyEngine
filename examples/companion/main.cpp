// M6 demo: the platformer is driven by a phone controller.
//
// The window shows the room code and the LAN URL. Phones load the companion PWA
// from this host and control the player. Network threads only push input; the
// fixed-step update drains it, exactly as designed.

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

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kMapW = 40;
constexpr int kMapH = 18;
constexpr float kTile = 32.0f;

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

std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 4;
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

    fill(0, 90, 200, 255);   // player
    fill(1, 210, 170, 90);   // crate
    fill(2, 90, 110, 120);   // tile
    return pixels;
}

struct Scene {
    ply::Tilemap* map = nullptr;
    ply::Entity player{};
    std::vector<ply::Entity> crates;
    ply::CompanionServer* server = nullptr;
};

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
    config.window.title = "plyEngine - companion";
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
    std::printf("  room code: %s\n", server.roomCode().c_str());
    std::printf("  open:      %s\n", server.url().c_str());
    std::printf("  mDNS:      %s\n", server.mdnsActive() ? "advertising" : "off");
    std::printf("========================================\n");

    int atlasSize = 0;
    std::vector<unsigned char> atlasPixels = buildAtlas(atlasSize);
    auto atlas = std::make_unique<ply::SpriteAtlas>(atlasSize, atlasSize,
                                                    atlasPixels.data(),
                                                    ply::TextureFilter::Nearest);
    atlas->addRegion("player", 0, 0, 32, 32);
    atlas->addRegion("crate", 32, 0, 32, 32);
    atlas->addRegion("tile", 64, 0, 32, 32);
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
    ply::Tilemap map(kMapW, kMapH, kTile);
    Scene scene;
    scene.map = &map;
    scene.server = &server;

    ply::PhysicsSettings settings;
    settings.gravity = {0.0f, -1600.0f};

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();

        for (int x = 0; x < kMapW; ++x) {
            map.setSolid(x, 0, true);
        }
        for (int y = 0; y < kMapH; ++y) {
            map.setSolid(0, y, true);
            map.setSolid(kMapW - 1, y, true);
        }
        for (int x = 5; x < 15; ++x) {
            map.setSolid(x, 6, true);
        }
        for (int x = 20; x < 30; ++x) {
            map.setSolid(x, 10, true);
        }

        scene.player = world.spawn();
        world.add<ply::Transform>(
            scene.player,
            ply::Transform{glm::vec2{160.0f, 160.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(scene.player, ply::GlobalTransform{});
        world.add<ply::Velocity>(scene.player, ply::Velocity{});
        world.add<ply::Body>(scene.player, ply::Body{glm::vec2{14.0f, 14.0f}});

        ply::Sprite sprite;
        sprite.texture = texture;
        sprite.region = atlas->region("player");
        sprite.size = glm::vec2{28.0f, 28.0f};
        world.add<ply::Sprite>(scene.player, sprite);

        for (int i = 0; i < 6; ++i) {
            const ply::Entity crate = world.spawn();
            world.add<ply::Transform>(
                crate, ply::Transform{glm::vec2{240.0f + static_cast<float>(i) * 130.0f,
                                                380.0f},
                                      0.0f, glm::vec2{1.0f, 1.0f}});
            world.add<ply::GlobalTransform>(crate, ply::GlobalTransform{});
            world.add<ply::Velocity>(crate, ply::Velocity{});
            world.add<ply::Body>(crate, ply::Body{glm::vec2{12.0f, 12.0f}});

            ply::Sprite crateSprite;
            crateSprite.texture = texture;
            crateSprite.region = atlas->region("crate");
            crateSprite.size = glm::vec2{24.0f, 24.0f};
            world.add<ply::Sprite>(crate, crateSprite);
            scene.crates.push_back(crate);
        }

        a.camera().setPosition(glm::vec2{kMapW * kTile * 0.5f, kMapH * kTile * 0.5f});
        a.camera().setZoom(0.75f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        ply::World& world = a.world();
        const float fdt = static_cast<float>(dt);

        // Drain network input at the fixed-step boundary.
        server.update(monotonicSeconds());

        // Local keyboard still works (useful without a phone).
        if (ply::Velocity* v = world.get<ply::Velocity>(scene.player)) {
            float target = 0.0f;
            bool jump = false;

            // Phone (slot 0..3): any connected slot can drive.
            for (int i = 0; i < serverConfig.slotCount; ++i) {
                const ply::companion::ControllerState s = server.input().state(i);
                if (s.x != 0.0f) {
                    target = s.x * 320.0f;
                }
                if (server.input().pressed(i, ply::companion::kButtonA)) {
                    jump = true;
                }
            }
            // Keyboard fallback.
            if (a.input().isKeyDown(ply::Key::Left) || a.input().isKeyDown(ply::Key::A)) {
                target = -320.0f;
            }
            if (a.input().isKeyDown(ply::Key::Right) || a.input().isKeyDown(ply::Key::D)) {
                target = 320.0f;
            }
            if (a.input().isKeyPressed(ply::Key::Space)) {
                jump = true;
            }

            v->value.x = target;

            const ply::Body* body = world.get<ply::Body>(scene.player);
            if (jump && body && body->grounded) {
                v->value.y = 620.0f;
            }
        }

        ply::stepPhysics(world, map, fdt, settings);
        ply::updateTransforms(world);
    });

    auto renderScene = [&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);
        ply::World& world = a.world();

        batch.begin(*texture, a.camera().viewProj());
        for (int y = 0; y < kMapH; ++y) {
            for (int x = 0; x < kMapW; ++x) {
                if (!map.isSolid(x, y)) {
                    continue;
                }
                batch.draw(glm::vec2{(static_cast<float>(x) + 0.5f) * kTile,
                                     (static_cast<float>(y) + 0.5f) * kTile},
                           glm::vec2{kTile, kTile}, atlas->region("tile"));
            }
        }
        batch.end();

        ply::drawSprites(world, batch, a.camera().viewProj());

        if (text) {
            batch.begin(text->texture(), a.camera().viewProj());

            // Anchor the HUD to the visible top-left corner. Hard-coded world
            // coordinates would drift off-screen at other zooms/positions.
            const glm::vec2 cam = a.camera().position();
            const float zoom = a.camera().zoom();
            const float halfW = static_cast<float>(a.window().width()) / (2.0f * zoom);
            const float halfH = static_cast<float>(a.window().height()) / (2.0f * zoom);
            const glm::vec2 topLeft{cam.x - halfW + 28.0f, cam.y + halfH - 40.0f};

            text->draw(batch, "ROOM  " + server.roomCode(), topLeft,
                       glm::vec4{1.0f, 1.0f, 0.6f, 1.0f}, 1.4f);
            text->draw(batch, server.url(), topLeft - glm::vec2{0.0f, 46.0f},
                       glm::vec4{0.75f, 0.85f, 1.0f, 1.0f}, 1.0f);
            text->draw(batch,
                       "players: " + std::to_string(server.connectedCount()) + "/" +
                           std::to_string(serverConfig.slotCount) +
                           (server.mdnsActive() ? "  (mDNS discoverable)" : ""),
                       topLeft - glm::vec2{0.0f, 82.0f}, glm::vec4{0.6f, 1.0f, 0.75f, 1.0f},
                       1.0f);
            batch.end();
        }
    };

    app.onRender(renderScene);
    app.runFrames(frames > 0 ? frames : 120);

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

        std::printf("companion demo: %d frames, room %s, %ld lit pixels\n",
                    app.framesRendered(), server.roomCode().c_str(), lit);

        if (verify) {
            if (lit < 10000) {
                std::fprintf(stderr, "VERIFY FAIL: too few pixels drawn (%ld)\n", lit);
                exitCode = 1;
            } else if (server.roomCode().size() != 4) {
                std::fprintf(stderr, "VERIFY FAIL: no room code\n");
                exitCode = 1;
            } else if (!server.running()) {
                std::fprintf(stderr, "VERIFY FAIL: server is not running\n");
                exitCode = 1;
            } else {
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
