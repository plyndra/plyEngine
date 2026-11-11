# Changelog

All notable changes to plyEngine are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html) with
`-alpha.N` prerelease tags until 1.0.0.

## [Unreleased]

Work toward the 2.0 top-down simulation target (`docs/planning/ROADMAP_TARGET.md`),
landed on `main` unit by unit. The 1.x public API stays frozen: everything below
is additive.

### Added

- **UI framework (M1a)** — `UIAtlasLoader` (regions, 9-slice margins declared or
  derived from the alpha contour, stretch/tile modes), `NineSlice` geometry,
  anchor+offset+scale integer layout (`layoutWidgets`), the `Bar`/`Slot`/
  `Hotbar`/`Panel`/`Tooltip` widgets, `PixelText` (BMFont, ink bounds, stable
  glyph hashes), `UIRouter` with consume-before-world and press capture, the
  source-agnostic input pump (`InputRouting`), `HudFrame` (the HUD-over-world
  composition as data), `RenderTargetPlan` (integer upscale policy, floor
  camera snap, scene layer order), `Minimap`, and `MapLoader` with the static
  tileset slice.
- **`PresentPass`** — the render-target GL half: a framebuffer + colour texture
  at the plan's internal resolution, the scene drawn between `begin()`/`end()`,
  and a `GL_NEAREST` present blit to `plan.destination` (exact k×k blocks, never
  a blended pixel; a destination larger than the window is impossible above
  factor 1, and the factor-1 clip is snapped to the factor grid).
- **16 new headless test suites** (31 total), including the one GL-context
  suite: the present blit is graded by structural readback under SDL's
  offscreen video driver — computed from pattern data, not a pixel golden —
  and skips loudly where no GL context exists.

### Changed

- The reference platformer demo renders its world through an integer-scaled
  render target (nearest upsampling, letterboxed at the plan's destination) in
  a7's layer order, with the HUD drawn above the upscale. Its pause plate is
  now a real HUD widget: clickable to resume, routed through `UIRouter`
  (interactive exactly while it is drawn), and the demo's input conversion
  divides the cursor by the present factor from the plan rather than a
  hardcoded 1.

## [1.0.1] — 2026-11-09

### Fixed

- **The demos now stay open when you run them without flags.** Every example
  ended in `app.runFrames(frames > 0 ? frames : N)` and therefore closed its
  window in well under a second — `./ply_companion` printed a room code and
  exited before any phone could join, which made the flagship path unusable for
  anyone who followed the download instructions. The default is now `app.run()`
  (interactive until the window closes); `--frames`, `--verify` and
  `--screenshot` keep the bounded behaviour that CI and the capture pipeline
  depend on. Found while capturing the README hero animation: the v1.0.0 gates
  never exercised this path, because every CI step passes `--frames`.
- `ply_companion` gained `--dump-frames <dir>` (with `--dump-stride`,
  `--dump-from`, `--dump-count`) so a scripted run can be encoded into the
  documentation animation without a screen recorder.

## [1.0.0] — 2026-11-09

First stable release. The public API is frozen: additions are minor changes,
removals or renames need a deprecation cycle. Behaviour can still change to fix
a bug.

What "stable" rests on, so it can be checked rather than believed:

- **Freeze enforced mechanically.** `scripts/release-check.sh` diffs every
  header under `engine/include/` against the `v1.0.0-beta.1` freeze tag and
  fails on any removed or renamed line unless the change is recorded in
  `docs/DESIGN.md` and explicitly allowed.
- **Artifacts are executed, not trusted.** For every tag, the Linux archive is
  unpacked on a toolchain-free Ubuntu 22.04 container, every advertised binary
  is asserted present and run headless, the glibc floor is read from
  `objdump -T`, the phone controller is served over HTTP and must return 200,
  and the source tarball must build the tutorial plus a `find_package`
  consumer on a clean machine.
- **Two documented consumption paths beyond FetchContent:**
  `find_package(plyEngine CONFIG REQUIRED)` after `cmake --install` (built and
  consumed in CI on Linux, Windows and macOS), and a vcpkg git registry hosted
  by this repository (`vcpkg install plyengine` with a
  `vcpkg-configuration.json` entry — verified end to end from the public
  repo). The conan recipe is verified locally (`conan create` + consumer) but
  ships as an overlay/local recipe, since Conan Center only accepts recipes
  through its own index.

### Changed since 1.0.0-beta.4

- `ply_companion` resolves its web root automatically (source checkout:
  `companion/dist`, release archive: `companion/`), so the phone controller
  works from an unpacked release without extra flags.
- Tutorial build time is measured and documented (1m46s configure+build on 12
  cores, including SDL3 from source) instead of estimated.

### Notes

- Outside-consumer reports are wanted evidence but are no longer a release
  gate; the reasoning and its consequences are recorded in `docs/DESIGN.md`
  ("1.0.0 promotion rule") and `docs/ROADMAP.md`.

## [1.0.0-beta.4] — 2026-11-09

Additive release during the beta soak; the API freeze stays intact (one new
static helper, no signature changes).

### Fixed

- **`ply_companion` served 404 to every phone when run from a release archive.**
  The demo's default web root was `companion/dist` (Vite's build output), but
  the shipped archive lays the built PWA out at `companion/`, so the host
  printed a LAN URL and every player got `404 not found: index.html` unless
  they knew to pass `--web companion`. New
  `ply::CompanionServer::resolveWebRoot()` picks the first candidate directory
  that actually contains `index.html`; the demo now tries
  `companion/dist` then `companion`, and an explicit `--web` still wins.
  Measured before/after on the shipped binary: `GET /` was 404, now 200.

