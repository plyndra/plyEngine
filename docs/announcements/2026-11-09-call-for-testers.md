# Call for testers — publication copy (v1.0.0-beta.3, 2026-11-09)

Tracked here so the published text is reviewable history. Reports get logged
in [../CONSUMER_REPORTS.md](../CONSUMER_REPORTS.md).

Every factual claim below was checked against the repo at `8566cc9` and the
published `v1.0.0-beta.3` release on 2026-11-09 (see the audit table at the
end). Nothing here is aspirational.

**Venue order** (council recommendation, owner-approved): EngineDev / C++
Discord `#showcase` first as a soft launch → r/gamedev → r/cpp Show & Tell →
Show HN only after ~2 weeks with no qualifying report. Never cross-post the
same day. Keep the version pinned at beta.3 across venues so reports stay
comparable. r/gamedev requires a post flair (use *Feedback Request*) and its
self-promotion rule needs context, not a bare link — this copy is written that
way, but confirm the current sidebar rules before posting.

---

## Title (r/gamedev)

Looking for a few testers: plyEngine 1.0.0-beta.3 — a C++20 2D engine where phones are the controllers

(102 characters; Reddit's limit is 300.)

---

## Body (r/gamedev)

I've been building plyEngine, a small MIT-licensed 2D game engine: C++20,
SDL3 + OpenGL 3.3, an archetype ECS, swept-AABB tilemap physics, PNG/TTF
assets, miniaudio audio, and a sandboxed Lua 5.4 scripting layer.

The part people find fun: **your phone is the controller.** The host runs the
game, prints a LAN URL and a 4-character room code, and up to four people open
that URL in their phone browser — no app install — and play a four-player
platformer together. Coins are contested and the first to the flag wins.

The reason I'm posting: the public API has been frozen since `v1.0.0-beta.1`,
but I won't tag 1.0.0 on the strength of my own tests. I need at least one
person who is not me to build something with it and tell me what broke.

### The 10-second version (no build, no toolchain)

Download an archive, unpack, run:

```sh
# Linux x86-64 / Windows x86-64 / macOS Apple Silicon (arm64 only)
./ply_companion --web companion     # serves the phone controller from companion/
```

It prints a room code and a URL like `http://192.168.1.20:49152`; have two or
three people on the same Wi-Fi open that URL, type the room code, and play.
`./ply_tutorial` is the completed five-coin game from the tutorial, for a
single keyboard player.

Release: <https://github.com/plyndra/plyEngine/releases/tag/v1.0.0-beta.3>
(Each archive is unpacked and executed on a clean Ubuntu 22.04 container
before publishing, so a broken archive fails CI rather than you.)

### The version I actually need (a build)

Any one of these is enough — pick the least annoying:

1. **Source build of the tutorial** (~15 min on a warm toolchain):
   <https://github.com/plyndra/plyEngine/blob/main/docs/TUTORIAL.md>
   Needs CMake ≥ 3.24, a C++20 compiler, and the SDL3/X11 dev packages; the
   exact apt list is in the tutorial's "Before you start" block.
2. **FetchContent** into your own CMake project (`GIT_TAG v1.0.0-beta.3`) —
   the README has a 6-line example.
3. **`find_package(plyEngine)`** after `cmake --install`.
4. **vcpkg**: the repo hosts its own vcpkg registry, so no upstream port is
   needed. Add this next to your `vcpkg.json`:

```json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "ec48c11eae9448c8c6e7739d93e06fa2433aa95f"
  },
  "registries": [{
    "kind": "git",
    "packages": ["plyengine"],
    "repository": "https://github.com/plyndra/plyEngine",
    "baseline": "a8e1a35b2301c55dd80708edc93010d0a1832aff"
  }]
}
```

   then `vcpkg install plyengine`. On a vanilla Linux box this also wants
   `zip unzip pkg-config` and the autoconf/libtool chain; full list in
   [packaging/README.md](https://github.com/plyndra/plyEngine/blob/main/packaging/README.md).

### What I'd love to hear back

- The **version and OS** you used, and your compiler if you built it.
- Anything that broke or surprised you — including "this doc is wrong about
  X". Friction reports are worth more to me than praise.
- If you build something, even a moving square: that is the evidence I need
  for the 1.0.0 soak gate.

I'll log qualifying reports (with your permission) in
[docs/CONSUMER_REPORTS.md](https://github.com/plyndra/plyEngine/blob/main/docs/CONSUMER_REPORTS.md).

Repo, docs, changelog: <https://github.com/plyndra/plyEngine> — CI is green on
Windows/Linux/macOS plus an ASan+UBSan job, and the release gate re-runs the
shipped archives rather than trusting the build tree.

Not asking for money or stars — just real builds and real friction.

---

## r/cpp Show & Tell variant

**Title:** Show & Tell: plyEngine — C++20/OpenGL 3.3 2D engine with a
phone-as-controller companion server, API frozen at beta

**Body:** Same engine summary, cut for a C++ audience: build system story
(pinned FetchContent deps with an offline `PLY_USE_SYSTEM_DEPS` mode, a
`find_package` config package, a self-hosted vcpkg registry and a conan
recipe), 15 headless ctest suites run under ASan+UBSan, and a public header
freeze enforced by a script that diffs `engine/include` against the freeze
tag. Ask: build it, report version + OS + compiler. Link to the same release
and to `docs/CONSUMER_REPORTS.md`.

---

## Discord `#showcase` message

> plyEngine — small C++20/OpenGL 3.3 2D engine, MIT. Frozen public API at
> v1.0.0-beta.3; I need outside builds before tagging 1.0.0. Four-player
> phone-controller demo runs from the release binaries (no build), and the
> tutorial builds in ~15 min: https://github.com/plyndra/plyEngine
> Especially interested in `find_package`/FetchContent friction.

---

## Audit table (checked 2026-11-09 against repo `8566cc9` + release assets)

| Claim in the post | Verified how | Result |
|---|---|---|
| Version is 1.0.0-beta.3 | `git tag`, release assets, `PLY_VERSION_STRING` | ✅ correct |
| Release URL resolves | HTTP GET | ✅ 200 |
| macOS archive | release asset list | ⚠️ **arm64 only** — stated in the post |
| Archives run without a build | release gate: unpack + `--verify` on Ubuntu 22.04 | ✅ |
| `--web companion` needed for phones | archive ships `companion/index.html`; default `webRoot` is `companion/dist` which the archive lacks | ✅ fixed in copy (previously wrong) |
| Four players | `kPlayerCount = 4` in `examples/companion/platformer.hpp` | ✅ |
| Five-coin tutorial | `kCoinCount = 5` | ✅ |
| API frozen since beta.1 | tag `v1.0.0-beta.1` + `release-check.sh` drift gate | ✅ |
| Lua 5.4 sandboxed | `engine/src/Scripting.cpp`, M9 record | ✅ |
| 15 headless suites | `ls tests/*.cpp` = 15, 15 `add_test` | ✅ |
| CI green on 3 OSes + sanitizers | Actions run on `8566cc9` | ✅ |
| vcpkg snippet installs | end-to-end run from the public repo, baseline `a8e1a35` | ✅ |
| "scan the QR code" | no QR code anywhere in engine/demo/PWA | ❌ **removed** — room code is typed |
| "room code + mDNS discovery" for phones | mDNS advertises the host; browsers still need the URL typed | ⚠️ reworded |
| ~15 min tutorial build | plausible warm; cold SDL3 build is slower | ⚠️ qualified |
