# 0007 — The UI framework is built first

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

Three candidate first slices: the visual world (tilemap renderer, autotiling,
y-sort), the simulation (pathfinding, NPC schedules, economy, headless), or the
HUD and UI framework. Each front-loads a different risk. In the reference frames
the HUD is roughly a third of what makes a frame read as a finished game, and
the engine currently has no screen-space UI at all — not even anchors.

## Decision

Build the **UI framework first** (M1a), against a real map loaded through the
native JSON format, so the HUD is not a shell over nothing.

## Consequences

- Fast visible progress on something the engine genuinely lacks, and it defers
  no basic capability: the map loader it needs is also a prerequisite for the
  world renderer.
- **Risk accepted:** a polished HUD can disguise a missing world. Mitigation is
  structural — the HUD is built over a real (if plain) map from `MapLoader`, and
  the milestone gate requires that map to render behind it.
- The 9-slice/atlas schema settled here constrains the art pipeline's UI class,
  which is cheaper to get wrong now than after 200 assets exist.
- The world renderer (M1b) starts one milestone later; the village frame arrives
  at M1c.

## Alternatives considered

1. **World renderer first** — validates the art pipeline and autotiling sooner,
   but leaves the engine with no HUD and no input router, and the HUD is the
   part that needs the most API iteration.
2. **Simulation first** — cheapest to test, but months pass before anything can
   be compared to the reference.
3. **World and HUD in parallel** — two half-finished frameworks, one review
   queue.

## References

- `docs/planning/UI_FRAMEWORK.md`
- `docs/planning/ROADMAP_TARGET.md` M1a
