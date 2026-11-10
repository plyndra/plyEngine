# 0009 — Engine work lands on `feat/*` branches; docs and assets go to main

- **Status:** Accepted
- **Date:** 2026-11-09
- **Applies to:** workflow from 2026-11-09 onward

## Context

Documentation has gone straight to `main` throughout the promotion work: small,
reversible, and CI verifies it. Engine subsystems are a different risk class —
they touch public headers, the API-freeze gate, the render pipeline and CI
minute budgets. A half-finished render-target change on `main` makes "releasable"
stop meaning anything.

## Decision

- **Engine subsystems:** one `feat/<subsystem>` branch per unit, PR with the
  freeze gate and CI as reviewer, merged when green.
- **Docs, assets, eval records:** straight to `main`, as today.

## Consequences

- `main` stays releasable during 2.0 work; a bisect still means something.
- One merge step and duplicate CI minutes per subsystem — accepted.
- Long-lived branches are discouraged: a subsystem branch should be a handful of
  commits, not a second trunk.
- Review economics stay as `AGENTS.md` defines them: one independent reader
  running the acceptance command by default, specialists only for named risks
  (determinism, render-thread boundaries, freeze compliance).

**Amended 2026-11-09 (pre-implementation audit):** `main` has **no branch
protection** (`gh api …/branches/main/protection` → 404). That is deliberate, not
an oversight: docs and assets must keep landing directly, so protection with
required checks would block the workflow this ADR depends on. The compensating
controls are `scripts/release-check.sh` (version, API drift, links — now
including `docs/planning/**`, tests) plus the review rule above. Revisit only if
a bad push to `main` actually happens.

## Alternatives considered

1. **Everything to main** — fastest, worst failure mode during a renderer
   rewrite.
2. **One long-lived `2.0` branch** — clean isolation, but no continuous
   verification on the trunk and a large merge at the end.

## References

- `AGENTS.md` (review economics), `docs/WORKSPACE_POLICY.md` if present
