// M5 demo: tilemap collision, per-axis swept physics, and a debug overlay.
//
// Controls: arrows / WASD to drive the player, F1 toggles the debug draw.
// Run with --verify to assert the scene actually rendered (headless CI).

#include "plyEngine/App.hpp"
#include "plyEngine/DebugDrawer.hpp"
#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Sprite.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Tilemap.hpp"
#include "plyEngine/Transform.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr int kMapW = 40;
constexpr int kMapH = 20;
constexpr float kTile = 32.0f;

// Builds a small procedural atlas: 0 = player, 1 = crate, 2 = tile.
std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 4;
    std::vector<unsigned char> pixels(static_cast<size_t>(outSize) * outSize * 4, 0);

    auto fill = [&](int index, int r, int g, int b) {
        const int cx = index * cell;
        for (int y = 0; y < cell; ++y) {
            for (int x = 0; x < cell; ++x) {
                const int px = cx + x;
                const int i = (y * outSize + px) * 4;
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
    bool debugDraw = false;
};

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    const char* screenshot = nullptr;
    bool verify = false;
    bool debugStart = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--verify") == 0) {
            verify = true;
        } else if (std::strcmp(argv[i], "--debug") == 0) {
            debugStart = true;
        }
    }

    ply::App::Config config;
    config.window.title = "plyEngine - physics";
    config.window.width = 1280;
    config.window.height = 720;

    ply::App app(config);
    if (!app.valid()) {
        std::fprintf(stderr, "app invalid: %s\n", app.error());
        return 1;
    }

    int atlasSize = 0;
    std::vector<unsigned char> atlasPixels = buildAtlas(atlasSize);
    auto atlas = std::make_unique<ply::SpriteAtlas>(atlasSize, atlasSize,
                                                    atlasPixels.data(),
                                                    ply::TextureFilter::Nearest);
    atlas->addRegion("player", 0, 0, 32, 32);
    atlas->addRegion("crate", 32, 0, 32, 32);
    atlas->addRegion("tile", 64, 0, 32, 32);
    auto texture = atlas->texturePtr();

    ply::SpriteBatch batch;
    ply::DebugDrawer debug;
    ply::Tilemap map(kMapW, kMapH, kTile);
    Scene scene;
    scene.map = &map;
    scene.debugDraw = debugStart;

    ply::PhysicsSettings settings;
    settings.gravity = {0.0f, -1600.0f};

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();

        // --- level: floor, walls and two platforms ------------------------
        for (int x = 0; x < kMapW; ++x) {
            map.setSolid(x, 0, true);
        }
        for (int y = 0; y < kMapH; ++y) {
            map.setSolid(0, y, true);
            map.setSolid(kMapW - 1, y, true);
        }
        for (int x = 4; x < 14; ++x) {
            map.setSolid(x, 6, true);
        }
        for (int x = 18; x < 28; ++x) {
            map.setSolid(x, 10, true);
        }
        for (int y = 1; y < 5; ++y) {
            map.setSolid(24, y, true);
        }

        // --- player -------------------------------------------------------
        scene.player = world.spawn();
        world.add<ply::Transform>(
            scene.player,
            ply::Transform{glm::vec2{200.0f, 200.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(scene.player, ply::GlobalTransform{});
        world.add<ply::Velocity>(scene.player, ply::Velocity{});
        world.add<ply::Body>(scene.player, ply::Body{glm::vec2{14.0f, 14.0f}});

        ply::Sprite playerSprite;
        playerSprite.texture = texture;
        playerSprite.region = atlas->region("player");
        playerSprite.size = glm::vec2{28.0f, 28.0f};
        world.add<ply::Sprite>(scene.player, playerSprite);

        // --- crates falling onto the level --------------------------------
        for (int i = 0; i < 12; ++i) {
            const ply::Entity crate = world.spawn();
            const float x = 180.0f + static_cast<float>(i % 6) * 150.0f;
            const float y = 420.0f + static_cast<float>(i / 6) * 120.0f;

            world.add<ply::Transform>(
                crate, ply::Transform{glm::vec2{x, y}, 0.0f, glm::vec2{1.0f, 1.0f}});
            world.add<ply::GlobalTransform>(crate, ply::GlobalTransform{});
            world.add<ply::Velocity>(crate, ply::Velocity{});
            world.add<ply::Body>(crate, ply::Body{glm::vec2{12.0f, 12.0f}});

            ply::Sprite sprite;
            sprite.texture = texture;
            sprite.region = atlas->region("crate");
            sprite.size = glm::vec2{24.0f, 24.0f};
            world.add<ply::Sprite>(crate, sprite);
            scene.crates.push_back(crate);
        }

        // Camera frames the level.
        a.camera().setPosition(glm::vec2{kMapW * kTile * 0.5f, kMapH * kTile * 0.4f});
        a.camera().setZoom(0.75f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        ply::World& world = a.world();
        const float fdt = static_cast<float>(dt);

        // Player control writes velocity; physics does the rest.
        if (ply::Velocity* v = world.get<ply::Velocity>(scene.player)) {
            const float speed = 320.0f;
            float target = 0.0f;
            if (a.input().isKeyDown(ply::Key::Left) || a.input().isKeyDown(ply::Key::A)) {
                target -= speed;
            }
            if (a.input().isKeyDown(ply::Key::Right) || a.input().isKeyDown(ply::Key::D)) {
                target += speed;
            }
            v->value.x = target;
        }

        const ply::Body* playerBody = world.get<ply::Body>(scene.player);
        if (playerBody && playerBody->grounded) {
            if (a.input().isKeyPressed(ply::Key::Space) ||
                a.input().isKeyPressed(ply::Key::Up) ||
                a.input().isKeyPressed(ply::Key::W)) {
                if (ply::Velocity* v = world.get<ply::Velocity>(scene.player)) {
                    v->value.y = 620.0f;
                }
            }
        }

        if (a.input().isKeyPressed(ply::Key::F1)) {
            scene.debugDraw = !scene.debugDraw;
        }

        // Keep the player from drifting out of the level.
        (void)fdt;
        ply::stepPhysics(world, map, fdt, settings);
        ply::updateTransforms(world);
    });

    auto renderScene = [&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);
        ply::World& world = a.world();

        // Level tiles.
        batch.begin(*texture, a.camera().viewProj());
        for (int y = 0; y < kMapH; ++y) {
            for (int x = 0; x < kMapW; ++x) {
                if (!map.isSolid(x, y)) {
                    continue;
                }
                const glm::vec2 center{
                    (static_cast<float>(x) + 0.5f) * kTile,
                    (static_cast<float>(y) + 0.5f) * kTile,
                };
                batch.draw(center, glm::vec2{kTile, kTile}, atlas->region("tile"));
            }
        }
        batch.end();

        // Entities.
        ply::drawSprites(world, batch, a.camera().viewProj());

        // Debug overlay.
        if (scene.debugDraw) {
            debug.begin(batch, a.camera().viewProj());

            // Collider outlines.
            world.each<ply::Body, ply::Transform>(
                [&](ply::Entity, ply::Body& body, ply::Transform& transform) {
                    ply::Aabb box = ply::bodyAabb(&transform, body);
                    const glm::vec4 color = body.grounded
                                                ? glm::vec4{0.3f, 1.0f, 0.5f, 1.0f}
                                                : glm::vec4{1.0f, 0.85f, 0.3f, 1.0f};
                    debug.drawBox(box, color, 1.5f);
                    debug.drawPoint(box.center(), 3.0f, glm::vec4{1.0f, 0.3f, 0.3f, 1.0f});
                });

            // Tile grid + solid cell fill.
            debug.drawGrid(ply::Aabb{{0.0f, 0.0f},
                                     {kMapW * kTile, kMapH * kTile}},
                           kTile, glm::vec4{0.25f, 0.35f, 0.55f, 0.5f}, 1.0f);

            debug.end();
        }
    };

    app.onRender(renderScene);
    // Interactive by default: a bounded default ran the whole demo in well
    // under a second and closed the window before anyone could look at it.
    const bool automated = verify || screenshot != nullptr;
    if (frames > 0 || automated) {
        app.runFrames(frames > 0 ? frames : 90);
    } else {
        app.run();
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

        int grounded = 0;
        for (const ply::Entity crate : scene.crates) {
            const ply::Body* body = app.world().get<ply::Body>(crate);
            if (body && body->grounded) {
                ++grounded;
            }
        }

        std::printf("physics demo: %d frames, %ld lit pixels, %d/%zu crates resting\n",
                    app.framesRendered(), lit, grounded, scene.crates.size());

        if (verify) {
            if (lit < 10000) {
                std::fprintf(stderr, "VERIFY FAIL: too few pixels drawn (%ld)\n", lit);
                exitCode = 1;
            } else if (grounded == 0) {
                std::fprintf(stderr, "VERIFY FAIL: no crate came to rest\n");
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

    return exitCode;
}
