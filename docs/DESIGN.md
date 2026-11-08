# plyEngine — Design

## Purpose

An open-source 2D game engine. Games render natively on a PC to a shared
screen; players join from phones as companion controllers through a web
browser / PWA (Jackbox-style). The PC is the single source of truth; phones
send inputs only.

## Decisions (settled via design review, 2026-11)

| # | Decision |
|---|---|
| 1 | Open-source 2D game engine, MIT + DCO, no CLA |
| 2 | Language: C++20 |
| 3 | Foundation: SDL3 (window/input/audio/threading/filesystem, GL context) |
| 4 | Renderer: OpenGL 3.3 core, desktop only |
| 5 | ECS: archetype (Bevy-style SoA) |
| 6 | Math: GLM |
| 7 | Build: CMake + FetchContent (pinned versions) |
| 8 | GDevelop Core: deferred; own JSON scene format (optional M4 spike) |
| 9 | Scripting: native-only through 1.0-alpha |
| 10 | Topology: Jackbox-style companion controllers |
| 11 | Companion: first-class — protocol + C++ SDK + reference PWA template |
| 12 | Hosting/transport: embedded Crow HTTP+WebSocket server; JSON over WebSocket |
| 13 | Mobile: browser/PWA on the phone, not a native app |
| 14 | Demo: single-screen platformer (PC renders, phone controls) |

## Adoption posture

- **Adopt (code):** SDL3 (zlib), glad (MIT), Crow (BSD-3), GLM (MIT).
- **Adopt (architecture):** Bevy — ECS, plugin system, render graph.
- **Rejected:** Frostfire-Forge (PolyForm Noncommercial — not open source);
  Bevy code (Rust); GDevelop GDJS runtime (JavaScript; GDCpp removed in v5).

## Layers

1. **Foundation** — SDL3 + GL loader.
2. **Core** — GLM math, archetype ECS, event bus, fixed-step game loop, assets, logging.
3. **Renderer** — OpenGL 3.3 core: sprite batching, texture atlas, ortho camera.
4. **Companion** — embedded Crow server, JSON protocol, room code + QR join.
5. **Game layer** — native C++ API (games link against plyEngine).

## Open branches (post-1.0-alpha)

- Embedded scripting (Lua binding)
- Browser/WASM build of the game (web distribution)
- Native Android/iOS apps
- WebRTC for internet play across networks (WebSocket/LAN first)
- Binary protocol (MessagePack) if profiling demands
