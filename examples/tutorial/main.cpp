// Tutorial game: the complete "first game" the docs walk you through
// (docs/TUTORIAL.md). One player, a flat arena, five coins to collect.
//
// It deliberately uses only the core loop: App, sprites from an atlas,
// an archetype ECS world, the fixed-timestep physics and text. Phone
// controllers (M6) are covered in docs/GETTING_STARTED.md afterwards.
//
// Controls: arrows / WASD to move, Space to jump.
// Run with --verify to assert all five coins were actually collected
// (headless CI plays it with a built-in hold-right script).

#include "plyEngine/App.hpp"
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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr int kMapW = 30;
constexpr int kMapH = 14;
constexpr float kTile = 32.0f;
constexpr int kCoinCount = 5;

const char* kFontCandidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
};

// Procedural atlas: 0 = player, 1 = coin, 2 = tile.
std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 3;
    std::vector<unsigned char> pixels(static_cast<size_t>(outSize) * outSize * 4, 0);

    auto fill = [&](int index, int r, int g, int b) {
        const int cx = index * cell;
        for (int y = 0; y < cell; ++y) {
            for (int x = 0; x < cell; ++x) {
                const int i = (y * outSize + (cx + x)) * 4;
                const bool edge = x < 2 || y < 2 || x >= cell - 2 || y >= cell - 2;
                pixels[i + 0] = static_cast<unsigned char>(edge ? 30 : r);
                pixels[i + 1] = static_cast<unsigned char>(edge ? 30 : g);
                pixels[i + 2] = static_cast<unsigned char>(edge ? 30 : b);
                pixels[i + 3] = 255;
            }
        }
    };

    fill(0, 90, 200, 255);  // player
    fill(1, 250, 210, 60);  // coin
    fill(2, 90, 110, 120);  // tile
    return pixels;
}

struct Coin {};

