# Getting started with plyEngine

plyEngine is a free, open-source 2D game engine for C++20. You render a game
on a PC; players join from their phones as controllers through any web
browser — no app install, Jackbox-style.

This page gets you from zero to a running demo in a few minutes. For
per-platform dependencies and CMake options, see [BUILDING.md](BUILDING.md).

## Build

```sh
git clone https://github.com/plyndra/plyEngine.git
cd plyEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Dependencies (SDL3, GLM, stb, nlohmann/json, Crow, miniaudio, Lua, …) are
pinned and fetched automatically by CMake — nothing to install beyond a C++20
compiler and CMake 3.24+. The OpenGL loader is vendored, so no Python or
generator step is required. See [BUILDING.md](BUILDING.md) for the per-OS
packages SDL3 needs on Linux.

## Run the tests

```sh
ctest --test-dir build --output-on-failure
```

Fifteen headless suites cover the ECS, transforms, timestep, assets, physics,
audio, scripting, the companion protocol and the reference demo's gameplay
logic. They
also run under AddressSanitizer + UBSan:

```sh
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPLY_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Run the examples

```sh
./build/ply_hello       # sprite batching + atlas + camera
./build/ply_ecs         # App loop, archetype ECS, transform hierarchy
./build/ply_assets      # PNG + spritesheet JSON + TTF text loading
./build/ply_physics     # tilemap collision, swept AABB, debug overlay (F1)
./build/ply_tutorial    # the small game the tutorial builds: docs/TUTORIAL.md
./build/ply_scripting   # drone behaviour in Lua, sprites and physics in C++
./build/ply_companion   # the reference platformer, phone-controlled
```

Every demo accepts `--verify`, which plays or checks real outcomes headlessly
(the tutorial collects all five coins; the platformer plays a full scripted
match to a win) — that is what CI asserts.

## Play the reference demo with phones

The reference demo is a four-player single-screen platformer: coins with
per-player scoring, patrolling saws, a pit, a staircase, a goal flag and
synthesised sound cues (jump, coin, death, win).

1. Build the companion PWA (needs Node.js):

   ```sh
   cd companion
   npm ci
   npm run build
   cd ..
   ```

2. Start the demo:

   ```sh
   ./build/ply_companion
   ```

3. It prints a **room code** and a LAN URL:

   ```
   ========================================
     plyEngine 1.0.0-beta.1 - platformer demo
     room code: HL5W
     open:      http://192.168.x.x:8080
   ========================================
   ```

4. On each phone, open the URL (same Wi-Fi), enter the room code, and join.
   Slots 1-4 drive **P1**-**P4** independently (each has its own colour, score
   and respawn):

   - P1 keyboard fallback: arrows + Space
   - P2 keyboard fallback: A / D / W
   - P3 / P4: phone only
   - P or START: pause · R or START (after a win): restart

   Coins are contested — first player to the flag wins.

`--no-mdns` disables mDNS advertising (use it on networks where multicast is
blocked); mDNS lets phones discover the game without typing the URL.

## Gameplay logic in Lua

Scripting is part of the engine (`ply::ScriptEngine`). A script gets a global
`ply` table and, if it defines `update(dt)`, the host can call it once per
fixed step:

```cpp
ply::World& world = app.world();
ply::ScriptEngine script(world);
if (!script.doFile("game.lua")) {
    std::fprintf(stderr, "%s\n", script.error().c_str());
}
// inside your fixed-step update:
if (!script.callUpdate(static_cast<float>(dt))) {
    std::fprintf(stderr, "%s\n", script.error().c_str());
}
```

```lua
-- game.lua: the C++ side owns sprites and physics; this owns behaviour.
function update(dt)
    local x, y = ply.get_position(drone)
    ply.set_position(drone, x + 60 * dt, y)
end
```

The API is `ply.spawn(x, y)`, `ply.destroy`, `ply.alive`, `ply.entity_count`,
`ply.get_position`/`set_position`, `ply.get_velocity`/`set_velocity`,
`ply.log`. `io`, `os` and `debug` are not loaded. See
[examples/scripting](../examples/scripting/main.cpp) for a runnable demo.

## Use the engine in your own game

A game outside this repo links plyEngine one of two ways — include paths,
dependencies and the C++20 requirement propagate in both.

**A) As a CMake subproject** (pinned `FetchContent`, or vendored and
`add_subdirectory`):

```cmake
include(FetchContent)
set(PLY_BUILD_TESTS OFF CACHE BOOL "" FORCE)      # don't build engine tests
set(PLY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)   # or its demos
FetchContent_Declare(plyEngine
    GIT_REPOSITORY https://github.com/plyndra/plyEngine.git
    GIT_TAG        v1.0.0-beta.1)
FetchContent_MakeAvailable(plyEngine)

add_executable(mygame src/main.cpp)
target_link_libraries(mygame PRIVATE plyEngine)
```

**B) As an installed SDK** (`find_package`):

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /opt/plyengine
```

```cmake
find_package(plyEngine CONFIG REQUIRED)   # -DCMAKE_PREFIX_PATH=/opt/plyengine
add_executable(mygame src/main.cpp)
target_link_libraries(mygame PRIVATE plyEngine::plyEngine)
```

The install ships the engine, its internal static archives, the three public
headers-only dependencies it exposes types from (GLM, nlohmann/json) and SDL3.
Crow/asio, mdns, stb and miniaudio are compiled into the engine, so a consumer
never configures them.

```cpp
#include "plyEngine/App.hpp"

int main() {
    ply::App::Config config;
    config.window.title = "my game";
    ply::App app(config);
    if (!app.valid()) return 1;
    app.run();
}
```

Do not set `CMAKE_CXX_STANDARD` yourself: `plyEngine` declares C++20 as a
public compile feature. Full walkthrough: [TUTORIAL.md](TUTORIAL.md).

## Next steps

- Build your first game: [TUTORIAL.md](TUTORIAL.md)
- How the engine is put together: [DESIGN.md](DESIGN.md)
- Where it's heading: [ROADMAP.md](ROADMAP.md)
- Contribute: [../CONTRIBUTING.md](../CONTRIBUTING.md) (DCO sign-off, no CLA)
