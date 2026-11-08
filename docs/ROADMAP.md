# plyEngine — Roadmap

| Milestone | Status | Exit criteria |
|---|---|---|
| **M0 — Scaffold** | ✅ Done | MIT LICENSE, README, CONTRIBUTING (DCO), CMake, CI matrix, repo layout |
| **M1 — Hello window** | ✅ Done | SDL3 + OpenGL 3.3 core context, textured quad |
| **M2 — Renderer** | ✅ Done | Sprite batching, texture atlas, ortho camera, input |
| **M3 — ECS core** | ✅ Done | Archetype ECS, fixed-step game loop, transform hierarchy, `App` |
| **M4 — Assets** | ✅ Done | PNG decode, spritesheet loaders (Aseprite/generic/TexturePacker), TTF baking + text, refcounted asset cache, ECS `Sprite` |
| **M5 — Simulation** | ✅ Done | Per-axis swept AABB physics, tilemap collision, spatial hash broadphase, debug draw, miniaudio |
| **M6 — Companion** | ✅ Done | Crow HTTP+WebSocket server, versioned JSON protocol, mDNS discovery + room code, reference PWA, fixed-step input queue |
| **M7 — 1.0-alpha** | ✅ Done | Full reference demo (2 phone players, coins/scoring, saws, pit, staircase, goal, pause/restart) with outcome-asserting `--verify`; `ply_tutorial` + step-by-step tutorial; GETTING_STARTED/BUILDING docs; release workflow with per-OS binaries that are unpacked and executed before publishing; issue templates, CoC, CHANGELOG; 14 ctest suites green in Release and ASan; tagged `v1.0.0-alpha.2` |

Dependencies land at their milestone: SDL3 + glad (M1), GLM + stb + nlohmann/json (M2/M4), miniaudio + Crow + asio + mdns (M5/M6).

## The 1.0-alpha gate (M7 exit criteria, as run)

Tagging `v1.0.0-alpha.N` requires all of the following — each is a check
that can fail, not a claim:

1. **14 `ctest` suites pass** in Release **and** under `-DPLY_SANITIZE=ON`
   (ASan + UBSan) on the CI matrix (Windows / Linux / macOS + sanitizer job).
2. **Every demo `--verify` passes** headlessly under Xvfb, asserting real
   outcomes — including a full scripted platformer match (coins collected,
   deaths to pit and saw, player 2 moving independently, player 1 wins) and
   the tutorial collecting all five coins.
3. **The packaged archive runs.** A smoke job unpacks the Linux archive on a
   clean Ubuntu 22.04 with no compiler or CMake, installs only runtime
   libraries, and re-runs both headless checks; it also asserts the glibc
   floor from `objdump -T`. The source tarball must build the tutorial on its
   own. Nothing is published until these pass — this gate exists because
   `v1.0.0-alpha.1` originally shipped binaries needing glibc 2.43 that no
   current distro could run.
4. **Docs complete**: README, GETTING_STARTED, BUILDING, TUTORIAL,
   DESIGN, ROADMAP, CHANGELOG all current, including how to consume the
   engine from another project.
5. **Version consistency**: `PLY_VERSION_STRING` in `CMakeLists.txt` equals
   the git tag. `ply::kVersion` is a *generated* header (`Version.hpp.in`),
   so it cannot drift; the demos print it on startup.
6. **OSS hygiene**: issue templates, CODE_OF_CONDUCT, DCO CONTRIBUTING,
   semver prerelease tags, Keep-a-Changelog, GitHub release marked as a
   prerelease.

## After 1.0-alpha

Post-1.0-alpha branches live in `docs/DESIGN.md` ("Open branches"):
scripting (Lua), web/WASM build, native mobile, internet play (WebRTC),
binary protocol. Target: `v1.0.0` once the API stops churning and the
tutorial/demo content grows.

## Verified per milestone

Each milestone ships with checks that can actually fail, run in CI:

- `ctest` — 14 headless suites: atlas UV math, ECS, ECS stress, timestep,
  transforms, assets, spritesheet, image, physics, audio, companion
  protocol, companion server, and `platformer` (a scripted full playthrough
  of the reference demo's game logic, GL-free).
- `ctest` under `-DPLY_SANITIZE=ON` — the same suite with ASan + UBSan, which is
  how the `remove<T>` use-after-free was found and proven fixed.
- `ply_hello --verify` — renders and asserts content size/centring.
- `ply_ecs --verify` — runs the ECS demo headlessly and asserts pixels were drawn.
- `ply_assets --verify` — loads a real PNG + TTF + spritesheet fixture and asserts pixels were drawn.
- `ply_physics --verify` — runs the tilemap physics demo headlessly and asserts pixels were drawn *and* that crates came to rest.
- `ply_tutorial --verify` — plays the tutorial game with a built-in script and asserts all five coins were collected and the win state reached.
- `ply_companion_server` — starts a real server, then speaks HTTP and a hand-written WebSocket handshake/join/input round-trip.
- `ply_companion --verify` — plays a complete two-player match through the
  real companion-input queue (Autopilot → `CompanionInput.push` → slot
  routing → gameplay) and asserts score, deaths, independent P2 movement,
  hazard patrol distance and the win state, plus the room-code HUD.
- **Release workflow** — re-runs build + ctest + Linux `--verify` on all
  three OSes for every `v*` tag, then unpacks the Linux archive on a
  toolchain-free Ubuntu 22.04, runs it there, asserts the glibc floor, and
  builds the tutorial from the source tarball — all before publishing.
- **Consumer build** — a scratch project that `add_subdirectory`s the engine,
  links `plyEngine` and prints `ply::kVersion` is compiled during M7 review;
  it caught the missing public `cxx_std_20` requirement.
