# Art pipeline — self-hosted generation

**Decision:** plyEngine's art is generated with self-hosted models into assets the
project owns ([ADR 0010](adr/0010-self-hosted-generated-art.md)). No
third-party art ships in builds; licences stay clean by construction, cost stays
zero per image, and nothing depends on a hosted service staying alive.

The pipeline is deliberately **not** "prompt → finished tileset". Image models
cannot hit exact pixel grids or guarantee seamless joins. So:

```
generate components  →  assemble deterministically  →  lint hard  →  commit with provenance
   (AI, fuzzy)            (code, exact)                (CI, failing)     (receipts)
```

Quality lives in the middle and the last stage, not the first.

---

## 1. Hardware and model constraints

Measured on this host (PVE3): **NVIDIA RTX 3050, 8 GB VRAM, 12 cores, 31 GB RAM.**

| Constraint | Consequence |
|---|---|
| 8 GB VRAM | SDXL at 1024² with fp16 and `--medvram` is realistic (~6–15 s/image). Batch generation happens overnight or in a background queue, never in a test. |
| FLUX.1 [dev] is too heavy **and** non-commercial | Not used. |
| FLUX.1 [schnell] (Apache-2.0) is viable but slow on 8 GB | Fallback for prompts SDXL fails; not the default. |
| No GPU in CI | Generation never runs in CI. Only **deterministic assembly and linting** do. |

### Model licence rules (checked before first use, recorded per asset)

| Model | Output commercial use | Rule for this project |
|---|---|---|
| **SDXL** — CreativeML OpenRAIL++-M | Allowed, with a banned-use list that travels with the licence; Stability's model card separately says "research purposes only", which the licence text does not | **Default.** Acceptable for shipping; record model + version + licence in provenance |
| **FLUX.1 [schnell]** — Apache-2.0 | Unrestricted | Allowed; fallback only |
| **FLUX.1 [dev]** | Non-commercial, with a clause read by some as restricting outputs | **Forbidden** |
| Qwen-Image / Z-Image / HiDream (Apache-2.0 / MIT class) | Unrestricted | Allowed if a LoRA becomes available |
| Pixel-art LoRAs (community) | Depends on the LoRA's own licence, not the base model | Check each LoRA's licence; record it; prefer LoRAs whose terms permit commercial output |

Any LoRA whose licence is unknown is treated as forbidden until written terms
exist. Same rule as the CC packs in Appendix A.

---

## 2. Toolchain

| Piece | Choice | Why |
|---|---|---|
| Runtime | **ComfyUI**, local, pinned commit | Node graph is a file, so a spec can name a workflow and a seed |
| Base model | SDXL + a pixel-art LoRA | Fits 8 GB, permissive licence |
| Structure control | ControlNet (openpose/grid for characters, canny/depth for props) | Keeps silhouettes repeatable across a set |
| Identity/consistency | IP-Adapter against a reference sheet | Same outfit/palette across 16 frames |
| Pixel snapping | nearest-exact downscale, then palette quantize | Removes "AI-mush"; output stays on the grid |
| Palette quantize | Aseprite CLI (`--palette`, `ColorQuantization`) or our own LUT pass | Deterministic, scriptable, CI-runnable |
| Autotile assembly | **our own assembler** in `tools/artgen/assemble` | Tilesetter/TilePipe2 prove the approach; we need it in CI and in our manifest format |
| 9-slice assembly | our own, with alpha-contour margin derivation | Same reason; mirrors `slicewright`'s inspect-after-generate pattern |
| Hosted fallback | Retro Diffusion ($0.03/img, MCP) or PixelLab ($12/mo, 4-/8-dir rotation) | If local quality stalls on characters, these are the cheap escape hatches — see Appendix B |

Reference architectures worth copying from: **`sprite-foundry`** (ComfyUI +
ControlNet + mechanical gates + lifecycle tracking + an engine-side lighting
check) and **`pixelmon`** (CLI over ComfyUI with `--tile`, locked seeds, and
contact sheets that print each seed).

---

## 3. Directory layout

```
tools/artgen/
  spec/            one JSON per asset class: prompt, model, LoRA, seed, palette, geometry
  workflow/        pinned ComfyUI graphs (tileable-terrain.json, prop.json, char-4dir.json, ui9.json)
  gen/             runner: submits a spec to ComfyUI, stores raw candidates + seed
  post/            palette LUT, alpha key, nearest-exact snap, outline, shadow generation
  assemble/        blob47.py, sheets.py, nineslice.py, atlas.py   (deterministic, no AI)
  lint/            the gates in §5
  emit/            writes assets/**/*.png and assets/manifests/*.json
assets/
  tilesets/  tiles/  characters/  ui/  maps/  manifests/  licenses/
```

Raw candidates live under `tools/artgen/out/` (git-ignored). Only linted,
accepted assets and their manifests enter `assets/`.

---

## 4. Spec format

