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
| **M7 — 1.0-alpha** | ✅ Done | Full reference demo (2 phone players, coins/scoring, saws, pit, staircase, goal, pause/restart) with outcome-asserting `--verify`; `ply_tutorial` + step-by-step tutorial; GETTING_STARTED/BUILDING docs; release workflow with per-OS binaries; issue templates, CoC, CHANGELOG; 14 ctest suites green in Release and ASan; tagged `v1.0.0-alpha.1` |

Dependencies land at their milestone: SDL3 + glad (M1), GLM + stb + nlohmann/json (M2/M4), miniaudio + Crow + asio + mdns (M5/M6).

## The 1.0-alpha gate (M7 exit criteria, as run)

Tagging `v1.0.0-alpha.1` requires all of the following — each is a check
that can fail, not a claim:

1. **14 `ctest` suites pass** in Release **and** under `-DPLY_SANITIZE=ON`
   (ASan + UBSan) on the CI matrix (Windows / Linux / macOS + sanitizer job).
2. **Every demo `--verify` passes** headlessly under Xvfb, asserting real
   outcomes — including a full scripted platformer match (coins collected,
   deaths to pit and saw, player 2 moving independently, player 1 wins) and
   the tutorial collecting all five coins.
3. **Docs complete**: README, GETTING_STARTED, BUILDING, TUTORIAL,
   DESIGN, ROADMAP, CHANGELOG all current.
4. **Release automation**: tag-triggered workflow builds/tests/packages on
   three OSes and attaches binaries + PWA + source tarball to the Release.
5. **OSS hygiene**: issue templates, CODE_OF_CONDUCT, DCO CONTRIBUTING,
   versioning `v1.0.0-alpha.N` (semver prerelease), Keep-a-Changelog.

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
  three OSes for every `v*` tag before publishing artifacts.
