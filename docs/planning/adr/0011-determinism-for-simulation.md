# 0011 — Every simulation system is deterministic and headless-testable

- **Status:** Accepted
- **Date:** 2026-11-09
- **Inherited from:** the 1.x fixed-timestep pillar, extended to new systems

## Context

1.x already runs on a fixed timestep with a scripted `--verify` harness that
asserts gameplay outcomes. The new systems — AI, procedural generation, economy,
weather, particles — are where determinism usually dies: unordered iteration,
wall-clock reads, ambient RNG, float accumulation through containers.

Determinism is also the project's only defensible advantage in this space:
almost no indie 2D engine can step its world headlessly and assert that a
30-day economy soak conserves money.

## Decision

Every new simulation system must be:

1. **Fixed-step** — advanced only from the sim clock, never from wall time.
2. **Seeded by named stream** — `terrain`, `loot`, `weather`, `ai`, `economy`;
   adding a draw in one stream must not shift another.
3. **Order-stable** — iteration over sets/maps is sorted by a stable key (entity
   id, cell id, agent id); unordered containers are not iterated in sim code.
4. **Serializable** — `SaveGame` round-trips to an identical state hash.
5. **Hashable** — each system exposes a state hash so a test can compare two runs
   or two seeds without rendering.

## Consequences

- Some fast code becomes less fast (sorted iteration, per-stream RNG). Accepted:
  the alternative is untestable simulation.
- CI gains cheap, high-value tests: same-seed equality, save/load equality,
  economy conservation, schedule satisfaction, generation validation.
- A grep gate rejects `std::chrono::`/`time()` usage outside the clock module and
  any iteration of `std::unordered_map` in simulation code paths. **Scope
  matters:** the gate covers the new sim/render-world modules, not the whole of
  `engine/src`, because `AudioSystem`, `AssetCache` and `CompanionServer` already
  hold unordered maps. Asset ids and audio voice ids must be assigned in load
  order rather than map order so they stay reproducible once the sim consumes
  them.
- Particle and lighting systems must observe state, never mutate gameplay state;
  that keeps golden gameplay hashes stable when visuals change.

## Alternatives considered

1. **Determinism only for gameplay-critical parts** — the line is never where you
   think, and economy/AI bugs are exactly what you need to reproduce.
2. **Snapshot/replay instead of determinism** — heavier, and hides the bugs that
   determinism surfaces immediately.

## References

- `docs/planning/ARCHITECTURE_TARGET.md` §5
- `experiments`: existing `--verify` and `ctest` harnesses
