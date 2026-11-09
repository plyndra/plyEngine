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
| **M7 — 1.0-alpha** | ✅ Done | Full reference demo (2 phone players at M7, coins/scoring, saws, pit, staircase, goal, pause/restart) with outcome-asserting `--verify`; `ply_tutorial` + step-by-step tutorial; GETTING_STARTED/BUILDING docs; release workflow with per-OS binaries that are unpacked and executed before publishing; issue templates, CoC, CHANGELOG; 14 ctest suites green in Release and ASan; tagged `v1.0.0-alpha.2` |

| **M9 — scripting & packaging** | ✅ Done | Lua 5.4 scripting layer (hand-bound, sandboxed, `update(dt)`) with demo + headless tests; vendored glad (no Python/generator step); `PLY_USE_SYSTEM_DEPS` offline build mode verified; vcpkg/conan recipes prepared and gated on repository visibility; `scripts/release-check.sh` enforces version/API-drift/link/ctest; tagged `v1.0.0-beta.2` |
| **M8 — 1.0-beta** | ✅ Done | Public API frozen; `find_package(plyEngine)` installable SDK with a CI consumer build; reference demo expanded to four phone players with synthesised sound cues asserted by `--verify`; tagged `v1.0.0-beta.1` |

Dependencies land at their milestone: SDL3 + glad (M1), GLM + stb + nlohmann/json (M2/M4), miniaudio + Crow + asio + mdns (M5/M6).

## The alpha gate (M7, as run)

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

## Beta soak: the exit criteria for v1.0.0

`v1.0.0-beta.1` is the freeze point. 1.0 is not a date; it is these three
criteria, all of which are checkable rather than remembered:

1. **The frozen API survives real work.** All work between beta.1 and 1.0
   must be additive to `engine/include/`. Mechanically enforced by
   `scripts/release-check.sh`, which diffs the public headers against the
   freeze tag and fails on any removed line unless explicitly overridden
   (`--allow-api-change`, which requires recording the decision in
   `docs/DESIGN.md`).
2. **At least one outside consumer builds it.** Someone who is not us
   reports a successful build through either consumption path
   (`find_package(plyEngine)` or `FetchContent`), with the version and OS.
   "No external users" is not evidence of stability, and this project's
   rule is that a claim needs evidence — the alpha.2 archive was the same
   mistake in a different costume.
3. **Every gate below still green**, on the tip of `main`, including the
   install-and-consume job on all three OSes.

Run the mechanical parts at any time:

```sh
scripts/release-check.sh --tag v1.0.0-beta.1   # version, API drift, links, ctest
```

If criterion 2 cannot be met within a reasonable window, the fallback is to
re-cut beta.N with a CHANGELOG note rather than to promote 1.0 on the
strength of our own tests alone. Promotion is a decision, and it gets
written down in `docs/DESIGN.md` when it happens.

## The beta gate (M8, as run)

1. Every alpha gate below, still green.
2. **Installable SDK** — `cmake --install` into a clean prefix, then a scratch
   consumer configures with `find_package(plyEngine CONFIG REQUIRED)` and links
   `plyEngine::plyEngine`. Built in CI on Linux / Windows / macOS.
3. **Four-player demo with audio** — `ply_companion --verify` asserts all four
   players moved independently and that jump/coin/death/win cues were emitted
   *and* played; `tests/platformer.cpp` asserts the same cues headlessly.
4. **API freeze declared** — `docs/DESIGN.md` states the policy: additions are
   minor, removals/renames need a deprecation cycle.

## After 1.0-beta

1. **`v1.0.0`** — the API is already frozen at beta; 1.0 needs the frozen
   surface to survive a release cycle without a breaking bug, plus any
   documentation gaps that outside users find.
2. Then the post-1.0 branches in `docs/DESIGN.md` (scripting, web/WASM,
   mobile, internet play, binary protocol).

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
- `ply_companion --verify` — plays a complete four-player match through the
  real companion-input queue (Autopilot → `CompanionInput.push` → slot
  routing → gameplay) and asserts score, deaths, independent movement of all
  four players, hazard patrol distance, the win state, the room-code HUD, and
  that the four gameplay sound cues were emitted and actually played.
- **Release workflow** — re-runs build + ctest + Linux `--verify` on all
  three OSes for every `v*` tag, then unpacks the Linux archive on a
  toolchain-free Ubuntu 22.04, runs it there, asserts the glibc floor, and
  builds the tutorial from the source tarball — all before publishing.
- **Consumer build** — a scratch project that `add_subdirectory`s the engine,
  links `plyEngine` and prints `ply::kVersion` is compiled during M7 review;
  it caught the missing public `cxx_std_20` requirement.
- `ply_scripting --verify` — loads a Lua script, runs 240 fixed steps, and
  asserts the script defined `update(dt)`, ran once per step, raised no error,
  and moved all six drones from where C++ placed them.
- **`scripts/release-check.sh`** — version consistency across CMake/CHANGELOG/
  tag, public-API drift against the freeze tag (removals fail unless
  `--allow-api-change`), doc links, and ctest. Runs in CI on Linux.
