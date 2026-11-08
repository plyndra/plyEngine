# plyEngine — Roadmap

| Milestone | Status | Exit criteria |
|---|---|---|
| **M0 — Scaffold** | ✅ Done | MIT LICENSE, README, CONTRIBUTING (DCO), CMake, CI matrix, repo layout |
| **M1 — Hello window** | ✅ Done | SDL3 + OpenGL 3.3 core context, textured quad |
| **M2 — Renderer** | ✅ Done | Sprite batching, texture atlas, ortho camera, input |
| **M3 — ECS core** | ✅ Done | Archetype ECS, fixed-step game loop, transform hierarchy, `App` |
| **M4 — Assets** | ✅ Done | PNG decode, spritesheet loaders (Aseprite/generic/TexturePacker), TTF baking + text, refcounted asset cache, ECS `Sprite` |
| **M5 — Simulation** | Next | AABB physics + tilemap collision, SDL3 audio |
| **M6 — Companion** | Planned | Crow server + JSON protocol + room code/QR join + reference PWA; platformer playable from a phone |
| **M7 — 1.0-alpha** | Planned | Full demo, docs, tutorial |

Dependencies land at their milestone: SDL3 + glad (M1), GLM (M2), Crow (M6).

## Verified per milestone

Each milestone ships with checks that can actually fail, run in CI:

- `ctest` — headless unit tests (atlas UV math, ECS, ECS stress, timestep, transforms).
- `ctest` under `-DPLY_SANITIZE=ON` — the same suite with ASan + UBSan, which is
  how the `remove<T>` use-after-free was found and proven fixed.
- `ply_hello --verify` — renders and asserts content size/centring.
- `ply_ecs --verify` — runs the ECS demo headlessly and asserts pixels were drawn.
- `ply_assets --verify` — loads a real PNG + TTF + spritesheet fixture and asserts pixels were drawn.
