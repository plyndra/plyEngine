# Call for testers — publication copy (v1.0.0, 2026-11-09)

Tracked here so the published text is reviewable history. Reports get logged
in [../CONSUMER_REPORTS.md](../CONSUMER_REPORTS.md).

Every factual claim below was checked against the repo and the published
`v1.0.0` release on 2026-11-09. Nothing here is
aspirational.

**Status 2026-11-09: 1.0.0 is released.** This copy was written during the
beta and updated after the promotion; the ask (build it, report friction) is
unchanged, but §What I'd love to hear back no longer frames reports as a gate.

**Venue order** (council recommendation, owner-approved): EngineDev / C++
Discord `#showcase` first as a soft launch → r/gamedev → r/cpp Show & Tell →
Show HN only after ~2 weeks with no qualifying report. Never cross-post the
same day. Keep the version pinned at beta.3 across venues so reports stay
comparable. r/gamedev requires a post flair (use *Feedback Request*) and its
self-promotion rule needs context, not a bare link — this copy is written that
way, but confirm the current sidebar rules before posting.

---

## Title (r/gamedev)

Looking for a few testers: plyEngine 1.0.0 — a C++20 2D engine where phones are the controllers

(102 characters; Reddit's limit is 300.)

---

## Body (r/gamedev)

Up to four people play a four-player platformer together on **their own
phones**, no app install: the host runs the game, prints a LAN URL and a
4-character room code, and everyone joins from the browser. Coins are
contested and the first to the flag wins.

Under the hood it is a small MIT-licensed 2D engine: C++20, SDL3 + OpenGL
3.3, an archetype ECS, swept-AABB tilemap physics, PNG/TTF assets, miniaudio
audio, and a sandboxed Lua 5.4 scripting layer.

Why I'm posting: 1.0.0 shipped with the public API frozen since `v1.0.0-beta.1`,
and the honest gap is ergonomics — whether these consumption paths are pleasant
with someone else's project layout. Outside reports no longer gate a release,
so the ask is simpler: build it, and tell me what broke.

### The 10-second version (no build, no toolchain)

Download an archive, unpack, run:

```sh
# Linux x86-64 / Windows x86-64 / macOS Apple Silicon (arm64 only)
./ply_companion                     # prints a LAN URL + 4-char room code
```

It picks the web root automatically (a source checkout builds the PWA into
`companion/dist`, the release archive ships it in `companion/`), prints a room
code and a URL like `http://192.168.1.20:49152`; have two or
three people on the same Wi-Fi open that URL, type the room code, and play.
`./ply_tutorial` is the completed five-coin game from the tutorial, for a
single keyboard player.

Release: <https://github.com/plyndra/plyEngine/releases/tag/v1.0.0>
(each archive is unpacked on a clean Ubuntu 22.04 container and its demos run
headless there before publishing, so a broken binary fails CI rather than
you).

### The version I actually need (a build)

Any one of these is enough — pick the least annoying:

1. **Source build of the tutorial**:
   <https://github.com/plyndra/plyEngine/blob/main/docs/TUTORIAL.md>
   Measured at 1m46s configure+build on 12 cores (a cold download plus an
   older laptop can take several times that). Needs CMake ≥ 3.24, a C++20 compiler, and the SDL3/X11 dev packages; the
   exact apt list is in the tutorial's "Before you start" block.
2. **FetchContent** into your own CMake project (`GIT_TAG v1.0.0`) —
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
- If you build something, even a moving square: I'd like to log it (with your
  permission) as a real-world data point.

I'll log qualifying reports (with your permission) in
[docs/CONSUMER_REPORTS.md](https://github.com/plyndra/plyEngine/blob/main/docs/CONSUMER_REPORTS.md).

Repo, docs, changelog: <https://github.com/plyndra/plyEngine> — CI is green on
Windows/Linux/macOS plus an ASan+UBSan job, and the release gate re-runs the
shipped archives rather than trusting the build tree.

Not asking for money or stars — just real builds and real friction.

---

## r/cpp Show & Tell variant

**Title:** Show & Tell: plyEngine 1.0.0 — C++20/OpenGL 3.3 2D engine with a
phone-as-controller companion server

**Body:** Same engine summary, cut for a C++ audience: build system story
(pinned FetchContent deps with an offline `PLY_USE_SYSTEM_DEPS` mode, a
`find_package` config package, a self-hosted vcpkg registry and a conan
recipe), 15 headless ctest suites run under ASan+UBSan, and a public header
freeze enforced by a script that diffs `engine/include` against the freeze
tag. Ask: build it, report version + OS + compiler. Link to the same release
and to `docs/CONSUMER_REPORTS.md`.

---

## Discord `#showcase` message

> plyEngine 1.0.0 — small C++20/OpenGL 3.3 2D engine, MIT, public API frozen.
> Four-player phone-controller demo runs straight from the release binaries
> (no build, no flags), and the tutorial builds in ~2 minutes:
> https://github.com/plyndra/plyEngine
> Now looking for outside friction reports on the `find_package` / FetchContent
> / vcpkg-registry paths — 1.0.0 shipped without them, and that is the part I
> am least sure about.

---

## Audit table (checked 2026-11-09 against repo `8566cc9` + release assets)

| Claim in the post | Verified how | Result |
|---|---|---|
| Version is 1.0.0 | `git tag v1.0.0`, release assets, generated `Version.hpp` | ✅ correct |
| Release URL resolves | HTTP GET | ✅ 200 |
| macOS archive | release asset list | ⚠️ **arm64 only** — stated in the post |
| Archives run without a build | release gate: unpack + `--verify` on Ubuntu 22.04 | ✅ |
| Phone controller loads from the archive | shipped binary, archive layout: `GET /` 200, `GET /app.js` 200 (was 404 before beta.4) | ✅ fixed in engine; no `--web` needed |
| Four players | `kPlayerCount = 4` in `examples/companion/platformer.hpp` | ✅ |
| Five-coin tutorial | `kCoinCount = 5` | ✅ |
| API frozen since beta.1 | tag `v1.0.0-beta.1` + `release-check.sh` drift gate | ✅ |
| Lua 5.4 sandboxed | `engine/src/Scripting.cpp`, M9 record | ✅ |
| 15 headless suites | `ls tests/*.cpp` = 15, 15 `add_test` | ✅ |
| CI green on 3 OSes + sanitizers | Actions run on `8566cc9` | ✅ |
| vcpkg snippet installs | end-to-end run from the public repo (baseline quoted in the snippet) | ✅ |
| "scan the QR code" | no QR code anywhere in engine/demo/PWA | ❌ **removed** — room code is typed |
| "room code + mDNS discovery" for phones | mDNS advertises the host; browsers still need the URL typed | ⚠️ reworded |
| Tutorial build time | measured: 1m46s configure+build, 12 cores, incl. SDL3 | ✅ measured, not guessed |
