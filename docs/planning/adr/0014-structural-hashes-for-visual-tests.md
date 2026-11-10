# 0014 — Test artefacts: structural hashes in CI, pixel goldens only in a pinned container

- **Status:** Accepted
- **Date:** 2026-11-09
- **Affects:** every "hash" written in `ARCHITECTURE_TARGET.md` and
  `ROADMAP_TARGET.md`

## Context

The 2.0 plan leans on hashes to assert visual and simulation invariants:
corner-invariance for nine-slice, integer snapping for the camera, layer order,
light values, particle determinism, and "golden frames" per milestone. But CI
renders with **Mesa llvmpipe**, and pixel output is not stable across Mesa
versions, driver flags, or even Xvfb/xvfb-run configurations. A pixel-hash gate
is the classic flaky check that gets switched off within two weeks — which would
leave the renderer with no gate at all.

## Decision

Two artefact classes, and they are not interchangeable:

1. **Structural hashes — the CI gate.** Hash the *state that encodes the
   invariant*, computed from the same code path that draws:
   - UI: laid-out rects (integers), widget order, fill fractions, margin values,
     and per-region source rectangles.
   - World: layer sequence, per-layer quad counts, y-sort order array, camera
     position after snapping, culled tile-id list hash, light sample values at
     named coordinates.
   - Simulation: state hashes per system, save-file bytes.

   These are deterministic on any driver, cheap to diff, and their failure names
   the invariant instead of a hex string.

2. **Pixel goldens — opt-in, local, container-pinned.** A rendered image may be
   compared byte-for-byte **only** when the comparison runs in the pinned
   toolchain image (same Mesa, same Xvfb flags, same internal resolution) and is
   invoked explicitly (`--update-goldens` / `--check-goldens`). They are never a
   default CI gate.

## Consequences

- Every "frame hash" phrase in the planning docs means **structural hash**; the
  few that implied pixels are corrected to say so.
- The integer-snap test asserts the snapped camera position and the resulting
  tile-rect list, not a bitmap — which is stricter about *the actual bug* (a
  half-pixel camera) and immune to driver drift.
- Nine-slice corner invariance hashes the four corner regions' source rects plus
  the emitted vertices for those corners; a wrong margin changes both.
- Reviewers can read a failing diff: `rects[3].w 20 -> 21` beats `frames differ`.
- Pixel goldens stay available for aesthetic review (side-by-side with
  `/testpng/`), which is a **human rubric**, not a hash — consistent with the
  measured finding that the reference frames are a style reference, not a pixel
  spec.

## Alternatives considered

1. **Pixel goldens as the default CI gate** — strongest visual guarantee,
   highest maintenance, and unachievable on software GL without pinning the whole
   runner image.
2. **No hashes at all, assert only "it drew something"** — the state 1.x is
   already in for its demos; explicitly what 2.0 is trying to escape.
3. **Structural hashes plus committed PNGs compared with a perceptual diff** —
   adds a threshold nobody can defend and a library dependency, for a signal the
   rubric already provides.

## References

- `docs/planning/ARCHITECTURE_TARGET.md` §2 (must-prove list)
- `docs/planning/ROADMAP_TARGET.md` M1a a2/a7, M1b b6/b8/b9, gates
- `evals/2.0.md` (grader definitions)
