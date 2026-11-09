# Promotions plan

Derived from the research report at
`/root/.pi/agent/AUDITS/plyengine-promotion-research-2026-11-09.md`. Every
element below traces to a finding there; anything that does not is marked
**hypothesis**.

Baseline at time of writing: `v1.0.0` published, 7 releases, 0 stars,
1 contributor, repo created 2026-11-08, no Reddit (owner decision).

## Principles

1. **The repository is the landing page for every channel.** Every post, list
   entry and package manager sends a stranger here for ~20 seconds. Fix
   conversion before buying attention.
2. **Write incident posts, not launch posts.** The channels with real reach
   reward technical takeaways and punish announcement-shaped content.
3. **One shot per door.** Show HN, curated lists and registry PRs are
   single-attempt surfaces with written gates. Attempt each once, when ready.
4. **Distribution is promotion.** Every package manager that carries the engine
   keeps advertising it while nobody is posting.

## Phase 0 — Make the repo convert (target: 2026-11-09 → 11-10, ~2h)

The audit found the repo invisible inside GitHub: no description, no topics, no
homepage, and no image anywhere despite "phone as controller" being the pitch.

| # | Task | Acceptance check |
|---|---|---|
| 0.1 | About description: problem-first, no stack list | `gh api repos/plyndra/plyEngine --jq .description` non-empty, ≤350 chars, names the audience |
| 0.2 | Topics set (≤20, lowercase/hyphens) | `gh api … --jq .topics` contains `game-engine`, `cpp20`, `opengl`, `ecs`, `sdl3`, `lua`, `gamedev`, `multiplayer` |
| 0.3 | Homepage → release page | `--jq .homepage` = release URL |
| 0.4 | Discussions enabled | `--jq .has_discussions` = true |
| 0.5 | **20-second GIF of four phones driving the platformer**, above the fold in README | README renders an image in the first screen; file tracked in repo or hosted |
| 0.6 | "Try it in 30 seconds" block: download → unpack → run, no flags | README section exists; commands match `ply_companion` behaviour verified in this repo |
| 0.7 | Social preview image set (repo settings → not API-visible) | manual, owner |

Gate to leave Phase 0: 0.1–0.6 done. Posting before 0.5 exists wastes the only
asset that explains the product in one look.

## Phase 1 — Earn the artifact, place it (2026-11-11 → 11-24)

| # | Task | Notes / acceptance |
|---|---|---|
| 1.1 | Publish the "four things that broke" write-up | Already drafted in `docs/announcements/2026-11-09-discord-announcement.md`. Publish somewhere it can be linked (repo `docs/` page is acceptable; a blog or dev.to mirror is better for reach). Link repo ← from the post, never the reverse |
| 1.2 | Discord `#showcase` in 2–3 C++/gamedev servers | One message per server, different days, answer every reply. No cross-posting same day |
| 1.3 | Pitch one newsletter or podcast with the **incident** story | **Hypothesis** — no evidence found on cold-pitch acceptance rates; expect silence, do not repeat |
| 1.4 | Reply to people who already have the problem | GitHub issue search for "2D engine C++" requests; answer with the problem framing, not a link dump |

Gate to leave Phase 1: at least one non-owner build report, or two channels
tried with zero response (that is data too — record it in
`docs/CONSUMER_REPORTS.md` as an outreach note, not as a build).

## Phase 2 — Show HN, once (target: week of 2026-12-01)

Written rules, quoted from the guidelines and each mapped to a pre-flight check:

| HN requirement | Pre-flight check |
|---|---|
| "things people can run on their computers" | Release archive runs on a clean machine: verified by `smoke-linux` gate |
| "easy for users to try … ideally without barriers such as signups" | No account, no email, no form anywhere in the path |
| "something you've worked on personally and which you're around to discuss" | Owner free for 4–6 hours that day |
| "new features and upgrades generally aren't substantive enough" | 1.0 is the submission; do not re-submit 1.0.x |
| "don't ask friends to upvote or comment" | No coordination with anyone, ever |
| Title must begin `Show HN` | `Show HN: plyEngine – a 2D C++ engine where phones are the controllers` |

Post format: link to the release/repo, then a **hand-written** first comment —
what it is, why the phone-controller idea, what is unfinished, what feedback is
wanted. Do not use generated text: 2026 guidance reports open hostility to
LLM-sounding launch prose on HN.

Failure handling: if it does not get traction, do nothing. No repost, no
"reminder" comment, no vote requests. The write-up (1.1) remains the durable
asset.

## Phase 3 — Compound channels (from ~2027-04, i.e. month 5)

Gated by the lists' own published thresholds — check each before opening a PR:

| Channel | Gate (as published 2026-11) | Status today |
|---|---|---|
| `awesome-cpp` | blocks repos/accounts created after 2024 | **closed** — do not attempt |
| `awesome-go` | ≥5 months history + SemVer release | eligible ~2027-04 |
| `awesome-ios` | ≥100 stars + >1 contributor | not yet |
| Homebrew tap → homebrew-core | maintainer review; tap has no gate | start with a tap |
| Conan Center | PR to `conan-center-index`; recipe already verified locally | after 1.0.1 exists |
| vcpkg upstream | PR to `microsoft/vcpkg`; in-repo registry already works | optional — registry already gives `vcpkg install plyengine` |
| Conference talk | regional C++/gamedev meetup CFP | packaging-incident material is the talk |

## Measurement

Track these, in this order of usefulness:

1. **Release asset download counts** — did anyone actually take the binaries.
2. **Distinct non-owner build reports** — the only signal that the engine is
   usable; goes in `docs/CONSUMER_REPORTS.md`.
3. **Issue quality** — a stranger's bug report citing the docs beats 50 stars.
4. **Stars** — reach only; a spike with no issue traffic means the post was read,
   not used.

Review cadence: after each phase, one paragraph in this file recording what was
tried, what happened, and what changes next time. Unmeasured promotion is
indistinguishable from none.

## Do not

- **No Reddit** (owner decision 2026-11-09; also its API registration now needs a
  bot account plus human review).
- No posting before the repo has a description, topics and a GIF (Phase 0 gate).
- No same-day cross-posting; no vote solicitation; no asking friends to comment.
- No re-submitting a maintenance release to Show HN.
- No generated launch text.
- Do not attempt `awesome-cpp`, or any list whose thresholds you do not meet —
  a rejected PR is a spent introduction.

## What the agent can do vs what the owner must do

| Can be executed here | Owner only |
|---|---|
| Repo metadata (0.1–0.4), README sections (0.5–0.6 asset capture) | Social preview image (0.7) |
| GIF capture from the headless four-player run | Publishing posts anywhere |
| Drafting/editing every text artifact | Answering replies and threads |
| Writing the record in this file after each phase | Deciding when the HN shot is taken |
