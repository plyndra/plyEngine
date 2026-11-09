# Call for testers — publication copy (v1.0.0, 2026-11-09)

Tracked here so the published text is reviewable history. Reports get logged
in [../CONSUMER_REPORTS.md](../CONSUMER_REPORTS.md).

Every factual claim below was checked against the repo and the published
`v1.0.0` release on 2026-11-09. Nothing here is
aspirational.

**Status 2026-11-09: 1.0.0 is released.** This copy was written during the
beta and updated after the promotion; the ask (build it, report friction) is
unchanged, but §What I'd love to hear back no longer frames reports as a gate.

**Access note (measured 2026-11-09):** this network gets a "Prove your
humanity" challenge on Reddit HTML and 403 on `*.json` paths, so the live rule
list and flair picker must be read in a browser while signed in; `/r/<sub>/.rss`
**is** readable and is how rule 3's current text was obtained. Rule 3 (updated
2026-11-09) permits sharing your own free tool only with technical takeaways
plus engagement in comments — hence the rewrite below.

**Venue order** (council recommendation, owner-approved): EngineDev / C++
Discord `#showcase` first as a soft launch → r/gamedev → r/cpp Show & Tell →
Show HN only after ~2 weeks with no qualifying report. Never cross-post the
same day. Keep the version pinned at beta.3 across venues so reports stay
comparable. r/gamedev requires a post flair (use *Feedback Request*) and its
self-promotion rule needs context, not a bare link — this copy is written that
way, but confirm the current sidebar rules before posting.

---

## Title (r/gamedev)

Four things that broke in my 2D engine only after people could download it

(75 characters; Reddit's limit is 300. Technical-title framing because rule 3
treats a project announcement as a showcase unless it carries takeaways.)

---

## Body (r/gamedev) — rewritten for rule 3

> **Why this shape.** r/gamedev's rule 3 ("No Showcasing Projects") permits
> sharing your own *free* tool only with "discussion or technical takeaways" —
> what challenges you hit, how you solved them, formulas/features/workarounds,
> examples of use — plus engaging with comments about it. A "please test my
> engine" post is a showcase and gets removed. The copy below leads with the
> engineering, and the ask is secondary. Flair: pick the closest of *Discussion*
> / *Feedback Request* in the composer; the current flair list could not be read
> from this network (see the access note at the end).

**Title:** Four things that broke in my 2D engine only after people could
download it

**Body:**

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

**The actual ask, kept small:** the free public API has been frozen since the
beta, and the part I cannot test myself is ergonomics of the consumption paths.
If you build something with it — `find_package` after `cmake --install`,
FetchContent, or the repo-as-vcpkg-registry above — I would like to know what
broke. Demos, docs and the archives:
<https://github.com/plyndra/plyEngine/releases/tag/v1.0.0>. Happy to answer
anything about the four failures above; that is the discussion I am here for.

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

## Audit table (checked 2026-11-09 against repo + the published v1.0.0 release)

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
| r/gamedev rule 3 compliance | rule 3 text obtained via `/r/gamedev/.rss` (post 1x6s41i, updated 2026-11-09) | ✅ body restructured around technical takeaways |
| Live rules page / flair list | 403 + "Prove your humanity" from this network; RSS readable | ⚠️ unverified — read in a signed-in browser at posting time |