```json
{
  "asset_class": "terrain_components",
  "name": "grass",
  "model": { "base": "sdxl", "lora": "pixelart-xl@v1", "licence": "openrail++-m" },
  "workflow": "workflow/tileable-terrain.json",
  "seed": 20261109,
  "palette": "assets/licenses/palettes/field-24.json",
  "geometry": { "tile_px": 16, "components": ["centre", "edge_n", "corner_outer", "corner_inner"] },
  "prompt": "16x16 seamless grass ground tile, top-down, flat lighting, no perspective, dense turf texture",
  "negative": "text, watermark, gradient, 3d, perspective, anti-aliasing",
  "candidates": 12,
  "accept": { "max_colours": 24, "alpha": "binary" }
}
```

Rules: every spec pins model + LoRA + workflow + seed; `candidates ≥ 8` so a
choice exists; the accepted candidate's index is recorded in provenance.

**Determinism note:** generation is *not* deterministic across model or driver
versions, and we do not pretend otherwise. What is deterministic and enforced is
everything after generation: assembly, quantisation, linting and the committed
bytes. The committed PNG is the source of truth; the manifest explains how it
came to exist.

---

## 5. Lint gates (CI, failing)

| Gate | Check | Failure mode it prevents |
|---|---|---|
| `blob47-complete` | all 47 neighbour cases present in an assembled terrain set; mask ordering matches `AutoTileResolver` | invisible holes at edges |
| `seam-continuity` | for each terrain pair, shared edge rows/cols match pixel-exactly where required | visible grid lines |
| `palette` | every pixel ∈ declared palette; ≤ declared max colours; store as an LUT index check | style drift between generated assets |
| `alpha-binary` | alpha ∈ {0,255} for pixel art (props may declare an exception) | halo edges in-game |
| `grid` | sizes are exact multiples of the tile/cell size; regions inside atlas bounds | silent blur from a 15 px tile |
| `nine-slice` | margins declared or derived; corners stretch-invariant at 3 target sizes; no sampling outside region | squashed panels, bleeding borders |
| `pivot-shadow` | props declare a feet pivot and a shadow region | floating objects |
| `provenance` | every PNG has a manifest entry with model, licence, seed, spec hash, asset hash | unlicensed or unaccountable art |
| `no-placeholder` | no asset marked `placeholder: true` may be referenced by a shipped map | forgetting to replace a grey square |

`ply_art_lint` is a normal CMake target and a CI step. Lint failures are build
failures, exactly like `release-check.sh`.

---

## 6. Provenance record

```json
{
  "asset": "assets/tilesets/grass-dirt.png",
  "sha256": "…",
  "asset_class": "terrain_autotile",
  "provider": "self-hosted",
  "model": { "base": "sdxl-1.0", "hash": "…", "lora": "pixelart-xl@v1", "loras_hash": "…" },
  "workflow": "workflow/tileable-terrain.json",
  "spec": "spec/terrain-grass-dirt.json",
  "spec_sha256": "…",
  "seed": 20261109,
  "candidate_index": 7,
  "pipeline_version": "artgen-0.1.0",
  "palette": "field-24",
  "licence": "outputs owned by plyEngine; base model OpenRAIL++-M",
  "human_fixup": ["cleaned 3 stray pixels at (12,4)", "redrew south edge"],
  "created": "2026-11-10"
}
```

`human_fixup` is not shameful bookkeeping — it is the record that a person judged
the asset, which is what separates a pipeline from a content farm. Manifests also
serve as the **download manifest** for first-run asset fetching
([ADR 0004](adr/0004-download-on-first-run-assets.md)): same file, so the
loader and the linter cannot disagree about what an asset is.

---

## 7. Per-asset-class recipes

| Class | Generate | Assemble | Special gates |
|---|---|---|---|
| **Terrain** | centre, edge, outer corner, inner corner per terrain, tileable prompt | blob-47 from components; **terrain pairs** assembled from two component sets to get cross-transitions | `blob47-complete`, `seam-continuity` |
| **Props** | single object, transparent background requested, then alpha-keyed | shadow generated procedurally (ellipse, never AI); pivot recorded at feet | `alpha-binary`, `pivot-shadow` |
| **Buildings** | base + roof as separate generations so roofs can occlude the player | two-region atlas entry + collision rect per floor-level | occlusion flag; collision fits footprint |
| **Characters (4-dir)** | ControlNet openpose 4×4 grid (down/up/left/right × 4 frames) + IP-Adapter for outfit identity + LoRA for style | sheet grid → per-clip regions; idle + walk minimum | palette, silhouette readability at 1× |
| **Animation** | per-tile strips (water 4 frames, torch 4) or skeleton-driven NPC animation | phase offsets per cell/tile recorded in the tileset | `grid`, animation frame count |
| **UI** | one panel, one bar, one slot, one tooltip per material; prompt for stretchable art | nine-slice with alpha-derived margins, then verified | `nine-slice` |
| **Icons** | batch of 16×16 icons against the UI palette | fixed grid, named regions | `palette`, `grid` |
| **Lighting/textures** | **never AI** — radial gradients, vignettes and noise generated by code | — | — |

---

## 8. Style lock

Style consistency is the thing that breaks first. Three cheap mechanisms, in
order of strength:

