// M3 demo: App + archetype ECS + transform hierarchy.
//
// A three-level hierarchy (system -> planet -> moon) driven on a fixed 60 Hz
// step and drawn from each entity's cached GlobalTransform.

#include "plyEngine/App.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Transform.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr float kTau = 6.28318530718f;

// Texture atlas data shared by the demo.
struct Atlas {
    std::vector<unsigned char> pixels;
    int size = 256;
};

// Builds an 4x4 atlas of 2-tone cells in a dark-background palette.
Atlas buildAtlas() {
    const int cell = 64;
    Atlas atlas;
    atlas.pixels.assign(static_cast<size_t>(atlas.size) * atlas.size * 4, 0);

    const int colors[][3] = {
        {240, 220, 120}, {120, 200, 240}, {240, 130, 150}, {150, 240, 170},
        {200, 160, 240}, {240, 180, 120}, {140, 200, 160}, {230, 230, 230},
        {250, 200, 80},  {100, 170, 230}, {235, 120, 130}, {130, 220, 200},
        {180, 140, 240}, {245, 190, 140}, {120, 190, 150}, {220, 220, 220},
    };

    for (int c = 0; c < 16; ++c) {
        const int cx = c % 4;
        const int cy = c / 4;
        for (int py = 0; py < cell; ++py) {
            for (int px = 0; px < cell; ++px) {
                const int x = cx * cell + px;
                const int y = cy * cell + py;
                const int i = (y * atlas.size + x) * 4;
                const bool border =
                    px < 2 || py < 2 || px >= cell - 2 || py >= cell - 2;

                if (border) {
                    atlas.pixels[i + 0] = 20;
                    atlas.pixels[i + 1] = 20;
                    atlas.pixels[i + 2] = 28;
                } else {
                    // Soft vertical gradient so rotation is visible.
                    const float t = static_cast<float>(py) / cell;
                    const float shade = 0.55f + 0.45f * (1.0f - t);
                    atlas.pixels[i + 0] = static_cast<unsigned char>(colors[c][0] * shade);
                    atlas.pixels[i + 1] = static_cast<unsigned char>(colors[c][1] * shade);
                    atlas.pixels[i + 2] = static_cast<unsigned char>(colors[c][2] * shade);
                }
                atlas.pixels[i + 3] = 255;
            }
        }
    }
    return atlas;
}

// Everything the demo keeps between frames.
struct Scene {
    ply::SpriteBatch* batch = nullptr;
    ply::SpriteAtlas* atlas = nullptr;

    struct Body {
        ply::Entity entity;
        const ply::TextureRegion* region;
        float size;
        float orbitRadius;
        float orbitSpeed;
        float phase;
        float spin;
    };
    std::vector<Body> bodies;
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
    config.window.title = "plyEngine - ECS + hierarchy";
    config.window.width = 1280;
    config.window.height = 720;
    config.fixedStep = 1.0 / 60.0;

    ply::App app(config);
    if (!app.valid()) {
        std::fprintf(stderr, "app invalid: %s\n", app.error());
        return 1;
    }

    Atlas atlasData = buildAtlas();
    // 4x4 grid of 64px cells -> 256x256.
    ply::SpriteAtlas atlas(256, 256, atlasData.pixels.data(), ply::TextureFilter::Linear);

    Scene scene;
    atlas.addRegion("sun", 0, 0, 64, 64);
    atlas.addRegion("planet", 64, 0, 64, 64);
    atlas.addRegion("moon", 128, 0, 64, 64);
    atlas.addRegion("ring", 192, 0, 64, 64);

    ply::SpriteBatch batch;
    scene.batch = &batch;
    scene.atlas = &atlas;

    // Shared lambda for spawning a hierarchy node (defined before use below).
    auto spawnNode = [](ply::World& world, glm::vec2 pos) {
        const ply::Entity e = world.spawn();
        world.add<ply::Transform>(
            e, ply::Transform{pos, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});
        return e;
    };

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();

        // Level 0: the sun at the origin.
        const ply::Entity sun = spawnNode(world, {0.0f, 0.0f});
        scene.bodies.push_back({sun, &atlas.region("sun"), 88.0f, 0.0f, 0.0f, 0.0f, 0.3f});

