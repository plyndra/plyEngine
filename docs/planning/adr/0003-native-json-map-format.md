# 0003 — Native JSON map format, no Tiled runtime dependency

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

The 1.x demo level is C++ constants (`examples/companion/platformer.hpp`); there
is no level format at all. The reference frames imply thousands of authored
cells, multiple layers, props, spawns and per-map metadata, which needs a data
format and an authoring path.

Tiled is the pragmatic authoring tool, but adopting TMX/JSON as the runtime
format makes an external tool's schema part of the engine's contract.

## Decision

The runtime format is **plyEngine's own JSON**: maps, tilesets, UI atlases and
asset manifests. Tiled is not a runtime dependency. One-time **importers** may
convert Tiled/Aseprite metadata into our schema when convenient.

## Consequences

- We must build and maintain: a schema, a validator, a round-trip test, and an
  authoring path (M1a `MapLoader`; a minimal editor only as far as M4 needs).
- The schema is free to carry what the sim needs — per-tile metadata, autotile
  mask tables, per-layer flags, provenance — without fitting another tool's
  model.
- Seeded generation writes the same format as hand-authored maps, so the
  generator is testable by round-tripping through the loader.
- Authoring is slower than Tiled until a minimal editor exists; accepted cost.

## Alternatives considered

1. **Tiled TMX/JSON as the runtime format** — free editor, but couples the engine
   to an external schema and still needs a loader.
2. **Tiled now, native later** — two formats to keep in sync, and the "later"
   never gets prioritised.
3. **Binary format** — faster to load, much worse to diff, review and generate.

## References

- `docs/planning/ARCHITECTURE_TARGET.md` §4
