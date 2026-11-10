# 0012 — Internal resolution 640×360, 16 px tiles, 32 px large props

- **Status:** Accepted
- **Date:** 2026-11-09
- **Depends on:** [0001](0001-top-down-sim-identity.md)

## Context

Every budget in `ARCHITECTURE_TARGET.md` is expressed in quads, and every rule in
`ART_PIPELINE.md` is expressed in tiles, but neither had a resolution. The
planning review measured the reference frames (2560×1440, dominant fine period
16 px, coarse cluster 48 px, 10.6k–18.6k unique colours per frame) and confirmed
they are a **style** reference rather than a pixel-exact spec — so the scale had
to be chosen, not inferred.

The arithmetic that forced a decision: at 1920×1080 internal with 16 px tiles a
single screen is 120×68 = **8 100 ground cells** before props or particles, which
exceeds the quad budget on tiles alone.

## Decision

- **Internal render resolution: 640×360.** Present with an integer nearest-
  neighbour upscale — 3× at 1080p, 4× at 1440p.
- **World tiles: 16 px.** **Buildings and large props: 32 px.** One grid for
  collision and autotiling; large art spans multiple cells.
- HUD art is drawn above the upscale at 2×/3× integer scale (never fractional).
- Quad budget: **≤4 000 per internal frame** (900 ground cells at 640×360 leaves
  room for decor, props, entities and particles).

## Consequences

- A full screen of ground is 900 quads, so layered rendering fits the budget with
  headroom; culling tests have concrete expected counts (40×22.5 tiles).
- Autotiling, prop footprints and collision share one 16 px grid, which keeps
  `AutoTileResolver` and `PropLayer` free of per-asset scale special cases.
- Large props and buildings are the only art class needing a 32 px canvas; the
  art pipeline carries two geometry classes instead of one.
- 640×360 UI art is small enough that a 3× HUD is drawn from ~40 px panels —
  good for the reference HUD's chunky frames, and cheap to author.
- Text at 640×360 must be integer-sized (10/20 px) or it will shimmer; this
  removes any temptation to fractional font scaling.
- Native-resolution rendering stays possible as a debug/observation mode but is
  not the shipping path.

## Alternatives considered

1. **960×540 internal** — 60×34 tiles visible, twice the art pixels per frame,
   less per-tile detail at the same output size.
2. **1920×1080 internal** — arithmetically impossible against the quad budget
   with 16 px tiles.
3. **32 px tiles everywhere** — halves art count, loses the dense look the
   reference frames are built on.
4. **Mixed 16/24/32 grids** — one grid, one autotile system, no exceptions; mixed
   grid sizes would leak into collision and culling.

## References

- `ARCHITECTURE_TARGET.md` §11 (budget), §3 (tile worlds)
- `ART_PIPELINE.md` §7 (per-class recipes)
