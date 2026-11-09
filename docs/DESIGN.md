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
`git archive` source tarball to the Release. Tags are
`v1.0.0-alpha.N` until the API stabilises and are published as GitHub
*prereleases*. Changelog is Keep-a-Changelog. Source-build-only was
rejected: SDL3's Linux dependencies are a real onboarding wall, and the CI
matrix already builds all three OSes, so binaries are nearly free.

**The version is authored once.** `PLY_VERSION_STRING` in `CMakeLists.txt`
is the single source; `ply::kVersion` is a *generated* header
(`Version.hpp.in` → `build/generated/plyEngine/Version.hpp`), so the code
cannot drift from the build. The git tag must equal that string and the
release checklist checks it; the demos print it at startup. (Before this,
the hand-written header, `project(VERSION)` and the tag were three values
that could disagree — they did, at `1.0.0` vs `1.0.0-alpha.1`.)

**The engine is a CMake subproject, not an installed SDK.** Consumers use
`add_subdirectory` or `FetchContent` with `PLY_BUILD_TESTS`/`
`PLY_BUILD_EXAMPLES` off, then link `plyEngine`. `plyEngine` declares
C++20 with `target_compile_features(... PUBLIC cxx_std_20)` and exports its
include dirs publicly, so nothing about the requirement is implicit — the
first consumer build proved that a directory-scoped `CMAKE_CXX_STANDARD`
leaks nothing to the linking target, and the fix is a public compile
feature. An installable `find_package(plyEngine)` package is deliberately
deferred: it would have to republish the pinned FetchContent dependencies
(glad, GLM, json), which is post-alpha work.

**Hygiene that ships with the tag.** Issue templates (bug/feature),
`CODE_OF_CONDUCT.md` (Contributor Covenant 2.1), CHANGELOG.md, and the
existing DCO flow. These are cheap, standard and reversible — they were
not up for debate, just done.

**A packaged binary is a claim, so the release asserts it.** The first
tagged build taught the lesson the hard way: binaries built on
`ubuntu-latest` demanded glibc 2.43 and would not start on Ubuntu 22.04,
Debian 12, or even the maintainer's own host. "It built in CI" is not
"it runs for users", and the CI matrix's passing tests never touched the
artifact. The release therefore has three artifact gates, all of which must
pass before anything is published:

1. **Toolchain floor** — Linux legs build inside a pinned `ubuntu:22.04`
   container.
2. **Unpack and run** — `smoke-linux` extracts the *shipped archive* on a
   clean Ubuntu 22.04 with no compiler and no CMake, installs only runtime
   libraries (xvfb + Mesa), and runs both headless `--verify` checks, plus
   an `objdump -T` assertion that no required `GLIBC_*` symbol exceeds 2.34.
3. **Source tarball builds** — `smoke-source` configures and builds
   `ply_tutorial` from the published `git archive`, proving the tarball is
   complete.

The floor (glibc 2.35) and the runtime library list are documented in
`docs/BUILDING.md`. The generalisable rule: a test suite that runs against
the build tree says nothing about the artifact; the artifact must be
executed, on a machine unlike the build machine.

## Beta: API freeze and distribution (settled in M8)

**The public API is frozen at `1.0.0-beta.1`.** Every header under
`engine/include/plyEngine/` is the contract; from here, additions are minor
and removals or renames require a deprecation cycle (keep the old spelling,
mark it `[[deprecated]]`, remove no earlier than 1.1). Bug fixes may still
change behaviour. The point of freezing before 1.0 is that an engine with
zero stable surface cannot accumulate users; churn after adoption costs far
more than a slightly imperfect name.

**The engine installs; it is no longer subproject-only.** `cmake --install`
writes the engine archive, its internal static archives (glad, stb, miniaudio),
the public headers, and `plyEngineConfig.cmake`, so consumers call
`find_package(plyEngine CONFIG REQUIRED)` and link `plyEngine::plyEngine`.
Three constraints shaped that design:

- **Header-only dependencies must not reach the consumer.** Crow/asio, mdns,
  stb and miniaudio are only used to *compile* engine translation units, so
they are `BUILD_INTERFACE`-only; a consumer never configures them. Only what
  the public headers expose — GLM and nlohmann/json — is a real dependency,
  along with SDL3 (a static library that must be on the link line) and
  `Threads`.
- **Third-party include paths stay PRIVATE.** No public header includes glad,
  stb, miniaudio, Crow or mdns, so a `BUILD_INTERFACE`-scoped glad include dir
  is enough for our own examples and tests and nothing third-party is
  installed except what the API genuinely exposes.
- **SDL3 owns its own export set.** Adding it to ours makes CMake demand
  `SDL3_Headers` join too, which cascades; instead the config file appends
  `SDL3::SDL3` to the imported target after `find_dependency(SDL3)`. SDL3 only
  installs itself when `SDL_INSTALL` is on, which defaults off for subprojects —
  easy to miss, and the reason the install test builds from a *clean prefix*.

The install path is guarded the same way the archive was: a consumer project
that only does `find_package` + link is built in CI, so an export regression
fails the pipeline rather than the first outside user.

**Demo: four players and sound, because shipped subsystems must be
visible.** Slots 1-4 map to P1-P4 with per-player colour, score and respawn;
keyboard fallback covers P1/P2, phones cover all four. Audio is exercised end
to end without shipping binary assets: the demo synthesises four short WAVs at
startup and plays them from *gameplay cues*. The game logic stays audio-free —
it emits `Sfx` events — so the headless suite asserts "a coin cue was emitted"
and the demo asserts "the cue was played". That split is the rule: logic
produces intents, the host produces side effects, and tests can check both
independently.

## Scripting: scope, sandbox and dependency choices (settled in M9)

**Lua, hand-bound, gameplay-only.** Scripts get entity lifecycle, position and
velocity, plus an `update(dt)` hook. They do not get rendering, asset loading
or the loop: a script moves things, it does not own the frame. That boundary
keeps the fixed-step determinism story intact — the script is called from
inside the step, with the step's dt, and cannot touch time.

**No sol2.** The whole API is ten functions and one metatable, so a ~400-line
C binding is smaller, compiles in seconds, and has no C++20 template surface to
break on MSVC or older Clang. A heavier binding is a post-1.0 question, not a
prerequisite for the feature.

**Sandbox by omission.** `luaL_openlibs` would hand a game script `io`, `os`
and `debug`. The binding opens `base`, `string`, `table`, `math`, `coroutine`
and `utf8` only, and redirects `print` into the host's log. A script that can
`os.execute` is a remote-code hole in a networked party game, and the companion
server is exactly that.

**Entities cross the boundary as generation-checked handles.** Lua holds a
userdata wrapping `Entity`, not a bare integer index, so a destroyed-then-
reused slot cannot silently alias: `ply.alive(stale)` is false, and `first ~=
second` after recycling. `ply.spawn(x, y)` creates Transform +
GlobalTransform + Velocity, because a "gameplay object" that cannot move is a
trap for script authors.

**Lua is pinned and vendored into the build.** Lua 5.4.7 source is fetched by
`FetchContent` in the default build and taken from `PLY_LUA_SOURCE_DIR` in
offline builds, exactly like the other dependencies. `Scripting.hpp` exposes no
Lua types, so consumers never see `lua.h`; the static Lua archive is exported in
the install export set for the same reason glad and stb are.

## Vendored tooling and offline builds (settled in M9)

**The OpenGL loader is committed, not generated.** glad output is deterministic
for a given spec, and generating it needs Python + `jinja2` + a Khronos fetch at
build time. That combination broke CI twice (runner images dropped `jinja2`) and
blocked offline builds, so `third_party/glad/` holds the generated loader with
its provenance, spec and licences written down. Regeneration is a documented,
rare operation.

**`PLY_USE_SYSTEM_DEPS` exists because package managers forbid network in
build.** It switches the five CMake-packaged dependencies to `find_package` and
takes stb/miniaudio/mdns/Lua from packager-staged source directories. It is
verified end to end locally (configure, build, 15/15 tests) against a prefix
containing SDL3, glm, nlohmann_json and Crow.

**Registry distribution is blocked on repository visibility, not on code.** The
repository is private; vcpkg and conan both fetch anonymously. `packaging/`
holds both recipes plus a hard gate that fails with that explanation instead of
a hash mismatch, and `packaging/README.md` states exactly what has and has not
been executed. Shipping a recipe labelled "works" without running it would be
the same unverified-claim mistake that produced the glibc-2.43 archive.

## Open branches (post-1.0)

- Embedded scripting (Lua binding)
- Browser/WASM build of the game (web distribution)
- Native Android/iOS apps
- WebRTC for internet play across networks (WebSocket/LAN first)
- Binary protocol (MessagePack) if profiling demands

## 1.0.0 promotion rule (decided 2026-11-09)

**The owner removed the outside-consumer requirement from the 1.0.0 gate.** The
rule recorded in `docs/ROADMAP.md` since the alpha.2 incident ("someone who is
not us reports a successful build") was replaced on 2026-11-09 by: outside
builds are wanted evidence, not a precondition.

Why the original rule existed and why it is being narrowed: `v1.0.0-alpha.1`
passed CI while shipping binaries that needed glibc 2.43 and started nowhere
current. That failure was mechanical — the artifact was never executed — and it
is now covered by gates that do execute it: `smoke-linux` unpacks the archive on
a toolchain-free Ubuntu 22.04, runs the headless demos, asserts the glibc floor,
and (since beta.4) serves the shipped phone controller over HTTP and requires
200s. What remains unproven without outside users is ergonomics: whether the
consumption paths are pleasant and correctly documented. That is a quality
argument, not a correctness one, so it no longer blocks the tag.

Consequences, stated so they are not rediscovered as surprises:

- `docs/CONSUMER_REPORTS.md` keeps collecting reports; an empty table is not a
  release blocker and must not be cited as evidence of stability either.
- Promotion still requires: additive-only API drift vs the freeze tag, every
  gate green on `main`, a CHANGELOG entry, and this section updated with the
  promotion date.
- If a future release ships an API defect that an outside user would have
  caught in an afternoon, the honest response is to reintroduce the gate — not
  to argue that the reports were merely advisory.

## 1.0.0 released (2026-11-09)

Promoted from `1.0.0-beta.4`. Recorded here because the promotion rule above
requires it.

The state at promotion, all checkable by re-running the gates:

- additive-only drift against the freeze tag `v1.0.0-beta.1`
  (`scripts/release-check.sh`);
- 15 headless ctest suites green in Release and under ASan+UBSan;
- the Linux archive unpacked and executed on a toolchain-free Ubuntu 22.04
  container — every advertised binary run headless, the glibc floor asserted,
  and the phone controller served over HTTP with `GET /` and `GET /app.js`
  returning 200;
- the source tarball building the tutorial and a `find_package` consumer;
- the vcpkg registry resolving `plyengine` `1.0.0` from the public repository.

Outside-consumer reports at promotion time: **none**, which the owner
explicitly accepted. That is a deliberate, recorded trade-off rather than an
oversight: see the "1.0.0 promotion rule" section above and
`docs/ROADMAP.md`.

Next branch, in priority order: Web/WASM export (the post-1.0 branch the owner
picked), then mobile, then internet play and any binary-protocol work the
profiling demands.
