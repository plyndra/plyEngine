# Roadmap target — 2.0

Sequence, gates and units for the top-down simulation engine. Milestones are
ordered so that each one ends with something you can **look at** and something
CI can **fail on**.

Effort figures are working estimates for one agent-assisted engineer, not
commitments.

---

## M1 — The frame (≈6–8 weeks)

**Goal:** a top-down world that stands next to `/testpng/` without embarrassment,
plus the HUD that frames it. No AI, no economy, no generation.

Ordered by [ADR 0007](adr/0007-hud-first.md) (UI first) and scoped by
[ADR 0008](adr/0008-overworld-and-cave-in-m1.md) (overworld + cave).

### M1a — UI framework on a real map (≈2 weeks)

| # | Unit | Done when |
|---|---|---|
| a1 | `UIAtlasLoader`: regions, 9-slice margins, stretch/tile modes, animation frames | loads a fixture atlas; round-trip equality |
| a2 | `NineSlice` draw: corners fixed, edges/centre stretched or tiled | corner-invariance test at 3 panel sizes |
| a3 | Margin derivation from alpha contours (fallback when the atlas declares none) | derived margins equal declared margins on the fixture |
| a4 | Anchor+offset+scale layout, integer snapping, DPI-independent | anchor math tests for 9 anchor points × 3 scales |
| a5 | `Bar` (gradient fill, animated), `Slot`, `Hotbar`, `Panel`, `Tooltip` | widget rects and fill fractions asserted |
| a6 | Pixel text: BMFont/TTF at integer sizes, no AA, baseline alignment | text bounds + a rendered-glyph hash |
| a7 | `Minimap`: render-target downscale of the world + markers | marker at world (x,y) maps to the expected minimap pixel |
| a8 | `MapLoader` + `TileSetLoader` (static, no autotile yet) and one hand-authored 30×20 map | map lints clean; static tiles render behind the HUD |
| a9 | Input router: UI consumes first, then world; phone dev client can drive both | a scripted click on a slot does not also move the player |

**Gate to leave M1a:** the HUD from `UI_FRAMEWORK.md` renders over a real
(if plain) map; 6 new test suites green; no wall-clock or GPU dependency in any
UI test.

### M1b — Tile worlds (≈2–3 weeks)

| # | Unit | Done when |
|---|---|---|
| b1 | `TileSet` metadata schema + loader (solid, variant group, mask role, animation) | schema validated; unknown mask fails loudly |
| b2 | `AutoTileResolver` with the blob-47 table, resolved at load, cached | all 47 cases resolve; unknown case errors |
| b3 | `TileLayer` (ids, flags) + `WorldMap` chunks + view-frustum culling | quad count == intersecting tiles, exact |
| b4 | `TileAnimator` with per-cell phase offset | frame index after n steps asserted; phases differ |
| b5 | `PropLayer`: multi-tile props, collision rects, pivot, shadow, occlusion flag | prop collision matches art footprint; occlusion toggles layer |
| b6 | `YSort` across props/NPCs/player with id tiebreak | fixture order array matches; stable on repeat |
| b7 | `Camera2D`: follow, deadzone, clamp, integer snap | snap test: 10.4 and 10.6 give identical hashes |
| b8 | `RenderTarget` + integer upscale + `SceneRenderer` layer list | 1 px line stays k px wide; layer order asserted |
| b9 | `LightMap` (ambient tint × light sprites) + `PostPass` vignette | light hash from (clock, lights); cave and noon fixtures differ as expected |
| b10 | `ParticleSystem` (seeded, fixed-step, visual-only) | same seed → same positions; gameplay hash unchanged with particles on |

**Gate to leave M1b:** a 40×30 village map with autotiled grass/dirt/water,
signed props, roof occlusion and a following camera, and a cave map with a lit
radius. Frame hashes committed as goldens.

### M1c — The two frames (≈1–2 weeks)

| # | Unit | Done when |
|---|---|---|
| c1 | Art generation for both environment kits (`ART_PIPELINE.md`) | lint-clean manifests with provenance |
| c2 | Hand-fix pass on rejected tiles | lint clean; visual review sign-off |
| c3 | Hero capture from the new demo (`--dump-frames` still works) | GIF ≤8 MB, labelled scripted |

