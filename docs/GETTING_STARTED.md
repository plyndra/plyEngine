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

Dependencies (SDL3, glad, GLM, stb, nlohmann/json, Crow, miniaudio, …) are
pinned and fetched automatically by CMake — nothing to install beyond a
C++20 compiler, CMake 3.24+, and Python 3 with `jinja2` (for the GL loader).
See [BUILDING.md](BUILDING.md) for the per-OS packages SDL3 needs on Linux.

## Run the tests

```sh
ctest --test-dir build --output-on-failure
```

Fourteen headless suites cover the ECS, transforms, timestep, assets,
physics, companion protocol and the reference demo's gameplay logic. They
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
./build/ply_companion   # the reference platformer, phone-controlled
```

Every demo accepts `--verify`, which plays or checks real outcomes headlessly
(the tutorial collects all five coins; the platformer plays a full scripted
match to a win) — that is what CI asserts.

## Play the reference demo with phones

The reference demo is a two-player single-screen platformer: coins, patrolling
saws, a pit, a staircase and a goal flag.

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
     plyEngine 1.0.0-alpha.1 - platformer demo
     room code: HL5W
     open:      http://192.168.x.x:8080
   ========================================
   ```

4. On each phone, open the URL (same Wi-Fi), enter the room code, and join.
   Slot 1 drives **P1**, slot 2 drives **P2**:

   - P1 keyboard fallback: arrows + Space
   - P2 keyboard fallback: A / D / W
   - P or START: pause · R or START (after a win): restart

   First player to the flag wins.

`--no-mdns` disables mDNS advertising (use it on networks where multicast is
blocked); mDNS lets phones discover the game without typing the URL.

## Next steps

- Build your first game: [TUTORIAL.md](TUTORIAL.md)
- How the engine is put together: [DESIGN.md](DESIGN.md)
- Where it's heading: [ROADMAP.md](ROADMAP.md)
- Contribute: [../CONTRIBUTING.md](../CONTRIBUTING.md) (DCO sign-off, no CLA)
