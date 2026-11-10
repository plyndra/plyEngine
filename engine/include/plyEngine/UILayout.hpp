#pragma once

#include <cstdint>
#include <vector>

namespace ply {

/// Where a widget attaches to the logical viewport.
///
/// Named exactly as UI_FRAMEWORK.md §2 spells them, so a spec lookup is a
/// token match: TopLeft | TopCentre | TopRight | MidLeft | Centre | MidRight
/// | BottomLeft | BottomCentre | BottomRight.
enum class UIAnchor {
    TopLeft,
    TopCentre,
    TopRight,
    MidLeft,
    Centre,
    MidRight,
    BottomLeft,
    BottomCentre,
    BottomRight,
};

/// One laid-out widget rect, in logical pixels, post-snap. Everything is
/// `int`: layout never produces a fraction that survived to the caller.
struct UIRect {
    int x = 0, y = 0, w = 0, h = 0;

    bool operator==(const UIRect&) const = default;
};

/// One widget's layout inputs. `width`/`height` are the widget's size in
/// source-art pixels, pre-scale; `scale` is the integer UI factor (1..3) that
/// multiplies the art *before* layout rounding. The widget's children/tree
/// layout lands with the a5 widgets; a4 lays flat lists.
struct UIWidgetSpec {
    UIAnchor anchor = UIAnchor::TopLeft;
    /// Offset from the anchored edge, in logical px, pointing into the
    /// viewport.
    int offsetX = 0, offsetY = 0;
    int width = 0, height = 0;
    int scale = 1;

    bool operator==(const UIWidgetSpec&) const = default;
};

/// Lays out `widgets` against a `logicalWidth` x `logicalHeight` viewport.
///
/// Contract (UI_FRAMEWORK.md §2):
///  - Anchor + offset, never absolute positions. Per axis the anchor picks the
///    edge the offset measures from: near anchors (Left/Top) put the rect's
///    near edge `offset` right of / below the viewport's near edge; far anchors
///    (Right/Bottom) put the rect's far edge `offset` left of / above the
///    viewport's far edge; centre anchors put the rect's centre `offset` past
///    the viewport's centre line.
///  - Integer scale only: the scaled size is `width * scale`, computed before
///    any rounding. A scale below 1 is treated as 1 (named behaviour, tested).
///  - Pixel snapping: the only division in the path is the centre-line halving,
///    and it floors (rounds toward negative infinity), so a half-pixel resolves
///    deterministically and never through host floating point. Every emitted
///    coordinate is an integer by construction.
///  - Pure function of (widget list, logical size) → rect list. No clock, no
///    GL, no animation state, no float anywhere in the path.
std::vector<UIRect> layoutWidgets(const std::vector<UIWidgetSpec>& widgets, int logicalWidth,
                                  int logicalHeight);

/// FNV-1a 64 over the rect list: count, then each rect as i32 x, y, w, h.
/// Same discipline as the quad hash in NineSlice — explicit little-endian
/// integers, never `std::hash` — so the value is stable across runs,
/// processes, hosts and builds (E-a4.3).
std::uint64_t structuralHash(const std::vector<UIRect>& rects);

/// Dirty-layout wrapper (UI_FRAMEWORK.md §2 rule 4): recomputes only when the
/// widget list or logical size changed since the last call, else returns the
/// cached rect list. Inputs are compared by value with `==` — never by hash —
/// so a changed tree cannot collide into a stale result.
class UILayoutCache {
public:
    /// Cached rect list for `widgets` at `logicalWidth` x `logicalHeight`,
    /// recomputed when either input changed.
    const std::vector<UIRect>& rects(const std::vector<UIWidgetSpec>& widgets, int logicalWidth,
                                     int logicalHeight);

    /// Forgets the cached entry. The next rects() call recomputes.
    void clear() { hasEntry_ = false; }

    /// How many times a layout has been computed so far. This is how "reuse,
    /// don't recompute" is observable: repeat calls with unchanged inputs must
    /// not move it.
    int recomputes() const { return recomputes_; }

private:
    std::vector<UIWidgetSpec> widgets_;
    int logicalWidth_ = 0;
    int logicalHeight_ = 0;
    bool hasEntry_ = false;
    int recomputes_ = 0;
    std::vector<UIRect> rects_;
};

}  // namespace ply