        // Level 1: planets orbiting the sun.
        const int planetCount = 5;
        for (int i = 0; i < planetCount; ++i) {
            const ply::Entity planet = spawnNode(world, {0.0f, 0.0f});
            ply::setParent(world, planet, sun);

            const float radius = 130.0f + 42.0f * static_cast<float>(i);
            scene.bodies.push_back({planet,
                                    &atlas.region("planet"),
                                    40.0f - 3.0f * static_cast<float>(i),
                                    radius,
                                    0.35f + 0.12f * static_cast<float>(i),
                                    static_cast<float>(i) * (kTau / planetCount),
                                    1.1f});

            // Level 2: a moon orbiting each planet.
            const ply::Entity moon = spawnNode(world, {0.0f, 0.0f});
            ply::setParent(world, moon, planet);
            scene.bodies.push_back({moon,
                                    &atlas.region("moon"),
                                    18.0f,
                                    46.0f + 8.0f * static_cast<float>(i),
                                    1.6f + 0.3f * static_cast<float>(i),
                                    static_cast<float>(i) * 1.7f,
                                    -2.0f});
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.06f, 0.06f, 0.10f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        ply::World& world = a.world();
        const float fdt = static_cast<float>(dt);

        // Drive orbital motion through the ECS: mutate Transform, mark dirty.
        for (Scene::Body& body : scene.bodies) {
            if (body.orbitSpeed == 0.0f) {
                ply::Transform* t = world.get<ply::Transform>(body.entity);
                if (t) {
                    t->rotation += body.spin * fdt;
                    ply::markTransformDirty(world, body.entity);
                }
                continue;
            }
            body.phase += body.orbitSpeed * fdt;
            ply::Transform* t = world.get<ply::Transform>(body.entity);
            if (t) {
                t->position = glm::vec2(std::cos(body.phase) * body.orbitRadius,
                                        std::sin(body.phase) * body.orbitRadius);
                t->rotation += body.spin * fdt;
                ply::markTransformDirty(world, body.entity);
            }
        }

        ply::updateTransforms(world);
    });

    auto renderScene = [&](ply::App& a, double /*alpha*/) {
        glClear(GL_COLOR_BUFFER_BIT);

        ply::World& world = a.world();
        batch.begin(atlas.texture(), a.camera().viewProj());
        for (const Scene::Body& body : scene.bodies) {
            const ply::GlobalTransform* gt = world.get<ply::GlobalTransform>(body.entity);
            if (!gt) {
                continue;
            }
            batch.draw(gt->matrix, glm::vec2(body.size, body.size), *body.region);
        }
        batch.end();
    };

    app.onRender(renderScene);

    int exitCode = 0;
    const int targetFrames = frames > 0 ? frames : 0;
    exitCode = app.runFrames(targetFrames > 0 ? targetFrames : 120);

    if (verify || screenshot) {
        // Render one more frame WITHOUT swapping, then read the back buffer.
        // Reading after swapBuffers() is undefined: the buffer we read may
        // already have been recycled.
        renderScene(app, 0.0);
        glFinish();
        const int w = app.window().width();
        const int h = app.window().height();
        std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3);
        glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());

        // Count non-background pixels to prove something was drawn.
        const unsigned char* ref = rgb.data();
        long drawn = 0;
        for (size_t i = 0; i < rgb.size(); i += 3) {
            if (rgb[i] != ref[0] || rgb[i + 1] != ref[1] || rgb[i + 2] != ref[2]) {
                ++drawn;
            }
        }
        std::printf("ecs demo: %d frames, %d steps, %zu bodies, %ld lit pixels\n",
                    app.framesRendered(), app.stepsSimulated(), scene.bodies.size(),
                    drawn);

        if (verify) {
            if (drawn < 1000) {
                std::fprintf(stderr, "VERIFY FAIL: almost nothing drawn (%ld px)\n",
                             drawn);
                exitCode = 1;
            } else {
                std::printf("verify: PASS\n");
            }
        }

        if (screenshot) {
            std::FILE* f = std::fopen(screenshot, "wb");
            if (f) {
                std::fprintf(f, "P6\n%d %d\n255\n", w, h);
                // glReadPixels is bottom-up; flip to top-down for PPM.
                for (int y = h - 1; y >= 0; --y) {
                    std::fwrite(&rgb[static_cast<size_t>(y) * w * 3], 1,
                                static_cast<size_t>(w) * 3, f);
                }
                std::fclose(f);
                std::printf("wrote %s\n", screenshot);
            }
        }
    } else {
        std::printf("ecs demo: %d frames, %d steps, %zu bodies\n",
                    app.framesRendered(), app.stepsSimulated(), scene.bodies.size());
    }

    return exitCode;
}
