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

## Simulation (settled in M5)

**Colliders are AABB only.** `Body` carries half-extents; the box is centred on
the entity's `Transform`. The collision math is deliberately shape-agnostic
(`ColliderQuery` returns boxes), so adding circles later is additive.

**Resolution is per-axis swept AABB, move-and-slide.** X is resolved, then Y, so
bodies slide along walls and **cannot tunnel** at any speed — a 100000-unit
delta still stops exactly on a thin wall's face (asserted in the tests).

**Already-overlapping bodies are not blocked.** A sweep that starts inside
geometry reports no hit so a body can work itself free instead of sticking.

**Broadphase is a spatial hash grid** with a brute-force path for tiny scenes.
The tilemap answers queries directly by converting the AABB to a cell range.
`insert()` is idempotent per id: re-inserting an id **moves** it, removing it
from its old cells. A caller that never calls `clear()` therefore cannot
accumulate stale candidates or grow the bucket map without bound.

**Tilemap cells are solid/empty; out-of-bounds counts as solid** by default so
bodies cannot leave the level (configurable).

**Physics is deterministic.** Semi-implicit Euler at a fixed dt with no
floating-point-order dependence; a test asserts two identical runs produce
bit-identical final positions.

**Audio is miniaudio**, which owns its own device alongside SDL3's window/input.
With no device the system reports not-ready and every call is a harmless no-op,
so headless CI needs no special casing. Play options (loop, volume, pitch, pan)
are **per voice**: `play()` never mutates the shared decoded sound, so one
looping call cannot make later one-shots loop.

**Debug draw is part of the engine**, not the demo: collider outlines, the tile
grid and body centres, behind an `F1`-style toggle.

## Asset pipeline (settled in M4)

**Decoders:** stb_image (PNG/JPEG/…) and stb_truetype (TTF), both pinned by
commit in FetchContent. nlohmann/json parses metadata.

**AssetCache<T> is a real refcounted cache.** Handles are `shared_ptr`-based and
the cache only holds `weak_ptr` references, so an asset is released the moment
its last handle dies; expired entries are purged lazily on the next load. The
cache never keeps assets alive on its own.

**Spritesheet formats:** our generic JSON, Aseprite JSON (hash and array forms),
and the TexturePacker/libGDX text atlas. Aseprite's hash form returns frames in
JSON object-key order rather than sheet order, so consumers should key frames by
name; the array form preserves order.

**Text** is baked per size into a single atlas (printable ASCII) and drawn
through its own batch, because `SpriteBatch` groups by texture.

**`Sprite` components share a texture via `shared_ptr`.** `drawSprites()` groups
instances by texture so each distinct texture costs one draw call.

**sRGB: still passthrough.** Pixels from a PNG reach the framebuffer unchanged.
Revisit at M5 when lighting/additive blending makes linear correctness matter.

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
