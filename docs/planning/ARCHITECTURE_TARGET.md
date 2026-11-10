# Architecture target — top-down simulation engine

Reference frames: `/testpng/` (three screenshots, 2560×1440). Measured: dominant
fine period 16 px, coarse cluster period 48 px, 10.6k–18.6k unique colours per
frame — a **style** reference, not a pixel-exact spec. Internal render resolution
and tile size are open decisions (see `ROADMAP_TARGET.md`); every budget below is
stated **per internal resolution** for that reason.

Target capability: sandbox survival games with behaviour-tree NPCs, procedurally
generated towns, and an agent-driven economy.

This document is the map. Each layer names the modules it adds, what it must
prove, and what it may reuse from 1.x.

---

## 1. What the reference frames require

| Observation in the frames | Requirement it creates |
|---|---|
| Terrain reads organic: grass↔dirt↔water↔cliff with soft edges, no square seams | Autotiled terrain with per-terrain-pair transition sets (blob-47 or layered soft edges) |
| Player and NPCs pass **behind** trees, fences, roofs; canopies overhang the walkway | Y-sorted depth with entities and props interleaved, plus an occlusion layer |
| Buildings are 5–7 tiles wide, with doors, roofs, chimneys, drop shadows | Off-grid placed objects with their own collision footprint, pivot, shadow and draw layer |
| Same village at noon and at 21:20 in rain; a near-black cave with a lit radius and glowing flora | Per-map ambient tint, additive light pools, emissive props, weather particles, vignette |
| Water and crops and torches animate; rain falls and splashes | Animated tiles driven by the sim clock, independent of entity animation |
| Hotbar, health/energy bars, ornate minimap, calendar, 3-slice chat panel, floating combat text | Screen-space UI layer with anchors, 9-slice/3-slice, gradients, and world→screen text |
| Camera centred on the character; world larger than the viewport | Camera follow with smoothing, integer snapping and map clamp |
| Several hundred distinct tiles and props | An authoring pipeline and an asset format, not C++ constants |

---

## 2. Render pipeline

**Fixed by [ADR 0012](adr/0012-internal-resolution-and-tile-size.md):** the world
renders at **640×360** internal, 16 px tiles (32 px for buildings and large
props), presented with an integer nearest-neighbour upscale — 3× at 1080p, 4× at
1440p — with the HUD drawn above the upscale at 2×/3×. A full screen of ground is
40 × 22.5 ≈ 900 cells.

Today the demo hardcodes its draw order inside `examples/companion/main.cpp`:
tiles, then sprites, then text. A simulation game cannot.

```
RenderTarget (world, integer-scaled internal resolution)
  ├─ GroundTileLayer          tiles, culled by camera rect
  ├─ GroundDecorLayer         grass tufts, pebbles, paths — y-sorted lightly
  ├─ ShadowLayer              ellipse shadows under entities/props
  ├─ WorldLayer (y-sorted)    props, buildings(base), NPCs, player, items
  ├─ OverheadLayer            building roofs, tree canopies, foreground tiles
  ├─ ParticleLayer            weather, dust, fireflies, spores
  ├─ LightPass                ambient tint × light map (multiply)
  └─ PostPass                 vignette, optional palette LUT
Present: nearest-neighbour integer upscale of the world target
  └─ UILayer                  screen space, drawn after upscale at native res
```

**Modules:** `RenderTarget`, `SceneRenderer` (named layers + sort keys),
`Camera2D` (follow/snap/clamp), `ParticleSystem`, `LightMap`.

**Must prove:**
- Draw order is data, not code: a test renders a fixture scene and asserts the
  layer sequence and per-layer quad counts.
- Integer snapping: camera at x=10.4 and x=10.6 produce identical frames after
  rounding — a **structural** hash (snapped position, tile-rect list, emitted
  quads), never a framebuffer comparison: pixel hashes flake on software GL, see
  [ADR 0014](adr/0014-structural-hashes-for-visual-tests.md).
- Upscale is nearest and integer: a 1 px line at internal res is *k* px wide at
  output, never blended.
- The light pass is deterministic from (clock, light list) → a structural hash
  of sampled light values at named coordinates.

**Reuse:** `SpriteBatch`, `SpriteAtlas`, `Texture` (`TextureFilter::Nearest`),
`Shader`, `OrthoCamera` (extended).

---

## 3. Tile worlds

`Tilemap` in 1.x is collision-only: `setSolid`/`isSolid`/`query`. There is no
tile data at all; the demo draws a single region for every solid cell.

**New modules**