1. **Palette LUT** — one palette per environment family, quantised hard. This
   alone removes most drift between independently generated assets.
2. **LoRA trained on a curated reference sheet** — assemble ~20–40 approved
   tiles/props you like, train a small LoRA, and use it for every subsequent
   asset in that family. Training data must be **CC0 or fully owned**; the CC0
   packs in Appendix A are legitimate training material and are otherwise never
   shipped.
3. **Prompt + workflow pinning** — same workflow file, same negative prompt,
   same seed family per asset class, recorded in the spec.

---

## 9. Human-in-the-loop acceptance

A candidate is accepted only when: it lints clean, it reads correctly at 1× (not
just at 4×), its silhouette is distinguishable from its neighbours in a row of
five, and it sits next to the existing set without a palette jump. Fixes are done
in Aseprite and **recorded in `human_fixup`**. Expect 20–30 % of generated tiles
to need a fix pass; budget it per class instead of being surprised by it.

---

## 10. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Model licence changes or contradiction (model card vs licence) | Record licence per asset at generation time; forbidden-model list in CI (`spec` lint rejects FLUX dev) |
| Style drift across sessions | Palette LUT + LoRA + workflow pinning; a "style sheet" fixture asset renders beside every new asset in review |
| Seams that pass numerically but look wrong | Lint catches geometry; review catches aesthetics — both required |
| AI-mush at 1× | nearest-exact snap + palette quantise + 1× review |
| GPU contention on the dev host | Generation runs in a background queue, never inside a test; CI never generates |
| "Generated art" reception / store policy | See Appendix C — disclose, and keep human fixes recorded |
| Pipeline becomes the project | Time-box M1c art generation; if a class stalls, fall back per Appendix B and move on |

---

## Appendix A — CC0 / CC-BY reference material (not shipped)

Collected during research. Roles: style reference, LoRA training data (CC0 only),
geometry fixtures for the assembler and linter, and a fallback if the generated
pipeline stalls. **None of these ship in a build** under the current decision.

| Pack | Contents | Licence (verified on source page where readable) | Role |
|---|---|---|---|
| Overworld Autotiles (dandelion dino) | 16 px blob-47 terrain autotiles, Aseprite source | **CC0** — "Credit is not needed" | Assembler/lint fixture; deliberately hard for deterministic autotilers |
| Forge Starter Tilesets (deforestpeg) | `grass_dirt_16` blob-47 with all 47 cases + collision, `water_16` Wang-16, `props_32`, `dungeon_stone_16`, `.tsx` reference ordering | **CC0 1.0** | Mask-table reference; cave geometry fixture |
| Mini Meadow (myobln) | 75 tiles incl. **animated water (4 frames)**, autotile edges | **CC0** | Animation-strip reference |
| Kenney Roguelike/RPG pack | 1 700+ 16×16 tiles: terrain, buildings, interiors, items, UI panels | **CC0** | Breadth reference, palette study |
| Hearthmark RPG UI (Cole Foster) | 9-slice panels, bars, slots, minimap kit, 96 icons, pixel font, **`atlas.json` with margins + stretch modes** | **CC0 1.0** | The UI atlas schema shape in `UI_FRAMEWORK.md` follows this |
| 16x16 base sprites / Sprite Ninji / Bennyboi 4-directional (OpenGameArt) | 4-direction walk cycles | **CC0** | Character layout reference |
| The Village (bitsmall) | 300+ tiles, 4 autotile terrains, 75 building combinations | **CC BY 4.0** — attribution required | Style reference only (attribution if ever used) |
| LPC base assets | broad top-down set | **CC-BY-SA 3.0 + GPL 3.0** | **Avoid** — share-alike would infect the project's art |

OpenGameArt blocks automated fetching (HTTP 403), so licence lines for those
entries come from search-catalogue text rather than the page itself: **re-verify
and archive the licence file at download time** into `assets/licenses/`.

## Appendix B — hosted fallbacks

| Service | Strength | Cost | Licence note |
|---|---|---|---|
| Retro Diffusion | true grid-aligned pixel art, 84 styles, tilesets, animations, **MCP server** + REST | from $0.03/image | Outputs owned by the creator, commercial OK per FAQ/author; ToS needs JS — verify before shipping |
| PixelLab | 4-/8-direction character rotation, skeleton animation, Wang tilesets, **MCP** + REST | $12 → 2 000 gens | Commercial use listed on free and paid tiers; verify |
| SpriteLab "UI Kits" | prompt/mockup → sliced UI kit with 9-slice borders | per-kit credits | Verify |

Reachable through MCP, so a hosted fallback can be driven from the same agent
workflow with no bespoke glue — but it reintroduces a per-asset licence record,
which is exactly what the self-hosted path avoids.

## Appendix C — disclosure

Steam's Content Survey distinguishes AI **development tools** (no disclosure)
from AI-generated content **shipped and consumed** (art, sound, narrative,
localisation → disclose), and asks for guardrails if content is generated at
runtime. itch and Google Play have their own variants. If plyEngine's demo ships
generated art, that is a disclosure line at submission time — decide it here, in
the open, rather than at the store form.
