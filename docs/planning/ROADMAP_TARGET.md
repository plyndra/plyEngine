# Roadmap target — 2.0

Sequence, gates and units for the top-down simulation engine. Milestones are
ordered so that each one ends with something you can **look at** and something
CI can **fail on**.

Effort figures are working estimates for one agent-assisted engineer, not
commitments.

## Resolved parameters (round 2, 2026-11-09)

| Parameter | Value | Where it came from |
|---|---|---|
| Internal render resolution | **640×360**, integer 3× at 1080p / 4× at 1440p | [ADR 0012](adr/0012-internal-resolution-and-tile-size.md) |
| Tile size | **16 px** world tiles, **32 px** for buildings and large props | [ADR 0012](adr/0012-internal-resolution-and-tile-size.md) |
| Visible tiles per screen | 40 × 22.5 → ~900 ground cells, leaving budget headroom | arithmetic from the above |
| M1 world size | village **96×72**, cave **64×48** cells | this doc, M1b |
| Quad budget | ≤ 4 000 per internal frame | `ARCHITECTURE_TARGET.md` §11 |
| Render target | `RenderTarget` + integer upscale lands in **M1a**, not M1b | minimap dependency |
| Dev client | scripted synthetic input in M1a; phone dev client in M2 | this doc, M1a |
| Art time-box | **three sessions** on the terrain proof, then re-decide | `ART_PIPELINE.md` §9 |
| Art gate scope | generated assets + fixture sheets only, widened later | `ART_PIPELINE.md` §5 |
| ComfyUI | installed at **M1c start**, pinned commit, own venv, systemd unit | `ART_PIPELINE.md` §2 |
| Cave art | own tileset family, shared palette with the overworld | `ART_PIPELINE.md` §7 |

---

## M1 — The frame (≈7–9 weeks)

**Goal:** a top-down world that stands next to `/testpng/` without embarrassment,
plus the HUD that frames it. No AI, no economy, no generation beyond M1c's assets.

### M1a — UI framework and render target on a real map (≈2.5 weeks)

| # | Unit | Done when |
|---|---|---|
| a1 | `UIAtlasLoader`: regions, 9-slice margins, stretch/tile modes, animation frames | loads the fixture atlas; load→emit→load identical |
| a2 | `NineSlice` draw: corners fixed, edges/centre stretched or tiled | corner-invariance hash at 3 panel sizes |
| a3 | Margin derivation from alpha contours (fallback when the atlas declares none) | derived == declared on the fixture |
| a4 | Anchor+offset+scale layout, integer snapping | 9 anchors × 3 logical sizes × 3 scales → expected rects |
| a5.1 | `Bar`: frame + gradient fill + ticks, value 0..1 | fill fraction → pixel width; clamps at 0/1 |
| a5.2 | `Slot`: frame + icon region + stack count + selection state | state → region mapping asserted |
| a5.3 | `Hotbar`: row of slots, selection index, wrap-around | click hits the right slot after layout rounding |
| a5.4 | `Panel` + `Tooltip`: nine-slice background, title bar, cursor-follow clamped to the viewport | tooltip never leaves bounds for any cursor position |
| a6 | Pixel text at integer sizes (BMFont/TTF), no AA, baseline-aligned | measured bounds == drawn quad extents; glyph hash |
| a7 | `RenderTarget` + integer upscale + `SceneRenderer` layer list | 1 px line stays k px wide; layer order asserted; camera snap: x=10.4 vs 10.6 → identical hash |
| a8 | `Minimap`: render-target downscale of the world + markers | world (x,y) → minimap pixel for 4 corner cases |
| a9 | `MapLoader` + the static tileset slice (regions + solid flags; full metadata is b1) and one hand-authored map | map lints clean; static tiles render behind the HUD |
| a10 | Scripted synthetic input source + router tests | UI consumes before world; release kept after capture leaves the widget |
| a11 | Input router wiring: keyboard/mouse now, phone client later | same router path for both; no second code path |

**Gate to leave M1a:** the HUD from `UI_FRAMEWORK.md` over a real map, with the
world rendering through an integer-scaled render target; the router test green;
7 new suites; no wall-clock or GPU dependency in any UI test.

### M1b — Tile worlds (≈2.5–3 weeks)

| # | Unit | Done when |
|---|---|---|
| b1.1 | `TileSet` metadata schema + a fixture tileset written by hand | schema validated; fixture round-trips |
| b1.2 | `TileSetLoader` + validation (unknown mask, out-of-range id, missing region) | each invalid case fails with a named error |
| b1.3 | Mask-table wiring between loader and resolver | table identity asserted against the fixture |
| b2 | `AutoTileResolver` with the blob-47 table, resolved at load, cached | all 47 cases resolve; unknown case errors |
| b3a | `TileLayer` (ids + flags) | layer flags survive round-trip |
| b3b | `WorldMap` chunks (16×16 cells) + view-frustum culling | submitted quads == intersecting tiles, exact count |
| b3c | Cross-chunk neighbour queries for autotiling | chunk-boundary fixture produces no seam |
| b4 | `TileAnimator` with per-cell phase offset | frame index after n steps; phases differ as configured |
| b5.1 | `PropLayer` data + loader (multi-tile, pivot, layer flag) | fixture props load with correct rects |
| b5.2 | Prop collision rects (partial tiles) | collision matches the art footprint |
| b5.3 | Prop shadow generation + pivot at feet | shadow anchors under the sprite, not at its centre |
| b5.4 | Occlusion layer toggle (roof/canopy over the player) | ordering flips exactly when the player is behind |
| b6 | `YSort` across props/NPCs/items/player, id tiebreak | fixture order array matches; stable on repeat |
| b7 | `Camera2D`: follow, deadzone, clamp to map, integer snap | snap test; clamp test at all 4 map edges |
| b8a | `LightMap`: ambient tint × light sprites | light hash from (clock, lights); noon vs cave fixtures differ |
| b8b | `PostPass`: vignette (+ optional palette LUT) | off/on hashes differ; off is the default in tests |
| b9 | `ParticleSystem` (seeded, fixed-step, visual-only) | same seed → same positions; gameplay hash unchanged with particles on |
| b10 | Headless benchmark harness | emits quads/frame, draw calls, step p95 against the budget |

