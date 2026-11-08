# plyEngine

An open-source 2D game engine in C++20. Games render natively on a PC to a
shared screen; players join from phones as companion controllers through a web
browser / PWA (Jackbox-style).

> **Status:** M0 — scaffold. Not yet usable for games.

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
