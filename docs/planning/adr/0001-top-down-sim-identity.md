# 0001 — Target identity: top-down sandbox simulation

- **Status:** Accepted
- **Date:** 2026-11-09
- **Supersedes:** the platformer-only reading of `docs/DESIGN.md` "Purpose"

## Context

plyEngine 1.0 shipped as a 2D side-view action engine whose flagship demo is a
four-player platformer driven by phone controllers. The owner's target is
different: games that look like the reference frames in `/testpng/` — a dense
top-down pixel-art world with layered terrain, y-sorted depth, day/night
lighting and a heavy HUD — supporting sandbox survival play with smart NPCs,
generated towns and an in-game economy.

Those two targets disagree about camera, player model, world representation and
input, so the engine cannot promise both without carrying two personalities.

## Decision

plyEngine's target identity is a **top-down sandbox survival engine**. New
subsystems are designed for that target first. The 1.x side-view capability
remains available and tested (it is what the frozen API describes), but it is no
longer the design centre of gravity.

## Consequences

- New work follows `ARCHITECTURE_TARGET.md`: tile worlds with layers and
  autotiling, y-sorting, screen-space UI, light pass, simulation clock.
- The 1.0 API stays frozen and additive; nothing is removed. A 2.0 boundary is
  expected and will be declared explicitly, not slipped into a minor release.
- The existing platformer demo keeps working during the transition and stays in
  CI; it stops being the promotion asset once the village frame exists.
- The promotion plan's hero asset and "try it in 30 seconds" claim will be
  rebuilt against the new demo (tracked in `docs/PROMOTION_PLAN.md`).

## Alternatives considered

1. **Two parallel products** (platformer engine + sim engine) — doubles every
   renderer/input decision.
2. **Stay platformer-first, add sim later** — the world representation is the
   foundation; retrofitting layers/y-sort later means rewriting the renderer.

## References

- `docs/planning/ARCHITECTURE_TARGET.md`
- `docs/planning/ROADMAP_TARGET.md`
