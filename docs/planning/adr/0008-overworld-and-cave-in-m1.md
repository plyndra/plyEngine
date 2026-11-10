# 0008 — M1 content scope: overworld plus cave

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

The reference frames show two environments: a village overworld (day and dusk)
and a near-black cave with glowing flora and water. One environment is the
cheapest validation; three is premature content proof.

## Decision

M1 covers **overworld and cave**, no more.

## Consequences

- Forces map-level metadata early: ambient tint, light model, tileset reference,
  weather enable/disable. That pressure is the point — one environment can be
  special-cased by accident, two cannot.
- Roughly doubles the M1 art budget versus a single environment (two tileset
  families, two palettes, a light model that must work in near-darkness).
- The cave is the harder lighting case; solving it in M1 means the overworld's
  dusk shot is nearly free.
- Third biome (desert/snow) is deferred to M4-era content, where generation
  makes variety cheap.

## Alternatives considered

1. **One biome** — cheapest, but leaves the ambient/lighting architecture
   unvalidated until later.
2. **Three biomes** — content proof before the renderer has proven itself once.

## References

- `docs/planning/ROADMAP_TARGET.md` M1
