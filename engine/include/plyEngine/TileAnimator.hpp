#pragma once

#include "plyEngine/TileSet.hpp"

namespace ply {

// TileAnimator (M1b b4, ARCHITECTURE_TARGET.md §3): "Per-tile frame sequences
// advanced from the sim clock; phase offset per cell so water doesn't pulse
// in lockstep". Pure data — no texture, no GL, no window, no clock OBJECT:
// the caller passes the sim step (the fixed-step counter — FixedTimestep.hpp
// — accumulated by the host), so the whole unit is gradable in ctest.
//
// Scope and conventions:
// - Animation data is b1's TileSet metadata: a tile's `animation` frames
//   (region + steps each, steps >= 1 at load) and its configured `phase`
//   offset in sim steps (ART_PIPELINE.md §7: "phase offsets per cell/tile
//   recorded in the tileset").
// - THE PER-CELL PHASE OFFSET is an explicit argument (cellPhase): each
//   cell animates on its own timeline, so water does not pulse in lockstep.
//   The query applies the tile's configured phase (phaseOf) automatically;
//   cellPhase adds whatever per-cell decorrelation the caller derives
//   (variant tiles of one variant group carrying different phases are the
//   authoring route; a caller-side coordinate offset is the runtime route).
// - The animation clock is `step + tile.phase + cellPhase`, reduced modulo
//   the cycle length (the sum of the frames' steps) into [0, cycle). The
//   frame index is the frame whose [start, start+steps) window holds the
//   reduced clock. All arithmetic is long long and reduction is exact for
//   any input — the b2 F1 lesson: a time value must not wrap at a reachable
//   value (pinned past 2^31).
// - A tile with no animation has exactly one frame: index 0, region = the
//   tile's own region (static art). An UNKNOWN tile is not animatable and
//   fails loudly: frame index -1, empty region — never a plausible frame
//   (b2's "unknown case errors" contract).
// - Frames with steps <= 0 cannot come from a loaded document
//   (NonPositiveSize at load); a hand-built TileSet that bypasses the
//   loader is defended by clamping such a frame to 1 step (pinned).

class TileAnimator {
public:
    /// Reads the animation strips and per-tile phase offsets from the
    /// tileset. Building cannot fail: unknown tiles and static art are
    /// query-time states, not build errors.
    static TileAnimator build(const TileSet& tileset);

    /// The tile's configured phase offset in sim steps (TileSet.hpp's
    /// `phase`); 0 for an unknown tile.
    int phaseOf(int tileId) const;

    /// The frame index a cell holding `tileId` shows at sim step `step`,
    /// with `cellPhase` as that cell's phase offset ON TOP of the tile's
    /// configured phase. -1 when the tile is unknown. `step` may be any
    /// long long (soaks included) and may be negative (reduced modulo the
    /// cycle like any other value).
    int frameIndexAt(int tileId, long long step, long long cellPhase) const;

    /// The art region of that frame — the animation frame's region, or the
    /// tile's own region for static art. Empty rect for an unknown tile.
    UIRect regionAt(int tileId, long long step, long long cellPhase) const;

    /// The cycle length in sim steps (sum of the frames' steps), or 0 for
    /// static or unknown tiles. Exposed so callers can align per-cell
    /// decorrelation to whole cycles and so the frame math is inspectable.
    long long cycleOf(int tileId) const;

private:
    TileSet tileset_{};
};

}  // namespace ply
