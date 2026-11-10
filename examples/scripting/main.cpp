// Scripting demo: gameplay logic lives in Lua, rendering and physics in C++.
//
// The C++ side owns the window, the sprite batch and the fixed step. It spawns
// a ring of drones with sprites, then hands the *behaviour* to a Lua script
// that reads `update(dt)`: the script orbits the drones and bounces them off
// the arena walls. Nothing in Lua knows what a sprite is.
//
// The script is written to a temporary file at startup (readable in the log
// output), so the demo ships no loose .lua asset an installer could forget.
//
// Controls: S toggles "script paused" (the demo keeps stepping physics so you
// can see what scripted motion looks like when the script stops driving it).
// Run with --verify to assert the script really moved every drone.

#include "plyEngine/App.hpp"
#include "plyEngine/Font.hpp"
#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Scripting.hpp"
#include "plyEngine/Sprite.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/TextRenderer.hpp"
#include "plyEngine/Transform.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kDroneCount = 6;
constexpr float kArenaW = 960.0f;
constexpr float kArenaH = 540.0f;

const char* kFontCandidates[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
};

// The whole gameplay layer of this demo. Note what is absent: no window, no
// sprites, no physics stepping — the script only reads and writes positions.
const char* kDemoScript = R"(
-- drones.lua: orbit a centre and bounce off the arena walls.
local drones = {}
local speed = 140
local angle = 0

function update(dt)
    angle = angle + dt * 0.9
    local i = 0
    for _, d in ipairs(drones) do
        local x, y = ply.get_position(d.entity)
        -- orbit term
        local ax = math.cos(angle + d.phase) * 90
        local ay = math.sin(angle + d.phase) * 60
        -- drift term so the pattern does not just spin in place
        x = x + ax * dt * 0.5
        y = y + ay * dt * 0.5

        -- bounce inside the arena
        if x < 60 or x > 900 then d.dirx = -d.dirx end
        if y < 60 or y > 480 then d.diry = -d.diry end
        x = x + d.dirx * speed * dt
        y = y + d.diry * speed * dt

        ply.set_position(d.entity, x, y)
        i = i + 1
    end
end

