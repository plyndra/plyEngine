# Tutorial: build your first plyEngine game

This is a complete, end-to-end walkthrough of a working game: one player, a
flat arena, five coins, a score, a win state, and a headless check CI can
run. Every code block below is taken from the finished source:

- **[`examples/tutorial/main.cpp`](../examples/tutorial/main.cpp)** — the
  full game (~340 lines), built as `ply_tutorial`.

If you want to type along, create a file and paste each step's code; the
final section tells you how to wire it into CMake.

**Before you start** you need a C++20 compiler, CMake ≥ 3.24, and the SDL3/X11
system development packages. The full per-OS list lives in
[BUILDING.md — Prerequisites](BUILDING.md); on Ubuntu/Debian it is:

```sh
sudo apt-get install -y build-essential cmake \
  libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxi-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libgl1-mesa-dev libgles2-mesa-dev libxss-dev libxfixes-dev \
  libxtst-dev libxpresent-dev libxshmfence-dev
```

What you will learn: the `App` loop, sprite atlases, the archetype ECS,
fixed-timestep physics with tilemap collision, pickups and score, HUD text,
and why every game should ship a `--verify` check.

---

## Step 1 — a window and a loop

Every plyEngine game is a `ply::App`. You fill in three callbacks: start
(initialize the world), update (simulate one fixed step), render (draw one
frame).

```cpp
#include "plyEngine/App.hpp"

int main() {
    ply::App::Config config;
    config.window.title = "plyEngine - tutorial";
    config.window.width = 960;
    config.window.height = 540;

    ply::App app(config);
    if (!app.valid()) {
        std::fprintf(stderr, "app invalid: %s\n", app.error());
        return 1;
    }

    app.onStart([&](ply::App& a) { /* build the level */ });
    app.onUpdate([&](ply::App& a, double dt) { /* one fixed step */ });
    app.onRender([&](ply::App& a, double alpha) { /* draw */ });

    app.run();          // interactive; app.runFrames(n) for CI
    return 0;
}
```

`onUpdate` runs at a fixed 60 Hz regardless of frame rate — game logic is
deterministic — while `onRender` runs as fast as your display allows and
receives an interpolation alpha. You rarely need to think about timing.

## Step 2 — sprites from an atlas

plyEngine draws textured quads in batches. A *texture atlas* packs many
images into one texture so a whole frame costs one draw call. Here the atlas
is built procedurally (a real game would load a PNG — see
`ply_assets` and the asset cache in `docs/DESIGN.md`):

```cpp
std::vector<unsigned char> buildAtlas(int& outSize) {
    const int cell = 32;
    outSize = cell * 3;   // player | coin | tile
    // ... fill pixels: dark-edged coloured cells ...
}

// in main():
auto atlas = std::make_unique<ply::SpriteAtlas>(
    atlasSize, atlasSize, atlasPixels.data(), ply::TextureFilter::Nearest);
atlas->addRegion("player", 0, 0, 32, 32);
atlas->addRegion("coin",   32, 0, 32, 32);
atlas->addRegion("tile",   64, 0, 32, 32);
auto texture = atlas->texturePtr();
```

`Nearest` filtering is the default because pixel art wants hard edges.

## Step 3 — entities: the archetype ECS

Everything in the world is an entity with components. The player needs four:

```cpp
game.player = world.spawn();
world.add<ply::Transform>(game.player,
    ply::Transform{glm::vec2{96.0f, 110.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
world.add<ply::GlobalTransform>(game.player, ply::GlobalTransform{});
world.add<ply::Velocity>(game.player, ply::Velocity{});
world.add<ply::Body>(game.player, ply::Body{glm::vec2{14.0f, 14.0f}});
```

- `Transform` — position/rotation/scale relative to the world (or a parent).
- `GlobalTransform` — the cached world matrix; `updateTransforms()`
  refreshes it for anything marked dirty.
- `Velocity` — units per second, integrated by the physics step.
- `Body` — a dynamic AABB with half-extents 14×14, plus contact flags
  (`grounded`, `hitX`, `hitY`) that physics fills in every step.

Components are stored SoA per archetype, so systems iterate contiguous
memory. `World::each<Ts...>(fn)` walks every entity that has all of `Ts`.

## Step 4 — draw it

Sprites are components too:

```cpp
ply::Sprite sprite;
sprite.texture = texture;              // shared_ptr: many sprites, one atlas
sprite.region  = atlas->region("player");
sprite.size    = glm::vec2{28.0f, 28.0f};
world.add<ply::Sprite>(game.player, sprite);
```

The render callback draws the static tilemap, then lets the engine batch all
`Sprite` entities in one pass:

