# plyEngine

**A free, open-source 2D game engine that helps you create games.**

plyEngine gives you the tools to build 2D games in C++20 and run them on a PC —
with a twist: players join from their phones as controllers through any web
browser, no app install. Render the game to a shared screen, and every phone
becomes a companion controller for party-style play (think Jackbox).

> **Status:** M0 — early scaffold. Actively developed; not yet usable for games.

## What you'll be able to build

- **2D games** — sprites, sprite atlases, orthographic camera, physics, audio.
- **Shared-screen party games** — the PC renders the game; each player controls
  it from their phone via a web browser / PWA.
- **Native desktop games** — Windows / Linux / macOS, OpenGL 3.3.

## Tech stack

| Layer | Choice |
|---|---|
| Language | C++20 |
| Foundation | SDL3 (window/input/audio/threading/filesystem, GL context) |
| Renderer | OpenGL 3.3 core, desktop only |
| ECS | Archetype (Bevy-style SoA) |
| Math | GLM |
| Companion | Crow (embedded HTTP + WebSocket), JSON protocol, TypeScript + Vite PWA |
| Build | CMake + vcpkg |
| License | MIT |

## Architecture

- `engine/` — C++ core (renderer, ECS, assets, physics, audio, companion server)
- `companion/` — reference PWA (TypeScript + Vite) that games use as the phone controller
- `examples/` — demo games (single-screen platformer)
- `docs/` — design decisions and roadmap

## Roadmap

M0 scaffold → M1 window → M2 renderer → M3 ECS → M4 assets → M5 simulation →
M6 companion → M7 1.0-alpha. See [docs/ROADMAP.md](docs/ROADMAP.md).

## Building

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Contributing

MIT-licensed, no CLA. Contributions use the
[Developer Certificate of Origin](CONTRIBUTING.md) (sign-off on every commit).
