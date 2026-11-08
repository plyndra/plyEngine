// M4 demo: load a real PNG spritesheet, parse spritesheet metadata, bake a TTF,
// and draw textured sprites plus text through the ECS.

#include "plyEngine/App.hpp"
#include "plyEngine/Font.hpp"
#include "plyEngine/Image.hpp"
#include "plyEngine/Sprite.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/Spritesheet.hpp"
#include "plyEngine/TextRenderer.hpp"
#include "plyEngine/Transform.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

std::string readText(const std::string& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

const char* kFontCandidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
};

}  // namespace

int main(int argc, char** argv) {
    int frames = 0;
    const char* screenshot = nullptr;
    bool verify = false;
    std::string assets = "tests/data";

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (std::strcmp(argv[i], "--verify") == 0) {
            verify = true;
        } else if (std::strcmp(argv[i], "--assets") == 0 && i + 1 < argc) {
            assets = argv[++i];
        }
    }

    // --- CPU-side asset loading (no GL needed yet) --------------------------
    const ply::Image sheetImage = ply::Image::loadFromFile(assets + "/test_sheet.png");
    if (!sheetImage.valid()) {
        std::fprintf(stderr, "failed to load spritesheet: %s\n",
                     sheetImage.error().c_str());
        return 1;
    }

    std::vector<ply::NamedRect> rects;
    std::string parseError;
    if (!ply::parseAsepriteJson(readText(assets + "/test_sheet.aseprite.json"), rects,
                                parseError)) {
        std::fprintf(stderr, "failed to parse spritesheet: %s\n", parseError.c_str());
        return 1;
    }
    std::printf("loaded spritesheet %dx%d with %zu frames\n", sheetImage.width(),
                sheetImage.height(), rects.size());

    ply::Font font;
    bool haveFont = false;
    for (const char* path : kFontCandidates) {
        if (font.loadFromFile(path, 28)) {
            haveFont = true;
            std::printf("baked font %s at 28px (atlas %dx%d)\n", path,
                        font.atlasWidth(), font.atlasHeight());
            break;
        }
    }
    if (!haveFont) {
        std::printf("no system TTF found; text will be skipped\n");
    }

    ply::App::Config config;
    config.window.title = "plyEngine - assets";
    config.window.width = 1280;
    config.window.height = 720;

    ply::App app(config);
    if (!app.valid()) {
        std::fprintf(stderr, "app invalid: %s\n", app.error());
        return 1;
    }

    // GL objects must be created after the context exists.
    auto atlas = std::make_unique<ply::SpriteAtlas>(sheetImage.width(),
                                                    sheetImage.height(),
                                                    sheetImage.pixels(),
                                                    ply::TextureFilter::Nearest);
    atlas->addRegions(rects);
    auto sharedTexture = atlas->texturePtr();

    std::unique_ptr<ply::TextRenderer> text;
    if (haveFont) {
        text = std::make_unique<ply::TextRenderer>(font);
    }

    ply::SpriteBatch batch;

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();

        // Lay the eight loaded frames out in a grid using the ECS.
        for (size_t i = 0; i < rects.size(); ++i) {
            const int col = static_cast<int>(i % 4);
            const int row = static_cast<int>(i / 4);

            const ply::Entity e = world.spawn();
            const glm::vec2 position{(-240.0f + col * 160.0f),
                                     (90.0f - row * 160.0f)};
            world.add<ply::Transform>(
                e, ply::Transform{position, 0.0f, glm::vec2{1.0f, 1.0f}});
            world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});

            ply::Sprite sprite;
            sprite.texture = sharedTexture;
            sprite.region = atlas->region(rects[i].name);
            sprite.size = glm::vec2(96.0f, 96.0f);
            world.add<ply::Sprite>(e, sprite);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.07f, 0.07f, 0.11f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double) { ply::updateTransforms(a.world()); });

    auto renderScene = [&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);

        // Sprites (batched per texture by drawSprites).
        ply::drawSprites(a.world(), batch, a.camera().viewProj());

        // Text batches separately because it uses a different texture.
        if (text) {
            batch.begin(text->texture(), a.camera().viewProj());
            text->draw(batch, "plyEngine M4 - assets loaded from disk",
                       glm::vec2(-600.0f, 320.0f), glm::vec4{1, 1, 1, 1}, 1.0f);
            text->draw(batch,
                       "PNG -> spritesheet JSON -> atlas -> ECS Sprite -> batch",
                       glm::vec2(-600.0f, 280.0f), glm::vec4{0.7f, 0.85f, 1.0f, 1.0f});
            text->draw(batch, std::to_string(rects.size()) + " frames, "
                              "TTF baked at 28px",
                       glm::vec2(-600.0f, 240.0f), glm::vec4{0.7f, 1.0f, 0.8f, 1.0f});
            batch.end();
        }
    };

    app.onRender(renderScene);

    app.runFrames(frames > 0 ? frames : 60);

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
        std::printf("assets demo: %d frames, %zu sprites, %ld lit pixels\n",
                    app.framesRendered(), rects.size(), lit);

        if (verify) {
            if (lit < 5000) {
                std::fprintf(stderr, "VERIFY FAIL: too few pixels drawn (%ld)\n", lit);
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