```cpp
app.onRender([&](ply::App& a, double) {
    glClear(GL_COLOR_BUFFER_BIT);

    batch.begin(*texture, a.camera().viewProj());
    for (int y = 0; y < kMapH; ++y)
        for (int x = 0; x < kMapW; ++x)
            if (game.map.isSolid(x, y))
                batch.draw(tileCentre(x, y), {kTile, kTile}, atlas->region("tile"));
    batch.end();

    ply::drawSprites(a.world(), batch, a.camera().viewProj());
});
```

Sprites are **centre-anchored**: `draw(centre, size, region)`. The camera is
an orthographic camera you position once in `onStart`.

## Step 5 — input, movement, physics

Update turns input into velocity, then advances physics for every body
against the tilemap:

```cpp
app.onUpdate([&](ply::App& a, double dt) {
    ply::World& world = a.world();

    float move = 0.0f;
    if (a.input().isKeyDown(ply::Key::Left)  || a.input().isKeyDown(ply::Key::A)) move = -1.0f;
    if (a.input().isKeyDown(ply::Key::Right) || a.input().isKeyDown(ply::Key::D)) move =  1.0f;
    const bool jump = a.input().isKeyPressed(ply::Key::Space);

    if (ply::Velocity* v = world.get<ply::Velocity>(game.player)) {
        v->value.x = move * 320.0f;
        if (jump) {
            if (const ply::Body* body = world.get<ply::Body>(game.player);
                body && body->grounded) {
                v->value.y = 620.0f;    // ~120 unit jump arc
            }
        }
    }

    ply::stepPhysics(world, game.map, static_cast<float>(dt));
    ply::updateTransforms(world);
});
```

`stepPhysics` integrates velocity and sweeps each AABB against the solid
cells of the tilemap — per axis, so sliding along walls feels right.
`grounded` is set by the physics itself; that is all you need for jumping.

The level is just a grid of solid cells (floor at row 2, walls at the edges):

```cpp
for (int x = 0; x < kMapW; ++x) game.map.setSolid(x, 2, true);
for (int y = 0; y < kMapH; ++y) {
    game.map.setSolid(0, y, true);
    game.map.setSolid(kMapW - 1, y, true);
}
```

## Step 6 — pickups and a win condition

Coins are entities with a marker component. Each step, test the player's
centre against every live coin and destroy the ones you touch (this is the
real code from `examples/tutorial/main.cpp`):

```cpp
struct Coin {};   // marker component: no data, just identity

// spawn
const ply::Entity coin = world.spawn();
world.add<ply::Transform>(coin, transform);
world.add<ply::GlobalTransform>(coin, ply::GlobalTransform{});
world.add<Coin>(coin, Coin{});

// pickup, in onUpdate after stepPhysics
const auto* playerTransform = world.get<ply::Transform>(game.player);
for (auto it = game.coins.begin(); it != game.coins.end(); ) {
    const auto* coinTransform = world.get<ply::Transform>(*it);
    const bool touch =
        playerTransform && coinTransform &&
        std::fabs(playerTransform->position.x - coinTransform->position.x) < 26.0f &&
        std::fabs(playerTransform->position.y - coinTransform->position.y) < 26.0f;
    if (touch) {
        world.destroy(*it);          // archetype storage re-packs itself
        it = game.coins.erase(it);
        ++game.score;
    } else {
        ++it;
    }
}
if (game.score == kCoinCount) game.won = true;
```

Destroying an entity is safe at any time; the generation-checked handle
means a stale reference is detected instead of silently aliasing a new
entity.

## Step 7 — HUD text

TTF text is baked into a glyph atlas once, then drawn like any sprite
(batches separately, because it is a different texture):

```cpp
ply::Font font;
font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 28);
ply::TextRenderer text(font);

// in render, anchored to the visible top-left corner:
batch.begin(text.texture(), a.camera().viewProj());
text.draw(batch, "COINS  " + std::to_string(game.score) + "/5",
          topLeft, glm::vec4{1.0f, 1.0f, 0.6f, 1.0f}, 1.4f);
batch.end();
```

Anchor to the *camera-relative* corner (as shown), never hard-coded world
coordinates, or your HUD drifts off-screen at other zooms.

## Step 8 — verify, don't hope

A demo that "doesn't crash" proves little. `--verify` plays the game with a
built-in script and asserts the real outcome — here, holding right until all
five coins are collected:

```cpp
if (verify) move = 1.0f;   // headless script; keyboard otherwise

// after runFrames():
if (verify) {
    if (game.score != kCoinCount || !game.won) {
        std::fprintf(stderr, "VERIFY FAIL: collected %d/%d\n",
                     game.score, kCoinCount);
        exitCode = 1;
    }
}
```

