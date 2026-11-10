# tools/artgen — self-hosted art pipeline

Pipeline stages (see [`docs/planning/ART_PIPELINE.md`](../../docs/planning/ART_PIPELINE.md)):

```
spec/       one JSON per asset class: prompt, model, LoRA, seed, palette, geometry
workflow/   pinned ComfyUI graphs
gen/        submit a spec, store raw candidates + seed (output → out/, git-ignored)
post/       palette LUT, alpha key, nearest-exact snap
assemble/   blob47.py, sheets.py, nineslice.py, atlas.py   (deterministic, no AI)
lint/       the CI gates
emit/       assets/**/*.png + assets/manifests/*.json
```

Nothing here runs in CI except `assemble/` and `lint/`. Generation never runs in
a test.

## Host state (measured 2026-11-09, PVE3)

| Thing | State | How it got there |
|---|---|---|
| Python 3 + pip | present | distro |
| **Pillow 13.0.0** | installed **system-wide** via `pip3 install --break-system-packages pillow` | needed to measure the reference frames; declared in `requirements.txt` so it is reproducible rather than folklore |
| ffmpeg 7.1.5 | present | `apt-get install --no-install-recommends ffmpeg`, for the hero capture |
| ComfyUI | **not installed** | per ADR 0010, installs at M1c start, pinned, own venv, `/opt/comfyui`, systemd unit |
| Aseprite | **not installed** (paid, optional) | palette work uses our own LUT pass |
| GPU | RTX 3050 8 GB | SDXL at 1024² fp16 expected to fit; throughput unmeasured |

## Install

```sh
python3 -m pip install -r tools/artgen/requirements.txt   # or a venv, preferred
```

Prefer a venv over `--break-system-packages` for anything new; the system-wide
Pillow is recorded here because it already exists, not as an endorsement.

## Rules

- Every spec pins model + LoRA + workflow + seed; `candidates >= 8`.
- Provenance is emitted with every asset (provider, model hash, seed, spec hash,
  asset sha256, licence, human fixups) — see `docs/planning/ART_PIPELINE.md` §6.
- Generated candidates stay out of git (`out/` is ignored); only linted,
  accepted assets and their manifests land under `assets/`.
