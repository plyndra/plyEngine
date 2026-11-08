# Changelog

All notable changes to plyEngine are documented here.
Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html) with
`-alpha.N` prerelease tags until 1.0.0.

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
  and a source tarball to each GitHub Release.
- **Community (M7)** — issue templates, `CODE_OF_CONDUCT.md`, this
  changelog, and CI matrix (Windows/Linux/macOS + sanitizer job).

### Known limitations

- Native-only through 1.0-alpha: no scripting, no web/WASM build, no
  mobile targets (see `docs/ROADMAP.md`).
- Companion protocol is WebSocket/LAN only; no internet play.
- The reference demo is content-light by design — it exists to prove the
  engine, not to be a finished game.

[1.0.0-alpha.1]: https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-alpha.1
