# plyEngine

**A free, open-source 2D game engine that helps you create games.**

plyEngine gives you the tools to build 2D games in C++20 and run them on a PC —
with a twist: players join from their phones as controllers through any web
browser, no app install. Render the game to a shared screen, and every phone
becomes a companion controller for party-style play (think Jackbox).

> **Status:** **1.0-alpha** (`v1.0.0-alpha.2`) — the whole engine surface is
> in place and covered by tests, but expect API changes before 1.0.
> Prebuilt demo binaries: [Releases](https://github.com/plyndra/plyEngine/releases).

## What you'll be able to build

- **2D games** — sprites, sprite atlases, orthographic camera, physics, audio.
- **Shared-screen party games** — the PC renders the game; each player controls
  it from their phone via a web browser / PWA.
- **Native desktop games** — Windows / Linux / macOS, OpenGL 3.3.

## Engine core

- **Archetype ECS** — cache-friendly component columns; entities carry
  generation-checked handles so stale references are detected.
- **Transform hierarchy** — parent/child composition with dirty-flag caching.
- **Fixed-timestep loop** — deterministic 60 Hz simulation with render
  interpolation, owned by `ply::App`.
- **Sprite batch renderer** — atlas-backed quads in a single draw call,
  centre-anchored, with per-texture filtering.
- **Asset pipeline** — PNG decoding, Aseprite/TexturePacker/generic spritesheet
  loaders, TTF text baked into an atlas, and a reference-counted asset cache.
- **Physics** — per-axis swept AABB collision against tilemaps, deterministic
  under the fixed timestep, with a spatial-hash broadphase.
- **Audio** — decoded sound effects with per-voice volume, pitch and pan.
- **Debug draw** — collider, tile-grid and body overlays for development.
- **Phone controllers** — embedded HTTP+WebSocket server, mDNS discovery, room
  codes, and a reference PWA so players join from a browser with no install.

## Tech stack

| Layer | Choice |
|---|---|
| Language | C++20 |
| Foundation | SDL3 (window/input/audio/threading/filesystem, GL context) |
| Renderer | OpenGL 3.3 core, desktop only |
| ECS | Archetype (Bevy-style SoA) |
| Math | GLM |
| Companion | Crow (HTTP + WebSocket), versioned JSON protocol, mDNS, TypeScript + Vite PWA |
| Build | CMake + FetchContent (pinned) |
| Licence | MIT (DCO sign-off, no CLA) |

## Quick start

```sh
git clone https://github.com/plyndra/plyEngine.git
cd plyEngine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # 14 headless suites
```

Full per-OS dependencies: [docs/BUILDING.md](docs/BUILDING.md).

Using the engine from your own game (one target, one link line — include
paths, dependencies and the C++20 requirement propagate):

```cmake
include(FetchContent)
set(PLY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PLY_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(plyEngine
    GIT_REPOSITORY https://github.com/plyndra/plyEngine.git
    GIT_TAG        v1.0.0-alpha.2)
FetchContent_MakeAvailable(plyEngine)

target_link_libraries(mygame PRIVATE plyEngine)
```

See [docs/TUTORIAL.md](docs/TUTORIAL.md) for the full walkthrough.

Run the examples:

```sh
./build/ply_hello          # batching + atlas + camera
./build/ply_ecs            # App + ECS + transform hierarchy
./build/ply_assets         # PNG + spritesheet JSON + TTF text
./build/ply_physics        # tilemap collision + debug overlay (F1)
./build/ply_tutorial       # the first-game walkthrough: docs/TUTORIAL.md
./build/ply_companion      # reference platformer, phone-controlled
```

Play the reference demo with phones:

```sh
cd companion && npm ci && npm run build && cd ..   # once: build the PWA
./build/ply_companion                              # prints room code + URL
```

Open the URL on each phone (same Wi-Fi), enter the room code. Slot 1 drives
**P1** (arrows + Space on the keyboard), slot 2 drives **P2** (A/D/W). Coins,
patrolling saws, a pit and a staircase lead to the flag — first player there
wins. `P` pauses, `R` restarts.

## Verification

Every milestone ships with checks that can actually fail:

- **14 `ctest` suites** — ECS, transforms, timestep, atlas math, assets,
  physics, audio, companion protocol/server, and a scripted full playthrough
  of the reference platformer (`platformer`), all green in Release **and**
  under `-DPLY_SANITIZE=ON` (ASan + UBSan).
- **`--verify` on every demo** — headless assertions on real outcomes: the
  tutorial collects all five coins; `ply_companion --verify` plays a complete
  match through the real companion-input queue and asserts coins, deaths,
  independent second player and a winner.
- **CI** — Windows / Linux / macOS build matrix, a sanitizer job, Xvfb render
  checks, and the PWA build. Tagged releases additionally **unpack and run**
  the Linux archive on a clean Ubuntu 22.04 with no toolchain installed,
  assert the glibc floor, and build the tutorial from the published source
  tarball before anything is published:
  [Releases](https://github.com/plyndra/plyEngine/releases).

## Documentation

| Doc | What it covers |
|---|---|
| [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) | Build, test, run, play with phones |
| [docs/BUILDING.md](docs/BUILDING.md) | Per-OS dependencies, CMake options, sanitizers |
| [docs/TUTORIAL.md](docs/TUTORIAL.md) | Build your first game, step by step |
| [docs/DESIGN.md](docs/DESIGN.md) | Architecture and settled design decisions |
| [docs/ROADMAP.md](docs/ROADMAP.md) | Milestones and what's next |
| [CHANGELOG.md](CHANGELOG.md) | Release notes (Keep a Changelog) |

## Roadmap

M0 scaffold → M1 window → M2 renderer → M3 ECS → M4 assets → M5 simulation →
M6 companion → **M7 1.0-alpha (done)**. Post-1.0-alpha branches (scripting,
web build, mobile, internet play) are listed in
[docs/ROADMAP.md](docs/ROADMAP.md).

## Contributing

MIT-licensed, no CLA. Contributions use the
[Developer Certificate of Origin](CONTRIBUTING.md) (sign-off on every commit)
and follow our [Code of Conduct](CODE_OF_CONDUCT.md). Bug reports and feature
requests: [issue templates](https://github.com/plyndra/plyEngine/issues/new/choose).