**Gate to leave M1b:** 96×72 village and 64×48 cave maps, autotiled grass/dirt/
water/cliff, signed props with occlusion, following camera, lights in the cave;
golden frame hashes committed; benchmark inside the budget.

### M1c — The two frames (≈2 weeks, time-boxed)

| # | Unit | Done when |
|---|---|---|
| c1.1 | Terrain components: grass + dirt (centre, edge, outer/inner corner) | **time-boxed: three sessions**, lint clean, 47 cases assembled |
| c1.2 | Water + cliff components | shared palette with c1.1; animated water frames for the tileset |
| c1.3 | Props: trees, bushes, rocks, fences, signs | lint clean; alpha silhouettes; pivots and shadows |
| c1.4 | Buildings: base + roof split, doors, chimneys | occlusion works; collision fits footprint |
| c1.5 | Cave family (own tiles, shared palette) | lit-radius look holds in the dark fixture |
| c1.6 | UI kit: panel, bar, slot, tooltip, minimap frame, icons | nine-slice margins derive and verify |
| c2 | Hand-fix pass on rejected tiles | lint clean; 1× readability review; fixes recorded in provenance |
| c3 | Hero capture from the new demo (`--dump-frames` path still works) | GIF ≤8 MB, labelled scripted |

**Gate to leave M1:** two frames (village by day, cave) plus the HUD passing the
rubric review (see `evals` note in the planning report: `/testpng/` is a style
reference, not a pixel golden, so acceptance is a named rubric with a human
grader plus structural asserts); all M1 suites green; benchmark within budget.

**If c1.1 misses its time-box:** switch terrain *components* to a hosted
generator (Retro Diffusion, ~$0.03/image) and keep local generation for props and
UI — per `ART_PIPELINE.md` §9.

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
| 7 | Interaction: click-to-talk/trade, pickup, harvest | resolves to the right entity; HUD reflects inventory |
| 8 | Walk-cycle animation (4-dir) driven by velocity | clip/frame from velocity asserted |
| 9 | `SaveGame` v1 round-trip | save→load→hash equality, including NPCs |
| 10 | **Phone dev client v1**: clock control, spawn NPC, teleport, overlay toggles | drives the same command surface tests use; documented |

**Gate:** a 60-second scripted run where 20 NPCs complete their day, the player
harvests, trades, saves and reloads, byte-identical across two runs.

---

## M3 — Simulated (≈4 weeks)

**Goal:** time and money actually run.

| # | Unit | Done when |
|---|---|---|
| 1 | `WorldClock` + day/night tint + seasonal hooks | tint hash per hour fixture |
| 2 | `RngStreams` split by domain | adding an AI draw does not change terrain |
| 3 | `Inventory` containers + shop stock | no negative quantities, no duplication |
| 4 | `Market` double auction + clearing batches | money conserved; price finite over 30 days |
| 5 | `Production` recipes (farm, fishery, smith) | supply responds to inputs within N ticks |
| 6 | NPC buy/sell behaviour on schedule | trade volume within configured band |
| 7 | `PriceHistory` + economy debug overlay (dev client v2) | overlay reads the same buffer tests assert |
| 8 | Music/ambience buses + weather audio | loop/duck assertions via the null backend |
| 9 | Weather system (rain, storms) + particles + tint | transitions deterministic per seed |

**Gate:** a 30-simulated-day soak in CI — no NaN, money conserved, prices
bounded, every NPC schedule satisfied, save/load stable.

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
| 8 | Minimal map debug view (seed, POIs, roads, lots) | renders from the same data as the sim |

**Gate:** `--generate --seed N` produces a validated, playable town for 200 seeds
in CI, and the debug view explains why a town looks the way it does.

---

## M5 — Survival (≈6+ weeks)

Needs (hunger/energy/temperature), resource nodes and respawn, crafting recipes,
tools with durability, player building placement, weather severity, seasons
affecting production, and a save-format migration path.

**Gate:** a 60-minute scripted playthrough from new game to a built structure,
save/reload mid-way, no invariant violation.

---

## Cross-cutting work that never gets its own milestone

- Art generation and lint for each new asset class (`ART_PIPELINE.md`).
- Golden-frame hashes for every visual subsystem that lands.
- Benchmark upkeep against the performance budget.
- API-freeze discipline: additive only; any reshape needs a deprecation cycle.
- Documentation updates here when a decision changes — an ADR is marked
  `superseded` rather than edited into agreement with reality.