### Added

- **The release gate now proves the phone path.** `smoke-linux` asserts
  `companion/{index.html,app.js,index.css}` exist in the unpacked archive and
  serves the shipped binary, requiring `GET /` and `GET /app.js` to return 200.
  Previously the check was `[ -d companion ]` plus headless `--verify`, neither
  of which fetches a URL — so a web root pointing at a missing directory could
  ship while every gate stayed green (which is exactly what happened in
  beta.2/beta.3).

### Docs

- The tester-announcement copy is audited against the repo, with an audit table
  recording how each claim was checked, and the tutorial's build time is now
  measured (1m46s configure+build on 12 cores) rather than guessed.

## [1.0.0-beta.3] — 2026-11-09

Additive release during the beta soak; the API freeze stays intact.

### Added

- **The repository itself is a vcpkg git registry.** `ports/plyengine/` and
  `versions/` live at the repo root and are kept valid by
  `scripts/sync-vcpkg-registry.sh` (drives `vcpkg format-manifest` +
  `x-add-version`; the version database is generated, not hand-hashed).
  Consumers add a `vcpkg-configuration.json` pointing at the repo — no
  upstream submission needed. Verified end to end: fresh clone, manifest
  consumer, `vcpkg install`, link, `ply::kVersion` printed.
- **`PLY_CROW_SOURCE_DIR`** — system-deps mode stages a full Crow checkout
  instead of a Crow CMake config package: Conan Center has no usable Crow
  recipe, so the conan recipe stages Crow v1.3.5 and asio comes from the
  Conan Center asio package. The vcpkg `find_package(Crow)` path is
  unchanged and remains the default.
- **Fresh-host prerequisites documented** for vcpkg (zip/unzip, pkg-config,
  autoconf chain, SDL3 dev libs) and for the tutorial ("Before you start"
  block in docs/TUTORIAL.md).

### Fixed