function add_drone(entity, phase)
    drones[#drones + 1] = { entity = entity, phase = phase, dirx = 1, diry = 1 }
end

ply.log("drones.lua loaded with " .. tostring(#drones) .. " drones")
)";

std::string writeScriptToTemp() {
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "ply_scripting_demo";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const std::filesystem::path path = dir / "drones.lua";
    std::ofstream out(path);
    out << kDemoScript;
    return path.string();
}

std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 2;
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
    fill(0, 120, 220, 255);  // drone
    fill(1, 60, 70, 90);     // wall
    return pixels;
}

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
    config.window.title = "plyEngine - scripting";
    config.window.width = static_cast<int>(kArenaW);
    config.window.height = static_cast<int>(kArenaH);

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
    atlas->addRegion("drone", 0, 0, 32, 32);
    atlas->addRegion("wall", 32, 0, 32, 32);
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
    std::unique_ptr<ply::ScriptEngine> script;
    std::vector<ply::Entity> drones;
    std::vector<glm::vec2> initialPositions;
    bool scriptPaused = false;
    size_t updateCalls = 0;
    std::string scriptError;

    app.onStart([&](ply::App& a) {
        ply::World& world = a.world();
        script = std::make_unique<ply::ScriptEngine>(world);

        const std::string scriptPath = writeScriptToTemp();
        if (!script->doFile(scriptPath)) {
            scriptError = script->error();
            std::fprintf(stderr, "script failed: %s\n", scriptError.c_str());
        }
        std::printf("scripting demo: loaded %s (%zu update hooks available)\n",
                    scriptPath.c_str(), script->hasUpdateCallback() ? 1u : 0u);

        // C++ owns appearance and physics; Lua owns behaviour.
        for (int i = 0; i < kDroneCount; ++i) {
            const float angle = (static_cast<float>(i) / kDroneCount) * 6.2831853f;
            const ply::Entity entity = world.spawn();
            ply::Transform transform;
            transform.position = glm::vec2{kArenaW * 0.5f + std::cos(angle) * 200.0f,
                                           kArenaH * 0.5f + std::sin(angle) * 140.0f};
            world.add<ply::Transform>(entity, transform);
            world.add<ply::GlobalTransform>(entity, ply::GlobalTransform{});
            world.add<ply::Velocity>(entity, ply::Velocity{});

            ply::Sprite sprite;
            sprite.texture = texture;
            sprite.region = atlas->region("drone");
            sprite.size = glm::vec2{26.0f, 26.0f};
            world.add<ply::Sprite>(entity, sprite);

            drones.push_back(entity);
            initialPositions.push_back(transform.position);

            // Hand the handle to the script, which registers it in its own
            // table. C++ created it; Lua decides what it does.
            script->setGlobalEntity("__drone", entity);
            char call[64];
            std::snprintf(call, sizeof(call), "add_drone(__drone, %f)", angle);
            if (!script->doString(call, "register drone")) {
                scriptError = script->error();
                std::fprintf(stderr, "script registration failed: %s\n",
                             scriptError.c_str());
            }
        }

        a.camera().setPosition(glm::vec2{kArenaW * 0.5f, kArenaH * 0.5f});
        a.camera().setZoom(1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    });

    app.onUpdate([&](ply::App& a, double dt) {
        if (script && script->hasUpdateCallback() && !scriptPaused) {
            if (!script->callUpdate(static_cast<float>(dt))) {
                scriptError = script->error();
                std::fprintf(stderr, "script error: %s\n", scriptError.c_str());
                scriptPaused = true;
            } else {
                ++updateCalls;
            }
        }
        ply::updateTransforms(a.world());
    });

    app.onRender([&](ply::App& a, double) {
        glClear(GL_COLOR_BUFFER_BIT);

        batch.begin(*texture, a.camera().viewProj());
        // Arena border, drawn as tiles.
        for (float x = 0.0f; x <= kArenaW; x += 32.0f) {
            batch.draw(glm::vec2{x, 16.0f}, glm::vec2{32.0f, 32.0f}, atlas->region("wall"));
            batch.draw(glm::vec2{x, kArenaH - 16.0f}, glm::vec2{32.0f, 32.0f},
                       atlas->region("wall"));
        }
        batch.end();

        ply::drawSprites(a.world(), batch, a.camera().viewProj());

        if (text) {
            batch.begin(text->texture(), a.camera().viewProj());
            const glm::vec2 cam = a.camera().position();
            const float halfW = static_cast<float>(a.window().width()) / 2.0f;
            const float halfH = static_cast<float>(a.window().height()) / 2.0f;
            const glm::vec2 topLeft{cam.x - halfW + 20.0f, cam.y + halfH - 34.0f};
            text->draw(batch, "Lua-driven drones (" + std::to_string(kDroneCount) + ")",
                       topLeft, glm::vec4{1.0f, 1.0f, 0.6f, 1.0f}, 1.3f);
            text->draw(batch,
                       "update() calls: " + std::to_string(updateCalls) +
                           (scriptPaused ? "   [paused]" : ""),
                       topLeft - glm::vec2{0.0f, 40.0f},
                       glm::vec4{0.75f, 0.85f, 1.0f, 1.0f}, 1.0f);
            if (!scriptError.empty()) {
                text->draw(batch, "script error: " + scriptError,
                           topLeft - glm::vec2{0.0f, 78.0f},
                           glm::vec4{1.0f, 0.5f, 0.5f, 1.0f}, 1.0f);
            }
            batch.end();
        }
    });

    // Interactive by default; --frames/--verify/--screenshot keep the bounded
    // behaviour CI and the docs pipeline rely on.
    const bool automated = verify || screenshot != nullptr;
    if (frames > 0 || automated) {
        app.runFrames(frames > 0 ? frames : (verify ? 240 : 120));
    } else {
        app.run();
    }

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

        std::printf("scripting demo: %d frames, %zu update() calls, %ld lit pixels, "
                    "api calls %zu\n",
                    app.framesRendered(), updateCalls, lit,
                    script ? script->apiCallCount() : 0u);

        if (verify) {
            auto check = [&](bool ok, const char* what) {
                if (!ok) {
                    std::fprintf(stderr, "VERIFY FAIL: %s\n", what);
                    exitCode = 1;
                }
            };
            check(script && script->ready(), "script loaded");
            check(script && script->hasUpdateCallback(), "script defines update(dt)");
            check(updateCalls >= 200, "update() ran once per fixed step");
            check(scriptError.empty(), "script raised no error");
            check(lit >= 5000, "scene rendered");

            // Every drone must have moved measurably from where C++ placed it.
            int moved = 0;
            for (size_t i = 0; i < drones.size() && i < initialPositions.size(); ++i) {
                const ply::Transform* transform =
                    app.world().get<ply::Transform>(drones[i]);
                if (!transform) {
                    continue;
                }
                const glm::vec2 delta = transform->position - initialPositions[i];
                if (std::fabs(delta.x) > 5.0f || std::fabs(delta.y) > 5.0f) {
                    ++moved;
                }
            }
            check(moved == kDroneCount, "the script moved every drone from its spawn");
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

    return exitCode;
}
