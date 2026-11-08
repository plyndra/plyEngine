# plyEngine — Roadmap

| Milestone | Exit criteria |
|---|---|
| **M0 — Scaffold** | MIT LICENSE, README, CONTRIBUTING (DCO), CMake + vcpkg manifest, CI matrix, repo layout |
| **M1 — Hello window** | SDL3 + OpenGL 3.3 core context, clear screen, textured quad |
| **M2 — Renderer** | Sprite batching, texture atlas, ortho camera, input |
| **M3 — ECS core** | Archetype ECS, fixed-step game loop, transform hierarchy |
| **M4 — Assets** | PNG/TTF/spritesheet/JSON scene loaders |
| **M5 — Simulation** | AABB physics + tilemap collision, SDL3 audio |
| **M6 — Companion** | Crow server + JSON protocol + room code/QR join + reference PWA; platformer playable from a phone |
| **M7 — 1.0-alpha** | Full demo, docs, tutorial |

Dependencies land at their milestone: SDL3 + GL loader (M1), GLM (M3), Crow (M6).