CI runs it headlessly:

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/ply_tutorial --verify
```

This convention scales: the reference platformer's `--verify` plays a full
two-player match through the real companion-input queue and asserts coins,
deaths and a winner (see `tests/platformer.cpp` for the same playthrough as
a plain `ctest` suite).

---

## Wiring it into CMake

```cmake
add_executable(ply_tutorial examples/tutorial/main.cpp)
target_link_libraries(ply_tutorial PRIVATE plyEngine)
```

Run it: `./build/ply_tutorial` — arrows/WASD to move, Space to jump.

## Optional: move gameplay logic into Lua

Once the C++ shell exists, behaviour can live in a script and be edited
without recompiling. Create the engine next to your world and call it once per
fixed step:

```cpp
#include "plyEngine/Scripting.hpp"

ply::ScriptEngine script(app.world());
if (!script.doFile("game.lua")) {
    std::fprintf(stderr, "script: %s\n", script.error().c_str());
}

app.onUpdate([&](ply::App& a, double dt) {
    if (!script.callUpdate(static_cast<float>(dt))) {
        std::fprintf(stderr, "script: %s\n", script.error().c_str());
    }
    ply::updateTransforms(a.world());
});
```

```lua
-- game.lua: nothing here knows what a sprite is.
function update(dt)
    local x, y = ply.get_position(player)
    if x < 900 then
        ply.set_position(player, x + 120 * dt, y)
    end
end
```

Entities created in C++ are handed to the script with
`script.setGlobalEntity("player", entity)`. Scripts get `ply.spawn`,
`ply.destroy`, `ply.alive`, `ply.entity_count`, position and velocity
getters/setters, and `ply.log`. `io`, `os` and `debug` are not loaded, so a
script cannot leave the sandbox.

`examples/scripting/` is the runnable version: six drones whose orbit logic is
Lua, with sprites and physics in C++ (`./build/ply_scripting --verify` asserts
every drone actually moved).

## Using plyEngine in your own project

The tutorial above added its target to plyEngine's own `CMakeLists.txt`. A
game outside the repo instead pulls the engine in. `add_subdirectory` (a
git submodule or vendored copy):

```cmake
cmake_minimum_required(VERSION 3.24)
project(mygame LANGUAGES C CXX)

# Don't build the engine's own tests and demos inside your project.
set(PLY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PLY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/plyEngine plyEngine)

add_executable(mygame src/main.cpp)
target_link_libraries(mygame PRIVATE plyEngine)
```

Or with `FetchContent`, pinned to a release tag (the pattern the engine's own
CI compiles on every OS):

```cmake
include(FetchContent)
set(PLY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PLY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(plyEngine
    GIT_REPOSITORY https://github.com/plyndra/plyEngine.git
    GIT_TAG        v1.0.0-beta.2)
FetchContent_MakeAvailable(plyEngine)

target_link_libraries(mygame PRIVATE plyEngine)
```

Linking `plyEngine` gives you the include paths, GLM/json propagation and the
required C++20 standard automatically — do not set `CMAKE_CXX_STANDARD`
yourself. The first configure downloads the pinned dependencies, so it needs
network access.

If you would rather not build the engine per project, install it once and use
`find_package` instead:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
cmake --install build --prefix /opt/plyengine
```

```cmake
find_package(plyEngine CONFIG REQUIRED)   # pass -DCMAKE_PREFIX_PATH=/opt/plyengine
target_link_libraries(mygame PRIVATE plyEngine::plyEngine)
```

```cpp
#include "plyEngine/App.hpp"
#include "plyEngine/Version.hpp"

int main() {
    std::printf("built against plyEngine %s\n", ply::kVersion);
    ply::App app;
    return app.valid() ? 0 : 1;
}
```

The version string comes from `PLY_VERSION_STRING` in the engine's
`CMakeLists.txt` (the header is generated), so it is never stale.

## Where next

- **Phone controllers** — the reference platformer
  (`examples/companion/`) adds a `CompanionServer`; up to four phones join a
  room and drive four players, each with its own colour and score. See
  [GETTING_STARTED.md](GETTING_STARTED.md).
- **Real assets** — PNG loading, Aseprite/TexturePackersheets (`ply_assets`
  demo) and the refcounted `AssetCache<T>` (covered by the `assets` ctest
  suite).
- **Audio** — `ply::AudioSystem` with decoded WAV/OGG effects and per-voice
  volume/pitch/pan. The reference demo plays synthesised cues for jumps,
  coins, deaths and the win; the `audio` ctest suite covers the API directly.
- **Why it works this way** — [DESIGN.md](DESIGN.md) records every settled
  decision (centre anchors, fixed timestep, sRGB passthrough, …).
