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
| 15 | 1.0-alpha demo scope: full vertical slice — 2 phone players, coins/scoring, patrolling hazards, pit, goal with win state, HUD, pause/restart |
| 16 | Docs: curated markdown through alpha (no site generator, no Doxygen); tutorial = one small game that compiles in CI |
| 17 | Distribution: GitHub Releases with per-OS binaries + PWA + source tarball; `v1.0.0-alpha.N` semver prerelease tags |
| 18 | Verification: gameplay outcomes asserted by a deterministic Autopilot shared by `--verify` and a headless ctest suite |

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

## Companion networking (settled in M6)

**Network threads never touch the simulation.** Crow callbacks only call
session join/leave and push controller states into a mutex-guarded queue. The
fixed-step update drains that queue, so the simulation stays single-threaded and
deterministic and never takes a lock while simulating.

**The protocol is a versioned JSON envelope.** Every frame is
`{v, type, seq, payload}`. The server sends `hello` with its version and rejects
mismatched clients with an explanatory `rejected` frame rather than
misinterpreting them.

**Stale input cannot rewind state.** Each input frame carries a sequence number;
frames older than the last applied one for that slot are dropped.

**Games read a virtual controller, not messages.** Phones, keyboard and gamepads
all reduce to `ControllerState{x, y, buttons}`, exposed per slot with
`pressed()`/`released()` edge detection between fixed steps.

**Pairing is room code + mDNS.** The host shows a 4-character code (from an
alphabet excluding I, O, 0 and 1) and advertises `_plyengine._tcp.local.` via
mjansson/mdns with the room code in a TXT record. Advertising is best-effort:
multicast may be blocked, so failure is logged and the typed code always works.

**Sessions have N slots and a reconnect grace window.** A phone that drops keeps
its slot for `reconnectGrace` seconds and re-binds by name; after the window the
slot frees. Time is injected so the state machine is testable without clocks.

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

## Reference demo & release (settled in M7)

**The demo is a full vertical slice, not a tech demo.** The reference
platformer (`examples/companion/`) must prove the engine end to end: a
hand-authored tilemap (pit, ledge, three-step staircase), coins with
per-player scoring, two patrolling saws, a pit death, a goal flag with a
win state, HUD (room code, URL, scores, state line) and pause/restart.
First player to the flag wins; there is no timer — a party game that ends
when someone arrives is enough to exercise every subsystem.

**Two phone players, slots map 1:1 to players.** Companion slot 0 drives
P1, slot 1 drives P2; slots 2-3 join as spectators. Keyboard fallback
(arrows+Space / A-D-W) keeps the demo playable with zero phones. The
server already accepted multiple joins from M6 — M7 makes the game
actually use them, because "phones as controllers" is the product pitch
and an undemonstrable pitch is a bug.

**Game logic is GL-free and lives beside the demo.** `platformer.hpp`
holds the whole game (level, step, win/death/pickup rules, Autopilot);
`main.cpp` holds only windowing, sprites and HUD. This is what lets
`tests/platformer.cpp` run a complete playthrough headless as a ctest
suite, and lets `--verify` assert the *same* logic — one code path, two
harnesses. A demo whose rules can only be tested through a GPU is a demo
whose rules don't get tested.

**`--verify` asserts outcomes through the real input path.** The
Autopilot is a deterministic scripted player: it reads game state and
emits the same `PlayerInput` a human's phone produces. In verify mode the
demo converts those to `ControllerState` frames and pushes them through
`CompanionInput.push()` — the exact queue → drain → slot-routing path a
phone uses — then asserts: coins collected, deaths ≥ 2 (pit and saw),
player 2 displaced from its spawn independently, hazards travelled > 100
units, and `state == Won && winner == 0`. The script is a phase machine
(Probe → Coin → Pit → Saw → Goal) with a stuck detector, so a regression
that breaks navigation fails the gate instead of hanging it. The same
playthrough runs in ctest (plus pause/reset/determinism checks), so the
suite is green on runners with no display at all.

**Docs are curated markdown; the tutorial compiles.** Through alpha the
API churns, so generated references (Doxygen) and site toolchains would
ship stale pages; GitHub renders `docs/*.md` fine. The tutorial is not a
snippet dump: it walks through `examples/tutorial/main.cpp`, a real
target built and `--verify`-checked in CI, so the documented code cannot
drift from code that runs. README stays the front door; GETTING_STARTED,
BUILDING and TUTORIAL are the three guides.

**Distribution: GitHub Releases, semver prerelease tags.** A `v*` tag
triggers `.github/workflows/release.yml`, which rebuilds, re-tests and
re-verifies on Windows/Linux/macOS, packages the six demo executables +
`companion/dist` + docs + CHANGELOG, and attaches per-OS archives plus a
`git archive` source tarball to the Release. Version lives in three
places that must agree: `CMakeLists.txt` `project(VERSION …)`,
`ply::kVersion` (`Version.hpp`, printed by the demo), and the git tag —
`v1.0.0-alpha.N` until the API stabilises. Changelog is Keep-a-Changelog.
Source-build-only was rejected: SDL3's Linux dependencies are a real
onboarding wall, and the CI matrix already builds all three OSes, so
binaries are nearly free.

**Hygiene that ships with the tag.** Issue templates (bug/feature),
`CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), CHANGELOG.md, and the
existing DCO flow. These are cheap, standard and reversible — they were
not up for debate, just done.

## Open branches (post-1.0-alpha)

- Embedded scripting (Lua binding)
- Browser/WASM build of the game (web distribution)
- Native Android/iOS apps
- WebRTC for internet play across networks (WebSocket/LAN first)
- Binary protocol (MessagePack) if profiling demands
