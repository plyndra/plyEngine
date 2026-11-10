#!/usr/bin/env bash
# Capture the hero animation from the scripted four-slot companion run.
#
# The run is `ply_companion --verify`: the Autopilot pushes controller states
# through the *same* CompanionInput queue a phone writes to, so the frames show
# the real slot-routing path. It is a scripted capture, not four physical
# phones, and the HUD says so on frame 1 — never publish it unlabelled.
#
# Determinism: the demo advances on a fixed timestep and the autopilot is
# scripted, so the same build and stride produce the same frames.
#
# Outputs (docs/announcements/):
#   <prefix>.gif  — README hero, <=8 MB, 960 px wide
#   <prefix>.mp4  — full resolution, for Discord/where GIFs are penalised
#
# Usage: scripts/capture-hero.sh [output-prefix]
#   requires: xvfb-run, ffmpeg, a built ./build/ply_companion, and the PWA in
#   companion/dist (cd companion && npm ci && npm run build).

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."

prefix="${1:-docs/announcements/hero-capture}"
frames_dir="$(mktemp -d)"
trap 'rm -rf "$frames_dir"' EXIT

bin=./build/ply_companion
[ -x "$bin" ] || { echo "missing $bin (configure and build first)" >&2; exit 1; }
[ -f companion/dist/index.html ] || { echo "missing companion/dist (npm run build in companion/)" >&2; exit 1; }
command -v ffmpeg >/dev/null || { echo "ffmpeg not installed" >&2; exit 1; }

echo "capturing frames -> $frames_dir"
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a "$bin" --verify --no-mdns --web companion/dist \
    --dump-frames "$frames_dir" --dump-stride 1 | grep -E 'capture:|verify:'

count=$(find "$frames_dir" -name 'frame_*.ppm' | wc -l)
[ "$count" -gt 0 ] || { echo "no frames captured" >&2; exit 1; }
echo "frames: $count"

# 20 fps: every frame of the scripted match (~380 of them) over ~19 s. The sim
# runs at 60 Hz, so 20 fps drops two of every three frames rather than
# introducing motion the capture never had. Two-pass palette, no dithering
# artefacts on the flat tile art.
ffmpeg -y -hide_banner -loglevel error -framerate 20 -i "$frames_dir/frame_%05d.ppm" \
    -vf "scale=960:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=192:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle" \
    -loop 0 "${prefix}.gif"

ffmpeg -y -hide_banner -loglevel error -framerate 20 -i "$frames_dir/frame_%05d.ppm" \
    -c:v libx264 -pix_fmt yuv420p -crf 20 -movflags +faststart "${prefix}.mp4"

ls -la "${prefix}.gif" "${prefix}.mp4"
echo
echo "Reminder: the animation is a scripted capture; label it wherever it is published."
