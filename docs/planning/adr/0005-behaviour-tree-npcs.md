# 0005 — NPC decisions use behaviour trees

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

"Smart NPCs" is a requirement. The mainstream options are utility scoring,
behaviour trees and GOAP. The choice matters mostly for **debuggability and cost
per agent at scale**, not for the ceiling: all three can produce convincing
townsfolk.

## Decision

Behaviour trees, data-defined (sequence, selector, condition, action, decorator)
with a per-agent blackboard, fed by a schedule and a needs model. Trees are
authored as data and may call Lua actions.

## Consequences

- Needs a tree runtime, a blackboard, a data schema, and a **debug view that
  prints the running path** — without that, BTs become unreadable at scale.
- Tests assert the *chosen branch* for a fixture blackboard, not merely that an
  NPC moved.
- Emergent behaviour comes from many small trees plus schedules, not from
  planning; if a behaviour needs to reason over alternatives, it is expressed as
  conditions and priorities.
- Utility scoring can be added later as a *node type* (a selector that scores
  children) without changing the runtime.

## Alternatives considered

1. **Utility scoring** — cheaper and more emergent for needs-driven behaviour,
   but hand-authored sequences (open shop, harvest, sleep) become awkward.
2. **GOAP** — strongest emergence, highest cost and hardest debugging; a
   classic solo-project sink.
3. **Hybrid utility+BT from day one** — more power than M2 needs; deferred as a
   node type rather than a rewrite.

## References

- `docs/planning/ARCHITECTURE_TARGET.md` §6
