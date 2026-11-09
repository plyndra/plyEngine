# Announcements — Discord copy (v1.0.0, 2026-11-09)

Tracked so the published text is reviewable history. Reports get logged in
[../CONSUMER_REPORTS.md](../CONSUMER_REPORTS.md).

**Owner decision 2026-11-09: no Reddit.** Reddit was dropped from the outreach
plan by the owner after the platform's API registration changed to require a
dedicated bot account plus human-approved access, and because the posting rules
there treat this kind of project announcement as a showcase. Don't re-propose
it. Discord is the channel; the repo and the release page are the links.

Every claim below was checked against the repo and the published `v1.0.0`
release (tag commit `bd2a1e6`) on 2026-11-09. Nothing here is aspirational.

## Discord `#showcase` message

> plyEngine 1.0.0 — small C++20/OpenGL 3.3 2D engine, MIT, public API frozen.
> Four-player phone-controller demo runs straight from the release binaries
> (no build, no flags), and the tutorial builds in ~2 minutes:
> https://github.com/plyndra/plyEngine
> Now looking for outside friction reports on the `find_package` / FetchContent
> / vcpkg-registry paths — 1.0.0 shipped without them, and that is the part I
> am least sure about. Happy to detail the four packaging failures that only
> showed up after download.

## Longer "what went wrong" variant (technical channels, HN-style posts)

**Four things that broke in my 2D engine only after people could download it**

I maintain plyEngine, a free MIT-licensed 2D engine (C++20, SDL3 + OpenGL 3.3,
archetype ECS, sandboxed Lua 5.4, phone-as-controller multiplayer). It reached
1.0 yesterday. Rather than pitch it, here are the four problems that only
showed up once the thing was actually downloaded and run by someone other than
me — they cost more time than any feature:

**1. Green CI shipped a binary nobody could start.** One release built on a
new CI image produced artifacts needing glibc 2.43. Every test passed, every
demo ran — in the *build tree*. Fix: the release job now unpacks the archive on
a toolchain-free Ubuntu 22.04 container, runs it there, and asserts the glibc
floor by reading `objdump -T` symbols instead of trusting the base image.

**2. The list of shipped binaries and the gate disagreed.** A later release
silently omitted a demo binary: packaging enumerated files by hand, the smoke
check only executed two of six. Presence and execution are different claims —
both are now asserted against the unpacked artifact, not the build directory.

**3. The flagship demo 404'd for every player.** The phone controller is served
by the engine's HTTP server from a directory of static files. The default path
pointed at Vite's build output (`companion/dist`), but the *release archive*
ships the built PWA at `companion/`. Headless `--verify` never fetches a URL,
so everything was green while every player's phone got
`404 not found: index.html`. Fix: resolve the web root by probing for
`index.html`, and make CI start the shipped binary and require 200s from `/`
and `/app.js`.

**4. Registering a package without owning a registry.** vcpkg only installs
ports it knows about; my repo isn't in the registry and I didn't want to
maintain an upstream PR before 1.0. It turns out a *repository can be its own
git registry*: `ports/<name>/` plus `versions/<first-letter>-/<name>.json` and
`versions/baseline.json`. Two traps that cost an evening: a flat
`versions/<name>.json` is silently ignored (the letter-prefixed directory is
mandatory), and `baseline.json` holds **version strings**, not the port's tree
hash — putting a tree hash there yields
`no version database entry for <port> at <sha>`. Let `vcpkg x-add-version`
generate the database; don't hand-roll it.

Bonus, because it is the part I am least sure others handle the same way:
testing a game engine with no GPU in CI. Every demo has a `--verify` mode that
plays a scripted session and asserts outcomes — coins collected, deaths
attributed to pit vs saw, four players moving independently, audio cues
requested *and* played — on top of the usual ctest suite under ASan+UBSan.

**The ask, kept small:** the free public API has been frozen since the beta,
and the part I cannot test myself is ergonomics of the consumption paths. If
you build something with it — `find_package` after `cmake --install`,
FetchContent, or the repo-as-vcpkg-registry above — I would like to know what
broke. Demos, docs and the archives:
<https://github.com/plyndra/plyEngine/releases/tag/v1.0.0>.

## Audit table (checked 2026-11-09 against repo + the published v1.0.0 release)

| Claim | Verified how | Result |
|---|---|---|
| Version is 1.0.0 | `git tag v1.0.0`, release assets, generated `Version.hpp` | ✅ |
| Release URL resolves | HTTP GET | ✅ 200 |
| macOS archive | release asset list | ⚠️ arm64 only — stated |
| Archives run without a build | release gate: unpack + `--verify` on Ubuntu 22.04 | ✅ |
| Phone controller loads without flags | shipped binary, archive layout: `GET /` 200, `/app.js` 200 | ✅ |
| Four players | `kPlayerCount = 4` in `examples/companion/platformer.hpp` | ✅ |
| Five-coin tutorial | `kCoinCount = 5` | ✅ |
| API frozen since beta.1 | tag `v1.0.0-beta.1` + `release-check.sh` drift gate | ✅ |
| Lua 5.4 sandboxed | `engine/src/Scripting.cpp`, M9 record | ✅ |
| 15 headless suites | `ls tests/*.cpp` = 15, 15 `add_test` | ✅ |
| CI green on 3 OSes + sanitizers | Actions run on `8566cc9`/`11bcda0` | ✅ |
| vcpkg snippet installs | end-to-end run from the public repo, baseline in `packaging/README.md` | ✅ |
| Tutorial build time | measured: 1m46s configure+build, 12 cores, incl. SDL3 | ✅ measured |