| Module | Responsibility |
|---|---|
| `TileSet` | Atlas + per-tile metadata: solid, variant group, autotile mask role, animation frames, collision polygon, palette id |
| `TileLayer` | Int32 tile ids per cell; per-layer flags (below/above entities, animated, opaque) |
| `AutoTileResolver` | (neighbour mask, layer, tileset) → tile id, using the blob-47 table; resolved at load, cached |
| `WorldMap` | Chunked storage (e.g. 16×16 cells), per-chunk dirty flags, view-frustum culling, cross-chunk neighbour queries for autotiling |
| `PropLayer` | Placed objects: multi-tile sprites, collision rects, pivot, shadow, occlusion flag, optional interaction |
| `YSort` | Sort key = feet Y (with a tiebreaker id) across props, NPCs, items, player |
| `TileAnimator` | Per-tile frame sequences advanced from the sim clock; phase offset per cell so water doesn't pulse in lockstep |

**Must prove:**
- Autotile table completeness: all 47 neighbour cases resolve; unknown masks
  fail loudly rather than drawing a hole.
- Culling correctness: quads submitted == tiles intersecting the camera rect
  (exact count assertion), and a chunk-boundary fixture produces no seam.
- Y-sort order: a fixture with crossing entities and a tall prop yields the
  expected order array; identical on repeat (ties broken by id).
- Animated tiles: frame index after *n* steps matches expectation; two cells
  with different phases are offset as configured.

**Reuse:** `Tilemap` stays as the collision query (a `ColliderQuery` source) but
is fed from `TileSet` solidity rather than being the source of truth.

---

## 4. Content pipeline

**Decision:** native JSON in plyEngine's own format; no Tiled runtime
dependency ([ADR 0003](adr/0003-native-json-map-format.md)). One-time importers
may convert Tiled/Aseprite metadata into our schema.

```
assets/maps/<name>.json          layers, tiles, props, spawns, map metadata
assets/tilesets/<name>.json      regions, mask table, variants, animation, collision
assets/ui/<name>.json            regions, 9-slice margins, stretch modes, font refs
assets/manifests/<name>.json     provenance: provider, model, seed, licence, sha256
```

**Modules:** `MapLoader` (+ validator), `TileSetLoader`, `UIAtlasLoader`,
`AssetManifest` (also the download manifest, [ADR 0004](adr/0004-download-on-first-run-assets.md)).

**Must prove:** load → save → load round-trip equality for maps and tilesets;
validator rejects (and names) out-of-range tile ids, unregistered tileset
references, missing autotile masks, and prop collisions outside bounds.

---

## 5. Simulation core

1.x already has the right shape: fixed timestep, explicit ECS, deterministic
`--verify`. Extend it:

| Module | Responsibility |
|---|---|
| `WorldClock` | Minutes → hours → days → seasons; single source of time for tint, schedules, tile animation, economy ticks, crop growth |
| `SimSystems` | Explicit, ordered system list per step; the order is asserted, not incidental |
| `RngStreams` | Named, seeded streams (`terrain`, `loot`, `weather`, `ai`, `economy`) so adding a feature never shifts another's randomness |
| `SaveGame` | Serialize ECS + world + clock + NPC/economy state; versioned schema |
| `SpatialIndex2D` | Existing `SpatialHashGrid`, extended for queries by layer and by ID component (AI neighbours, economy participants) |

**Must prove:** same seed + same input → identical state hash after N steps;
serialize→deserialize→hash equality; system order fixture (swap two systems and
the test notices); no wall-clock reads in simulation code (grep gate).

---

## 6. Navigation and NPCs

**Decision:** behaviour trees ([ADR 0005](adr/0005-behaviour-tree-npcs.md)).

| Module | Responsibility |
|---|---|
| `NavGrid` | Walkable mask derived from tiles + props; per-agent footprint (1×1, 2×2 for animals) |
| `PathFinder` | A* with tie-breaking by cell id; optional flow field for crowds heading to the same goal |
| `NpcAgent` | Needs (hunger, energy, social), home/work/leisure POIs, inventory, money, relationships |
| `BehaviourTree` | Data-defined nodes (sequence, selector, condition, action, decorator), blackboard per agent |
| `Perception` | What an agent knows (visible entities, remembered positions with decay) |
| `Schedule` | Per-role daily plan feeding BT goals (a fisher is at the dock 06:00–12:00) |
| `Dialogue` | Data-driven lines, optionally scripted in Lua |

**Must prove:** path cost equals a hand-computed optimum on a fixture map;
unreachable goals fail fast rather than spinning; a simulated day places every
NPC at its scheduled POI within tolerance; BT fixture asserts the *chosen
branch* given a blackboard, not merely that the NPC moved.