- `.gitignore`'s unanchored `vcpkg/` pattern shadowed `packaging/vcpkg/`, so
  the entire vcpkg port directory had never been committed (tag
  `39cd37a`-era fix, present in this tag's source tarball).
- vcpkg port: `supports` expression parenthesized per manifest schema;
  portfile stages Lua 5.4.7 (`PLY_LUA_SOURCE_DIR became a hard requirement
  when M9 landed) with a verified archive hash.
- `docs/CONSUMER_REPORTS.md` added so outside-consumer build reports can be
  logged as beta-soak criterion-2 evidence.

## [1.0.0-beta.2] — 2026-11-08

Additive release during the beta soak: the public API only grew (see
`scripts/release-check.sh`, which enforces that).

### Added

- **Lua 5.4 scripting** (`ply::ScriptEngine`). A script gets a sandboxed `ply`
  API — `spawn(x, y)`, `destroy`, `alive`, `entity_count`, position and
  velocity getters/setters, `log` — plus an optional `update(dt)` hook called
  once per fixed step. `io`, `os` and `debug` are not loaded; `print` is
  redirected to the host log. Entities cross the boundary as
  generation-checked handles, so a stale reference is detectable from Lua.
  Hand-bound (no sol2): ten functions and one metatable. New demo
  `ply_scripting` and `scripting` ctest suite.
- **Offline build mode** `PLY_USE_SYSTEM_DEPS=ON`: no FetchContent at all —
  SDL3/glm/nlohmann_json/Crow/asio via `find_package`, and
  stb/miniaudio/mdns/Lua from `PLY_*_SOURCE_DIR` directories. Verified end to
  end (15/15 tests).
- **Packaging scaffolding** in `packaging/`: a vcpkg port and a conan recipe
  built on that mode, plus `packaging/README.md` stating exactly what has and
  has not been executed. **Both are blocked:** the repository is private and
  both registries fetch sources anonymously, so the portfile fails with that
  explanation instead of a hash mismatch.
- **`scripts/release-check.sh`**: version consistency (CMake ↔ CHANGELOG ↔
  tag), public-API drift against the freeze tag (any removed line in
  `engine/include` fails unless `--allow-api-change`), documentation link
  check, and ctest. Wired into CI.
- **Beta-soak exit criteria** in `docs/ROADMAP.md`, with the mechanical parts
  enforceable rather than remembered.

### Changed

- **glad is vendored** (`third_party/glad/`, generated output committed with
  provenance and licences). Builds no longer need Python or `jinja2`, and the
  loader is no longer generated during the build — which removes the two CI
  failures and the release-run PyPI breakage that generation caused.
- `PLY_BUILD_SCRIPTING` option (default `ON`) gates the scripting layer.
- Release checklist, DESIGN and BUILDING document the new dependency modes.

### Fixed

- Documentation no longer claims a `jinja2` requirement, and the build no
  longer has one.

## [1.0.0-beta.1] — 2026-11-08

First beta. The public API is now frozen for 1.0 (additions are minor;
removals and renames require a deprecation cycle).

### Added

- **Installable SDK.** `cmake --install` writes the engine archive, its
  internal static archives, the public headers and a
  `plyEngineConfig.cmake`, so a consumer can use
  `find_package(plyEngine CONFIG REQUIRED)` and link
  `plyEngine::plyEngine` instead of vendoring the whole tree. Only
  `Threads`, `SDL3`, `glm` and `nlohmann_json` are required at consumer
  configure time — Crow/asio, mdns, stb and miniaudio are header-only and
  compiled into the engine's objects. New `PLY_INSTALL` and
  `PLY_BUILD_DEPS_INSTALL` options (both default `ON`).
- **Four players in the reference demo.** Slots 1-4 drive P1-P4, each with
  its own colour, score, respawn and HUD entry; coins are contested and the
  first player to the flag wins. Keyboard fallback covers P1/P2, phones
  cover all four.
- **Sound in the reference demo.** The demo synthesises four short WAVs at
  startup (no binary assets in the repo) and plays them from gameplay cues:
  jump, coin, death and win. The game logic emits `demo::Sfx` events and
  stays audio-free, so both the headless suite and `--verify` can assert
  cues were requested and played.
- **CI consumer build.** A scratch project that only does `find_package` +
  link is built on Linux, Windows and macOS, so an export regression fails
  CI instead of the first outside user.

### Changed

- `ply_companion --verify` now asserts four independent players plus the
  four sound cues, and the scripted Autopilot probes all three non-scripted
  slots.
- `tests/platformer.cpp` asserts cue emission, contested-coin totals and
  per-player displacement; the demo carries four player colours.
- Reference demo, README, GETTING_STARTED, BUILDING, TUTORIAL, DESIGN and
  ROADMAP updated for beta, the install path and four players.

### Fixed

- SDL3 would not have been installed by `cmake --install` (its
  `SDL_INSTALL` defaults to off for subprojects), which would have broken
  `find_dependency(SDL3)` for every consumer.

## [1.0.0-alpha.2] — 2026-11-08

Supersedes `1.0.0-alpha.1`. If you downloaded the previous Linux archive, it
required glibc 2.43 and would not start on current distros — use this one.

### Fixed

- **Consuming plyEngine from your own project failed to compile.** The C++20
  requirement was directory-scoped, so it did not reach targets that link
  `plyEngine`; a scratch `add_subdirectory` consumer failed on defaulted
  `operator==` in `Entity.hpp`. `plyEngine` now declares
  `target_compile_features(... PUBLIC cxx_std_20)`. Verified by building and
  running a consumer project against the engine.
- **Version could drift.** `ply::kVersion` was hand-written (`1.0.0-alpha.1`)
  while `project(VERSION)` said `1.0.0`. It is now generated from
  `PLY_VERSION_STRING` in `CMakeLists.txt` via `Version.hpp.in`, so the
  header cannot disagree with the build; the tag must match that string.
- **Linux archives did not run on current distros** (alpha.1): built on
  `ubuntu-latest`, they needed glibc 2.43. Linux release legs now build in a
  pinned `ubuntu:22.04` container (glibc 2.35 floor) and assert the floor
  from `objdump -T` before packaging.
- **Nothing verified the shipped artifact.** Added `smoke-linux`, which
  unpacks the release archive on a clean Ubuntu 22.04 with no compiler or
  CMake, installs only runtime libraries, and runs both headless checks;
  and `smoke-source`, which builds the tutorial from the published source
  tarball. The Release is not published unless both pass.
- **PWA files leaked onto the releases page** as standalone assets; the
  release job now downloads only the `package-*` artifacts.
- **`-alpha.N` releases were not flagged as prereleases**, so GitHub
  advertised an alpha as the latest stable release.

### Added

- **"Using plyEngine in your own project"** in `docs/TUTORIAL.md` and
  `docs/GETTING_STARTED.md`: `FetchContent`/`add_subdirectory` snippets,
  with what propagates and what to turn off.
- **Runtime requirements for prebuilt binaries** in `docs/BUILDING.md`,
  including the exact headless command (`xvfb`, Mesa, fonts).
- `CODE_OF_CONDUCT.md` is now included inside the release archives.
- Richer GitHub Release body: what each binary is, how to run it, per-OS
  requirements.

### Changed

- `ply_companion --verify` raises `--frames` to the 1800 needed for the
  scripted playthrough, so the pre-M7 CI invocation cannot fail for lack of
  simulated time.
- Tutorial corrections: real pickup code (no invented helper), correct line
  count, and audio/asset coverage attributed to the tests that actually
  cover them.

## [1.0.0-alpha.1] — 2026-11-08

First tagged release. Everything below shipped across milestones M0–M7.

### Added

- **Foundation** — SDL3 (pinned) window/input/audio, glad GL loader,
  CMake `FetchContent` with exact pins, MIT licence, DCO contribution flow.
- **Renderer** — OpenGL 3.3 core sprite batcher, texture atlases,
  centre-anchored sprites, orthographic camera, sRGB passthrough.
- **ECS** — archetype (Bevy-style SoA) storage, packed index+generation
  entity handles, hierarchical transforms with dirty-flag propagation.
- **Loop** — `ply::App` with a fixed 60 Hz timestep and render interpolation.
- **Assets** — PNG decoding, Aseprite/TexturePacker/generic spritesheet
  loaders, TTF text baked to a glyph atlas, refcounted `AssetCache<T>`.
- **Simulation** — per-axis swept AABB physics, tilemap collision,
  spatial-hash broadphase, debug draw, miniaudio sound effects.
- **Companion** — embedded Crow HTTP+WebSocket server, versioned JSON
  protocol, mDNS discovery, 4-character room codes, fixed-step input queue,
  reference TypeScript+Vite PWA so phones act as controllers.
- **Reference demo (M7)** — a two-player single-screen platformer: coins and
  scoring, patrolling saws, a pit, a staircase, goal flag, pause/restart,
  phone slots 0/1 driving P1/P2 with keyboard fallback.
- **Tutorial (M7)** — `ply_tutorial`, a complete first game, plus
  `docs/TUTORIAL.md`, `docs/GETTING_STARTED.md` and `docs/BUILDING.md`.
- **Verification (M7)** — 14 headless `ctest` suites (including a scripted
  full platformer playthrough) green in Release and under ASan+UBSan; every
  demo has a `--verify` that asserts real outcomes.
- **Release engineering (M7)** — tag-triggered GitHub Actions workflow
  attaching prebuilt demo binaries for Windows/Linux/macOS, the built PWA
  and a source tarball to each GitHub Release. Linux binaries are built in a
  pinned Ubuntu 22.04 container (glibc 2.35 floor) and the job asserts that
  floor with an `objdump -T` symbol check before packaging.
- **Community (M7)** — issue templates, `CODE_OF_CONDUCT.md`, this
  changelog, and CI matrix (Windows/Linux/macOS + sanitizer job).

### Fixed

- **Windows build (M6 code, caught at M7)** — `CompanionServer` and
  `MdnsAdvertiser` used POSIX `close()`/`setsockopt()` signatures and the
  companion integration test used POSIX headers, `mkdir()` and `/tmp`, so the
  Windows job could not compile them. Now behind `engine/src/SocketCompat.hpp`
  plus `std::filesystem` temp dirs, with `WSAStartup` guaranteed.
- **macOS/Windows CI (since M4)** — the runner images stopped shipping
  `jinja2`, which glad's loader generation needs; CI now provides it via
  `PYTHONPATH` for every interpreter CMake might resolve.

### Known limitations


- Native-only through 1.0-alpha: no scripting, no web/WASM build, no
  mobile targets (see `docs/ROADMAP.md`).
- Companion protocol is WebSocket/LAN only; no internet play.
- The reference demo is content-light by design — it exists to prove the
  engine, not to be a finished game.

[1.0.0-beta.2]: https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-beta.2
[1.0.0-beta.1]: https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-beta.1
[1.0.0-alpha.2]: https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-alpha.2
[1.0.0-alpha.1]: https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-alpha.1