struct Game {
    ply::Tilemap map{kMapW, kMapH, kTile};
    ply::Entity player{};
    std::vector<ply::Entity> coins;
    int score = 0;
    bool won = false;
};

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    const char* screenshot = nullptr;
    bool verify = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--verify") == 0) {
            verify = true;
        }
    }

    ply::App::Config config;
    config.window.title = "plyEngine - tutorial";
    config.window.width = 960;
    config.window.height = 540;

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
    atlas->addRegion("coin", 32, 0, 32, 32);
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
    Game game;

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();

        // Floor and walls: solid cells the physics can collide with.
        for (int x = 0; x < kMapW; ++x) {
            game.map.setSolid(x, 0, true);
        }
        for (int y = 0; y < kMapH; ++y) {
            game.map.setSolid(0, y, true);
            game.map.setSolid(kMapW - 1, y, true);
        }
        for (int x = 0; x < kMapW; ++x) {
            game.map.setSolid(x, 2, true);
        }

        // The player: transform + velocity + dynamic body + drawable sprite.
        game.player = world.spawn();
        world.add<ply::Transform>(
            game.player,
            ply::Transform{glm::vec2{96.0f, 110.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(game.player, ply::GlobalTransform{});
        world.add<ply::Velocity>(game.player, ply::Velocity{});
        world.add<ply::Body>(game.player, ply::Body{glm::vec2{14.0f, 14.0f}});
        ply::Sprite playerSprite;
        playerSprite.texture = texture;
        playerSprite.region = atlas->region("player");
        playerSprite.size = glm::vec2{28.0f, 28.0f};
        world.add<ply::Sprite>(game.player, playerSprite);

        // Five coins in a line, at walking height.
        const int coinCells[kCoinCount] = {8, 11, 14, 17, 20};
        for (int i = 0; i < kCoinCount; ++i) {
            const ply::Entity coin = world.spawn();
            world.add<ply::Transform>(
                coin, ply::Transform{glm::vec2{coinCells[i] * kTile + kTile * 0.5f,
                                               3.0f * kTile + 16.0f},
                                     0.0f, glm::vec2{1.0f, 1.0f}});
            world.add<ply::GlobalTransform>(coin, ply::GlobalTransform{});
            world.add<Coin>(coin, Coin{});
            ply::Sprite coinSprite;
            coinSprite.texture = texture;
            coinSprite.region = atlas->region("coin");
            coinSprite.size = glm::vec2{24.0f, 24.0f};
            world.add<ply::Sprite>(coin, coinSprite);
            game.coins.push_back(coin);
        }

        a.camera().setPosition(glm::vec2{kMapW * kTile * 0.5f, kMapH * kTile * 0.5f});
        a.camera().setZoom(1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        ply::World& world = a.world();
        const float fdt = static_cast<float>(dt);

        // --- input -----------------------------------------------------------
        float move = 0.0f;
        bool jump = false;
        if (verify) {
            // Headless script: hold right. The line of coins does the rest.
            move = 1.0f;
        } else {
            if (a.input().isKeyDown(ply::Key::Left) || a.input().isKeyDown(ply::Key::A)) {
                move = -1.0f;
            }
            if (a.input().isKeyDown(ply::Key::Right) || a.input().isKeyDown(ply::Key::D)) {
                move = 1.0f;
            }
            jump = a.input().isKeyPressed(ply::Key::Space);
        }

        // --- simulation ------------------------------------------------------
        if (ply::Velocity* velocity = world.get<ply::Velocity>(game.player)) {
            velocity->value.x = move * 320.0f;
            if (jump) {
                if (const ply::Body* body = world.get<ply::Body>(game.player)) {
                    if (body->grounded) {
                        velocity->value.y = 620.0f;
                    }
                }
            }
        }

        ply::stepPhysics(world, game.map, fdt);
        ply::updateTransforms(world);

        // --- pickups ---------------------------------------------------------
        if (!game.won) {
            const ply::Transform* playerTransform =
                world.get<ply::Transform>(game.player);
            for (auto it = game.coins.begin(); it != game.coins.end();) {
                const ply::Transform* coinTransform =
                    world.get<ply::Transform>(*it);
                const bool touch =
                    playerTransform && coinTransform &&
                    std::fabs(playerTransform->position.x - coinTransform->position.x) <
                        26.0f &&
                    std::fabs(playerTransform->position.y - coinTransform->position.y) <
                        26.0f;
                if (touch) {
                    world.destroy(*it);
                    it = game.coins.erase(it);
                    ++game.score;
                } else {
                    ++it;
                }
            }
            if (game.score == kCoinCount) {
                game.won = true;
            }
        }
    });

    app.onRender([&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);
        ply::World& world = a.world();

        batch.begin(*texture, a.camera().viewProj());
        for (int y = 0; y < kMapH; ++y) {
            for (int x = 0; x < kMapW; ++x) {
                if (!game.map.isSolid(x, y)) {
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
            const glm::vec2 cam = a.camera().position();
            const float zoom = a.camera().zoom();
            const float halfW = static_cast<float>(a.window().width()) / (2.0f * zoom);
            const float halfH = static_cast<float>(a.window().height()) / (2.0f * zoom);
            const glm::vec2 topLeft{cam.x - halfW + 20.0f, cam.y + halfH - 34.0f};

            text->draw(batch, "COINS  " + std::to_string(game.score) + "/" +
                                  std::to_string(kCoinCount),
                       topLeft, glm::vec4{1.0f, 1.0f, 0.6f, 1.0f}, 1.4f);
            if (game.won) {
                text->draw(batch, "ALL COLLECTED!  you built your first game",
                           topLeft - glm::vec2{0.0f, 44.0f},
                           glm::vec4{0.55f, 1.0f, 0.65f, 1.0f}, 1.2f);
            }
            batch.end();
        }
    });

    app.runFrames(frames > 0 ? frames : (verify ? 240 : 120));

    int exitCode = 0;
    if (verify || screenshot) {
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

        std::printf("tutorial demo: %d frames, %ld lit pixels, coins %d/%d%s\n",
                    app.framesRendered(), lit, game.score, kCoinCount,
                    game.won ? " (win)" : "");

        if (verify) {
            if (lit < 5000) {
                std::fprintf(stderr, "VERIFY FAIL: too few pixels drawn (%ld)\n", lit);
                exitCode = 1;
            } else if (game.score != kCoinCount || !game.won) {
                std::fprintf(stderr,
                             "VERIFY FAIL: script collected %d/%d coins, won=%d\n",
                             game.score, kCoinCount, game.won ? 1 : 0);
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
