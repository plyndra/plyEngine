# 0010 — Art is generated self-hosted into assets plyEngine owns

- **Status:** Accepted
- **Date:** 2026-11-09

## Context

The target look needs a large, internally consistent asset set (terrain
components and 47-case assemblies, props, buildings, 4-direction characters,
animated tiles, a 9-slice UI kit, icons). Options: license existing packs, hire
an artist, or generate.

Licensed packs (surveyed: several strong CC0 sets, one dense CC BY 4.0 village
set) are fast but impose style limits, attribution duties, and — for the best
village art — a licence we do not control. Commissioning is the highest quality
per asset but slowest to first pixel.

## Decision

Generate the art **self-hosted** with permissive base models (SDXL, OpenRAIL++-M;
FLUX schnell Apache-2.0 as fallback; **FLUX dev forbidden**) into assets the
project owns, using the pipeline in `ART_PIPELINE.md`: generate components,
assemble deterministically, lint hard, commit with provenance.

CC0 packs remain in-repo knowledge only as style reference, geometry fixtures and
(palette-permitting) LoRA training data — they do not ship.

## Consequences

- No per-asset licence obligations in shipped builds; no dependency on a hosted
  service; no per-image cost. The 8 GB RTX 3050 on the dev host is sufficient for
  SDXL at 1024².
- The project takes on a **toolchain**: ComfyUI + workflows + LoRA + palette
  quantise + assemblers + linter. Generation never runs in CI; only assembly and
  lint do, which keeps builds fast and hermetic.
- Quality is not automatic: expect a 20–30 % human fix pass, recorded per asset
  in `human_fixup`. Style lock relies on a palette LUT, a LoRA trained on
  approved CC0/owned references, and pinned workflows.
- Disclosure duties appear if generated art ships (Steam content survey and
  friends) — decided deliberately, recorded in `ART_PIPELINE.md` Appendix C.
- Hosted pixel-art services (Retro Diffusion, PixelLab) remain documented
  fallbacks, reachable over MCP, if local quality stalls on characters.

## Alternatives considered

1. **License CC0 packs** — fastest path to a frame, no toolchain; rejected
   because the target look needs cross-terrain transitions and UI/HUD density
   that no single pack provides, and mixed packs drift in style.
2. **Commission one biome** — best coherence; rejected on schedule (weeks before
   anything is judged) rather than on principle.
3. **Hybrid: license terrain, generate props/UI** — plausible later; deferred,
   because it reintroduces the licence tracking the self-hosted path removes.

## References

- `docs/planning/ART_PIPELINE.md`
- `/testpng/` (visual target)
