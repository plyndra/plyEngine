# Changelog

All notable changes to plyEngine are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html) with
`-alpha.N` prerelease tags until 1.0.0.

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
