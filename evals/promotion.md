# Eval definition — promotion plan

Eval-driven definition for `docs/PROMOTION_PLAN.md`, written **before** Phase 0
executes (eval-harness skill: define, then implement, then evaluate). Baseline
numbers live in [baseline.json](baseline.json); run history appends to
`promotion.log`.

Rule that outranks the rest: **a gate is never relaxed to make a run green.**
If a criterion was wrong, amend this file and the plan in the same commit, with
the reason.

## Decisions this definition assumes

Settled 2026-11-09 (grilling round 1, owner confirmed):

1. Hero asset is captured from the deterministic four-slot `--verify` run and
   labelled as such — not from four physical phones.
2. Hero is a GIF ≤8 MB / 960 px / 15–20 s, plus a full-res MP4 kept in
   `docs/announcements/`.
3. README above-the-fold order: hero → one-line problem statement → C++ snippet
   → download.
4. Canonical home for the incident write-up is a GitHub Discussions post
   (depends on `has_discussions`), with `docs/announcements/` as source of
   truth and a dev.to mirror only after one channel clears.
5. A pre-Phase-0 baseline is recorded before any promotion work ships.
6. The Show HN attempt is **readiness-triggered**; 2026-12-01 is a floor, not a
   trigger.
7. `awesome-go` and `awesome-ios` are removed from Phase 3: the repository has
   no Go code and native iOS is an explicit non-goal (`docs/DESIGN.md`).
8. Outreach replies are capped (30 min/day, ≤5/week, existing threads only) and
   are logged as outreach, never as build reports. Homebrew tap and Conan Center
   are deferred until after the HN attempt.

## Capability evals

| ID | Grader | Exact check |
|---|---|---|
| C0.1 | code | `test -n "$(gh api repos/plyndra/plyEngine --jq '.description // empty')"` and length ≤350 and mentions the audience (`grep -qi 'C++\|game develop'`) and contains no stack list (`grep -qvi 'C++20.*OpenGL\|SDL3.*Lua'`) |
| C0.2 | code | `gh api repos/plyndra/plyEngine --jq '.topics\|join(" ")'` contains `game-engine cpp20 opengl ecs sdl3 lua gamedev multiplayer`, all lowercase/hyphenated, count ≤20 |
| C0.3 | code | `gh api … --jq .homepage` is a `…/releases` URL and `curl -sIL` returns 200 |
| C0.4 | code | `gh api … --jq .has_discussions` = `true` |
| C0.5 | code + rule | `git ls-files 'docs/**/*.gif'` non-empty; file ≤8 MB; README references it on a line < 40; provenance recorded in `docs/announcements/` (which run, which labels) |
| C0.6 | code | the "try it in 30 seconds" block, copied verbatim into a clean `ubuntu:22.04` container, downloads the published archive, unpacks it, and `./ply_companion` prints a room code — the `smoke-linux` pattern, run from the README text rather than the workflow |
| C1.1 | code + human | write-up URL returns 200; contains the four incident headings; owner confirms the text is hand-written (no model output) |
| C1.2 | human | one receipt per server under `docs/announcements/`: server name, date, message snapshot; no two servers on the same day |
| C1.4 | human | ≥3 replies, each in an existing thread, each logged as an outreach note in `docs/CONSUMER_REPORTS.md`; weekly count ≤5 |
| C2 | rule | all six Phase 2 pre-flight checks recorded as PASS in the plan on the day of posting, plus owner-availability confirmation |
| C3 | code | `conan create packaging/conan --version=…` passes locally; tap formula installs in a container with `brew install --build-from-source` |

Text artifacts (README block, Discussions post, Discord message) are graded by
**rule grader** for the forbidden-token list below and by **human grader** for
voice. No model grader decides a factual claim (host rule, and the plan's own
"no generated launch text").

## Regression evals

| ID | Grader | Condition | Tier |
|---|---|---|---|
| R1 | code | `scripts/release-check.sh` PASS | release-critical |
| R2 | code | `ctest --test-dir build` 15/15 | release-critical |
| R3 | code | every relative markdown link resolves (inside R1) | release-critical |
| R4 | rule | one-shot doors: no PR to `awesome-cpp`; no Show HN submission before C2; no repost of `1.0.x` | **absolute (k=∞)** |
| R5 | code | description / topics / homepage / hero never regress to empty, null or absent | absolute |
| R6 | rule | published text contains none of: `excited to announce`, `game-changer`, `revolutionary`, `🚀`, `thrilled` | absolute |
| R7 | rule | first README screen still = image + one-liner + snippet | soft |

## Metrics and thresholds

- Capability evals: `pass@3 ≥ 0.90` (three attempts allowed on reversible
  items).
- Regression evals R1–R3: `pass^3 = 1.00`.
- R4–R6: single-attempt surfaces. One failure is the failure; there is no k to
  average over.
- Cost metric, recorded per phase: **hours spent per non-owner build report**.
  This is the only number that says whether promotion effort is buying anything.

## Anti-patterns for this run

- Letting stars become a pass condition. The plan ranks stars last; keep it last.
- Overfitting the README to one commenter's taste between HN attempt and nothing.
- Measuring only the happy path (a GIF that renders in a browser but not in the
  GitHub mobile reader).
- Recording a channel attempt without a receipt, which makes a zero
  indistinguishable from "never tried".
- Adding a gate after the fact to explain a missed target.

## Storage

- `evals/promotion.md` — this definition (versioned with code).
- `evals/promotion.log` — append `date eval-id PASS|FAIL evidence` per run.
- `evals/baseline.json` → later `evals/baseline-<date>.json` snapshots.
- `evals/report-<date>.md` — per-phase report.
- Phase-writeback paragraphs stay in `docs/PROMOTION_PLAN.md` as the plan asks.