---

## 7. Procedural generation

Seeded, staged, and validated — generation that produces an unplayable town is a
bug, not a re-roll.

```
seed → terrain (noise + biome rules) → water/cliff carve → POI placement
     → road graph (A* between POIs) → lot subdivision → building assembly
     → prop decoration → population + economy seeding → VALIDATION
```

**Validation gates (hard fail):** every POI reachable from every other POI; no
building overlapping a road or water; no resource node inside a wall; all
network edges symmetric; population within configured bounds; town has ≥1 water
source and ≥1 shop. Emitted as a report and asserted in CI for a fixed seed set.

**Must prove:** fixed seed → identical town hash; validation pass on a matrix of
seeds (e.g. 200) with zero failures; a deliberately broken rule fails the gate.

---

## 8. Economy

**Decision:** agent-to-agent bargaining, first implemented as a **batched double
auction** behind a policy interface ([ADR 0006](adr/0006-double-auction-economy.md)).

| Module | Responsibility |
|---|---|
| `Item` / `ItemStack` / `Inventory` | Goods, quantities, containers (player, NPC, shop, crate) |
| `Market` | Per-good order book; clears in batches; emits trades with prices |
| `MarketPolicy` | Interface: `DoubleAuction` first; `PairwiseBargain` later without an API break |
| `Production` | Recipes/outputs: farms, fisheries, smiths — drive supply |
| `PriceHistory` | Ring buffer per good for UI graphs and tests |

**Must prove:** money conservation per clearing batch; no item appears or
vanishes except through a declared production/consumption event; prices stay
finite and positive over 30 simulated days; a supply shock moves prices in the
expected direction within N ticks.

---

## 9. UI

Front-loaded by [ADR 0007](adr/0007-hud-first.md); detailed in
[UI_FRAMEWORK.md](UI_FRAMEWORK.md). Summary: screen-space layer above the
world, anchor+offset+scale layout, 9-slice/3-slice with margins from the atlas
(or derived from alpha when declared absent), widgets (panel, bar, slot,
hotbar, inventory grid, tooltip, dialogue, minimap, floating text), and a single
input router shared by keyboard/mouse and the phone dev client.

---

## 10. Audio

Extend `AudioSystem`: looping music with crossfade, ambience beds (rain, cave
drip), and buses (`master`/`music`/`sfx`/`ambience`) so weather can duck music.
Assertions stay headless via the null backend: voice counts, loop restarts,
bus gains.

---

## 11. Performance budget

Design target, asserted by a headless benchmark in CI:

All figures are **targets to assert once a benchmark exists**, not measurements.

| Metric | Budget |
|---|---|
| Visible quads, per internal frame | ≤ 4 000 (tiles + props + entities + particles). At 640×360 with 16 px tiles a full screen is 40×22.5 = 900 ground cells, leaving room for decor, props, entities and particles under budget |
| Draw calls per world frame | ≤ 8 (ground, decor, shadow, world, overhead, particles, light, post) |
| NPC pathfinding | 100 agents re-pathing at 1 Hz within 2 ms |
| Economy tick | 200 agents, 30 goods, clear within 1 ms |
| Frame CPU (headless sim, no GPU) | fixed step of 1/60 s completed in ≤ 8 ms p95 on the dev host |

---

## 12. Explicit non-goals

3D rendering, physics beyond swept AABB, online multiplayer, shader graph
authoring, a general-purpose editor UI (a map editor is in scope only as far as
M4 needs it), mobile export, and any lighting model heavier than one multiply
pass plus additive light sprites.

---

## 13. Reuse from 1.x

| Keep as-is | Extend | Replace |
|---|---|---|
| Archetype ECS, fixed timestep, `World`, `Entity` | `OrthoCamera` (+follow/snap/clamp) | `Tilemap` collision-only rendering assumptions |
| `SpriteBatch`, `SpriteAtlas`, `Texture`, `Image`, `Spritesheet`, `Font`, `TextRenderer` | `Sprite` (+rotation/scale/flip/frame), `drawSprites` (+y-sort) | Demo-hardcoded draw order |
| `AudioSystem` (voices, pitch/pan, loop flag) | + buses, music, ambience | — |
| `CompanionServer`/`CompanionInput`/`RoomCode` | Repurposed as the dev/test client ([ADR 0002](adr/0002-phone-companion-as-dev-tool.md)) | Player-facing framing |
| `Scripting` (sandboxed Lua) | + NPC behaviour, dialogue, recipes, economy rules | — |
| `--verify` + `ctest` + `release-check.sh` culture | + art lint, sim determinism, generation validation | — |
