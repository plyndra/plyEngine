# Call for testers — draft post

**Suggested venues** (in order): r/gamedev, r/cpp (Show & Tell), EngineDev /
C++ Discord #showcase, Hacker News "Show HN", lobster.gg.

---

**Title options**

1. Looking for a few testers: plyEngine 1.0.0-beta.2 — 2D C++20 engine with "your phone is the controller" multiplayer
2. I spent the last mile making my toy 2D engine installable — beta.2 needs real builds to earn its 1.0

---

**Body**

plyEngine is a small, free, MIT-licensed 2D game engine: C++20, SDL3 +
OpenGL 3.3, archetype ECS, swept-AABB physics, PNG/TTF assets, miniaudio
audio, an embedded sandboxed Lua 5.4 scripting layer, and a companion server
that turns phones into gamepads over the LAN — no app to install, the PWA
just opens in the browser.

To see what it looks like, the repo ships everything as `--verify`-able
demos:

- `ply_tutorial` — a five-coin tutorial game; docs/TUTORIAL.md is a
  step-by-step build-along.
- `ply_companion` — a **four-player** platformer where everyone plays on
  their phone (room code + mDNS discovery). Prebuilt binaries are in the
  release; unpack, run, have friends scan the QR code.
- `ply_scripting` — drones driven by a Lua script.

One binary that has no build requirement: download the release for your OS,
unpack, and run (Linux build is pinned to Ubuntu 22.04's glibc).

**What I'm asking for**

The honest version: the engine's public API is frozen as of beta, and I won't
call it 1.0 until someone other than me has built something with it. Concretely:

1. Pick one:
   - Build the tutorial from source (`docs/TUTORIAL.md`, ~15 minutes; needs
     cmake, a C++20 compiler and the SDL3/X11 dev packages — the exact
     apt package list is in its "Before you start" block),
   - or consume it your way: `find_package(plyEngine)` after
     `cmake --install`, or FetchContent straight into your CMakeLists, or
   - through vcpkg without any upstream PR: add a `vcpkg-configuration.json`
     next to your manifest —

     ```json
     {
       "default-registry": {
         "kind": "git",
         "repository": "https://github.com/microsoft/vcpkg",
         "baseline": "<your default, or a vcpkg master sha>"
       },
       "registries": [{
         "kind": "git",
         "packages": ["plyengine"],
         "repository": "https://github.com/plyndra/plyEngine",
         "baseline": "2e58e25f6caede252f102d6018082b67be70a4ee"
       }]
     }
     ```

     then `vcpkg install plyengine` (needs zip/unzip, pkg-config and the
     autoconf chain plus SDL3 dev libs on a vanilla Linux box — full list in
     `packaging/README.md`).
2. Tell me version + OS + compiler, and — importantly — anything that broke,
   including "the find_package docs are wrong about X".

Everything is on the repo: <https://github.com/plyndra/plyEngine>
(docs, CHANGELOG, CI runs green on Win/Linux/macOS, sanitizers included).

Not looking for money or stars — just real builds and real friction reports.
