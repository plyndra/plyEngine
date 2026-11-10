# 0004 — Assets are downloaded on first run

- **Status:** Accepted
- **Date:** 2026-11-09
- **Interacts with:** promotion eval C0.6 ("try it in 30 seconds")

## Context

1.x ships a 12 MB archive whose demo synthesises its art in code specifically to
keep `--verify` hermetic and the download small. A top-down world needs several
megabytes of tilesets and props, and generation (`ART_PIPELINE.md`) will keep
producing more. Embedding everything in the binary inflates every release;
shipping an `assets/` folder inflates every archive.

## Decision

Ship a **minimal fallback** asset bundle in the archive (enough to render a
placeholder scene offline), and **fetch the full asset set on first run** from a
pinned manifest with per-file sha256, cached in a user data directory.

## Consequences

- Required before this can ship: a manifest schema (shared with the art
  provenance records), a fetcher with resume/retry, checksum verification, a
  local cache with a version stamp, and a documented offline path
  (`--assets <dir>`, `--offline`).
- **Hermetic tests must not touch the network.** `--verify` and CI run against
  the bundled fallback or a checked-in fixture directory; the fetcher has its own
  tests with a local HTTP server.
- The promotion plan's "no barriers" pre-flight gains a network dependency:
  first-run offline shows the fallback scene plus a clear message, and the
  30-second claim must be worded honestly (`evals/promotion.md` C0.6 is updated
  when this lands).
- Redistribution rights become relevant only for the **bundled fallback**, which
  will be our own generated art, so the constraint stays simple.

## Alternatives considered

1. **Embed all assets in the binary** (hermetic, self-contained, bigger
   downloads) — rejected for size as the asset count grows.
2. **Ship `assets/` in the archive** — no network, but every release grows and
   the first run pays the full cost anyway.
3. **Download on demand per map** — smallest first run, most failure modes.

## References

- `docs/planning/ART_PIPELINE.md` §6 (manifest = provenance)
- `evals/promotion.md` C0.6