**Gate to leave M1:** two screenshots (village by day, cave) that pass the
comparison review, plus the HUD; all M1 tests green; benchmark within the
performance budget in `ARCHITECTURE_TARGET.md`.

---

## M2 — Alive (≈4 weeks)

**Goal:** the town acts like it is inhabited.

| # | Unit | Done when |
|---|---|---|
| 1 | `NavGrid` from tiles+props, per-footprint | walkable mask hash for a fixture map |
| 2 | A* with deterministic tie-break | path cost equals hand-computed optimum |
| 3 | `NpcAgent` + needs + inventory + money | state serializes; needs decay per tick |
| 4 | `BehaviourTree` runtime (sequence/selector/condition/action/decorator) + blackboard | BT fixture asserts the chosen branch |
| 5 | `Schedule` per role, feeding BT goals | NPC at work POI 09:00 within tolerance, every day |
| 6 | `Perception` + memory decay | sees/heard state asserted from fixtures |
| 7 | Interaction: click-to-talk/trade, item pickup, resource harvest | interaction resolves to the right entity; UI reflects inventory |
| 8 | Walk-cycle animation (4-dir) wired to velocity | clip/frame from velocity asserted |
| 9 | `SaveGame` v1 round-trip | save→load→hash equality, including NPCs |
| 10 | Phone dev client: time control, spawn NPC, teleport, overlay toggles | documented panel; drives the same commands tests use |

**Gate:** a 60-second scripted run where 20 NPCs complete their day, the player
harvests, trades, and saves/reloads, byte-identical across two runs.

---

## M3 — Simulated (≈4 weeks)

**Goal:** time and money actually run.

| # | Unit | Done when |
|---|---|---|
| 1 | `WorldClock` + day/night tint + seasonal hooks | tint hash per hour fixture |
| 2 | `RngStreams` split by domain | adding an AI draw does not change terrain |
| 3 | `Inventory` containers + shop stock | inventory invariants (no negative, no dup) |
| 4 | `Market` double auction + clearing batches | money conserved; price finite over 30 days |
| 5 | `Production` recipes (farm, fishery, smith) | supply responds to inputs within N ticks |
| 6 | NPC buy/sell behaviour on schedule | trade volume within configured band |
| 7 | `PriceHistory` + economy debug overlay | overlay reads from the same buffer tests assert |
| 8 | Music/ambience buses + weather audio | loop/duck assertions via null backend |
| 9 | Weather system (rain, storms) + particles + tint | weather transitions deterministic per seed |

**Gate:** a 30-simulated-day soak in CI: no NaN, money conserved, prices
bounded, every NPC's schedule satisfied, save/load stable.

---

## M4 — Generated (≈4–6 weeks)

**Goal:** towns are produced, not authored.

| # | Unit | Done when |
|---|---|---|
| 1 | Terrain gen (noise + biome rules) from a seeded stream | fixed seed → identical heightmap hash |
| 2 | Water/cliff carving + coastline rules | no orphan puddles; coast tileable |
| 3 | POI placement + road graph (A*) | every POI reachable |
| 4 | Lot subdivision + building assembly from templates | no overlaps, no road collisions |
| 5 | Prop decoration rules per zone | density within bounds; no prop inside walls |
| 6 | Population + economy seeding | population/economy invariants hold |
| 7 | Validation report + 200-seed CI matrix | zero failures; injected fault fails the gate |
| 8 | Minimal map debug view (seed, POIs, roads, lots) | view renders from the same data as the sim |

**Gate:** `--generate --seed N` produces a playable town, validated, for 200
seeds in CI, and the debug view shows why a town looks the way it does.

---

## M5 — Survival (≈6+ weeks)

**Goal:** it is a game.

Needs (hunger/energy/temperature), resource nodes and respawn, crafting recipes,
tools with durability, building placement by the player, weather severity,
seasons affecting production, and a save-format migration path.

**Gate:** a 60-minute scripted playthrough from new game to a built structure,
save/reload mid-way, no invariant violation.

---

## Cross-cutting work that never gets its own milestone

- Art generation and lint for each new asset class (see `ART_PIPELINE.md`).
- Golden-frame hashes for every visual subsystem that lands.
- Benchmark upkeep against the performance budget.
- API-freeze discipline: additive only; any reshape needs a deprecation cycle.
- Documentation updates in `docs/planning/` when a decision changes — an ADR gets
  marked `superseded` rather than edited into agreement with reality.
