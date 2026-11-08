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

## ECS conventions (settled in M3)

**Entity handles are packed index+generation.** A stale handle to a destroyed
entity is detectable, so it can never silently address a recycled slot.

**Components may be any movable type.** Columns relocate and destroy through
type-erased function pointers (`ComponentMeta`), so a component holding a
`std::vector` (e.g. `Children`) is supported and its destructor runs exactly
once.

**Archetype storage.** Entities with the same component set share an archetype;
each component type is a contiguous column. Adding/removing a component moves
the entity between archetypes.

**Transforms are hierarchical with dirty propagation.** `Transform` is local;
`GlobalTransform` is the cached world matrix. Mutating a `Transform` through a
raw pointer requires `markTransformDirty()`; `setTransform()` does it for you and
is the preferred API. Cycles in `Parent` links are rejected.

**Destroying an entity does not require hierarchy bookkeeping from the caller.**
`updateTransforms()` treats an entity whose `Parent` is dead as a root (marking
it dirty so its stale world matrix is rebuilt from its local transform) and
prunes dead or contradictory entries from `Children` lists. Destroying a parent
therefore orphans its children rather than leaving them with a frozen matrix.

**Archetype vector references are never held across allocation.** `ensureArchetype()`
and `appendRow()` push into `archetypes_`, which can reallocate. Code holds the
archetype *index* and re-indexes after any such call. A regression test
(`tests/world_stress.cpp`) sweeps reallocation boundaries, and CI runs the suite
under ASan/UBSan.

**Single-threaded, parallel-ready.** Systems run sequentially in M3. Archetype
layout and system signatures are chosen so a parallel scheduler can be added
without an API break.

**`ply::App` owns the loop.** Fixed-step accumulator with interpolation alpha;
raw pieces stay public for games that want their own loop.

## Renderer conventions (settled in M2 review)

**Sprite anchor: centre.** `SpriteBatch::draw()` takes the sprite's centre, not
its bottom-left corner, matching the 2D-engine mainstream (Godot, Unity 2D,
Cocos). Rotation pivots, culling bounds and M3 transform hierarchies all assume
one convention, so it is fixed now rather than retrofitted.

**Texture filtering: per-texture, default NEAREST.** Pixel art is the default
audience; `TextureFilter::Linear` is available for pre-scaled art and UI.

**Colour space: sRGB passthrough (no linearization), deliberate.** Textures
upload as `GL_RGBA8` and `GL_FRAMEBUFFER_SRGB` stays off, so authored RGB values
reach the framebuffer unchanged — which is what 2D pixel artists expect and
what makes headless render checks byte-exact. Moving to a linear pipeline
(sRGB internal formats + `GL_FRAMEBUFFER_SRGB`) is deferred to **M4**, when a
real asset pipeline and colour-managed art exist. It is a one-line change in
`Texture.cpp` plus a clear-colour conversion.

**Antialiasing: none (MSAA off).** Hard pixel edges are correct for the target
style. If MSAA is added later it must be opt-in per game, since it breaks
pixel-exact reproducibility.

## Open branches (post-1.0-alpha)

- Embedded scripting (Lua binding)
- Browser/WASM build of the game (web distribution)
- Native Android/iOS apps
- WebRTC for internet play across networks (WebSocket/LAN first)
- Binary protocol (MessagePack) if profiling demands
