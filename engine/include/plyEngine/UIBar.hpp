#pragma once

#include "plyEngine/UILayout.hpp"

#include <vector>

namespace ply {

/// Frame cap insets for a Bar, in **screen** pixels (post-scale): what a
/// nine-slice frame's corners occupy inside the laid-out rect. A frame whose
/// atlas region declares margins m at UI scale s has caps m*s.
struct UIBarCaps {
    int left = 0, top = 0, right = 0, bottom = 0;

    bool operator==(const UIBarCaps&) const = default;
};

/// Fill geometry for a `Bar` (M1a a5.1): value 0..1 as **per mille** (0..1000),
/// so the fill width is pure integer math and hashes exactly — no float enters
/// the layout or the structural hash. A caller holding a float fraction
/// converts to per mille at the API edge (round-half-even or truncation is the
/// caller's declared choice; this layer never sees the float).
///
/// Contract (UI_FRAMEWORK.md §5, Bar):
///  - The fill starts at the frame's left cap and runs left-to-right; its
///    width is floor(innerWidth * value / 1000), clamped so it never exceeds
///    the inner track and is 0 for value 0.
///  - Values below 0 or above 1000 are clamped to 0 / 1000 (named behaviour,
///    tested): value 1 clamps to a full track, not an overflow.
///  - A degenerate inner track (caps cover the whole frame) yields a fill of
///    width 0, never a negative one.
///  - The gradient is drawn content — colours, not geometry; layout only
///    decides the rect the gradient fills, and animation never affects it.
UIRect barFillRect(const UIRect& frame, const UIBarCaps& caps, int valuePermille);

/// Segment-tick geometry: `tickCount` segments produce tickCount-1 interior
/// boundaries — 1 px wide, full inner height — at floor(innerWidth * k /
/// tickCount). tickCount <= 1 emits nothing. Ticks span the inner track
/// regardless of the current fill, so segment boundaries stay put as the value
/// moves (pure function of the frame, not of the value).
std::vector<UIRect> barTickRects(const UIRect& frame, const UIBarCaps& caps, int tickCount);

}  // namespace ply
