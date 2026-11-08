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
| **M7 — 1.0-alpha** | Next | Full demo, docs, tutorial |

Dependencies land at their milestone: SDL3 + glad (M1), GLM + stb + nlohmann/json (M2/M4), miniaudio + Crow + asio + mdns (M5/M6).

## Verified per milestone

Each milestone ships with checks that can actually fail, run in CI:

- `ctest` — headless unit tests (atlas UV math, ECS, ECS stress, timestep, transforms).
- `ctest` under `-DPLY_SANITIZE=ON` — the same suite with ASan + UBSan, which is
  how the `remove<T>` use-after-free was found and proven fixed.
- `ply_hello --verify` — renders and asserts content size/centring.
- `ply_ecs --verify` — runs the ECS demo headlessly and asserts pixels were drawn.
- `ply_assets --verify` — loads a real PNG + TTF + spritesheet fixture and asserts pixels were drawn.
- `ply_physics --verify` — runs the tilemap physics demo headlessly and asserts pixels were drawn *and* that crates came to rest.
- `ply_companion_server` — starts a real server, then speaks HTTP and a hand-written WebSocket handshake/join/input round-trip.
- `ply_companion --verify` — renders the phone-controlled demo including the room-code HUD.
