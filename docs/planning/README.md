# Planning — plyEngine 2.0 target

**Status:** planning record, 2026-11-09. Nothing here is implemented. These
documents define what plyEngine becomes after 1.0.1, and they exist so the
decisions are checkable rather than remembered.

## What this is

plyEngine 1.x is a **2D side-view action engine with phone controllers**: sprite
batching, archetype ECS, fixed-timestep physics, tilemap collision, Lua
scripting, and a four-player platformer as the reference demo. It is frozen,
mechanically tested, and shipping.

The target is a **top-down sandbox survival engine**: a tiled world with layered
art and y-sorted depth, simulated NPCs on schedules, procedurally generated
towns, and an in-game economy. The reference frames for the visual target are in
`/testpng/` (16 px tiles at 4× integer scale, dense decoration, autotiled
terrain, day/night light pools, HUD-heavy UI).

## Documents

| File | What it answers |
|---|---|
| [ARCHITECTURE_TARGET.md](ARCHITECTURE_TARGET.md) | What subsystems exist in 2.0, in what order they render and simulate, and what each one must prove |
| [ROADMAP_TARGET.md](ROADMAP_TARGET.md) | The milestone sequence M1–M5, unit decomposition, and the gate to leave each milestone |
| [UI_FRAMEWORK.md](UI_FRAMEWORK.md) | The screen-space UI layer: anchors, 9-slice, widgets, atlas schema, input routing, tests |
| [ART_PIPELINE.md](ART_PIPELINE.md) | The self-hosted generative art pipeline: components → deterministic assembly → hard lint gates → provenance |
| [adr/](adr/) | One file per decision taken in the 2026-11-09 planning session |

## Decision log

| # | Decision | ADR |
|---|---|---|
| 1 | The engine's target identity becomes top-down sandbox survival | [0001](adr/0001-top-down-sim-identity.md) |
| 2 | Phone companion becomes a development/test tool, not a player feature | [0002](adr/0002-phone-companion-as-dev-tool.md) |
| 3 | Native plyEngine JSON map format; no Tiled runtime dependency | [0003](adr/0003-native-json-map-format.md) |
| 4 | Assets are downloaded on first run, with a pinned manifest and an offline fallback | [0004](adr/0004-download-on-first-run-assets.md) |
| 5 | NPC decisions use behaviour trees | [0005](adr/0005-behaviour-tree-npcs.md) |
| 6 | Economy is agent-to-agent bargaining, first shipped as a batched double auction | [0006](adr/0006-double-auction-economy.md) |
| 7 | The HUD/UI framework is built first | [0007](adr/0007-hud-first.md) |
| 8 | M1 content scope is overworld plus cave | [0008](adr/0008-overworld-and-cave-in-m1.md) |
| 9 | Engine work lands on `feat/*` branches with PR review; docs and assets go to main | [0009](adr/0009-branch-per-subsystem.md) |
| 10 | Art is generated self-hosted into assets plyEngine owns | [0010](adr/0010-self-hosted-generated-art.md) |
| 11 | Every new simulation system is deterministic and headless-testable | [0011](adr/0011-determinism-for-simulation.md) |

## Relationship to existing documents

- `docs/DESIGN.md` — 1.x architecture. Still accurate for everything it
  describes; extended, not replaced, by `ARCHITECTURE_TARGET.md`.
- `docs/ROADMAP.md` — the 1.x milestone history (M0–M9, 1.0.0). Complete. The
  2.0 sequence lives in `ROADMAP_TARGET.md`; a pointer has been added there.
- `docs/PROMOTION_PLAN.md` and `evals/` — the promotion work, unchanged. Two
  items interact with this plan and are flagged in the ADRs: the "try it in 30
  seconds" block depends on asset delivery (0004), and the hero asset depends on
  the demo (0001).
- `docs/CONSUMER_REPORTS.md` — unaffected.

## Standing constraints

1. **API freeze is mechanical.** `scripts/release-check.sh` fails on removed or
   renamed lines in `engine/include` relative to `v1.0.0-beta.1`. Everything
   planned here is additive; anything that must change shape needs a deprecation
   cycle and an `--allow-api-change` run recorded in `DESIGN.md`.
2. **Headless first.** No subsystem is accepted unless it can be stepped,
   asserted and hashed without a window and without a GPU.
3. **Receipts over claims.** Measurements, seeds, hashes and licences are stored
   with the artefact they describe (see `ART_PIPELINE.md` provenance and the
   `evals/` culture).
4. **No generated launch text.** Unrelated to art generation (see the promotion
   plan), but it is the reason `ART_PIPELINE.md` separates "generated assets"
   from "generated prose".
