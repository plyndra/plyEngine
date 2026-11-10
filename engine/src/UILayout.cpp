#include "plyEngine/UILayout.hpp"

#include "plyEngine/StructuralHash.hpp"

namespace ply {

namespace {

/// Horizontal component of an anchor: which viewport edge the offset measures
/// from, and on which side of the rect that edge attaches.
enum class Horiz { Left, Centre, Right };

/// Vertical component of an anchor.
enum class Vert { Top, Middle, Bottom };

Horiz horiz(UIAnchor anchor) {
    switch (anchor) {
        case UIAnchor::TopLeft:
        case UIAnchor::MidLeft:
        case UIAnchor::BottomLeft: return Horiz::Left;
        case UIAnchor::TopCentre:
        case UIAnchor::Centre:
        case UIAnchor::BottomCentre: return Horiz::Centre;
        case UIAnchor::TopRight:
        case UIAnchor::MidRight:
        case UIAnchor::BottomRight: return Horiz::Right;
    }
    return Horiz::Left;  // unreachable; every enumerator is handled
}

Vert vert(UIAnchor anchor) {
    switch (anchor) {
        case UIAnchor::TopLeft:
        case UIAnchor::TopCentre:
        case UIAnchor::TopRight: return Vert::Top;
        case UIAnchor::MidLeft:
        case UIAnchor::Centre:
        case UIAnchor::MidRight: return Vert::Middle;
        case UIAnchor::BottomLeft:
        case UIAnchor::BottomCentre:
        case UIAnchor::BottomRight: return Vert::Bottom;
    }
    return Vert::Top;  // unreachable; every enumerator is handled
}

}  // namespace

/// Floor division: rounds toward negative infinity, unlike C++'s truncation.
/// This *is* the snapping step — a half-pixel from the centre-line division
/// resolves down, deterministically, with no floating point involved.
/// Exported (additively) for the minimap path (M1a a8): the marker centring
/// is the same halving rule, so it calls this rather than re-deriving it.
int floorDiv2(int numerator) {
    int q = numerator / 2;
    if (numerator % 2 != 0 && numerator < 0) --q;
    return q;
}

/// Scaled size: the integer scale multiplies the source art before any layout
/// arithmetic. A scale below 1 is treated as 1 (named behaviour, tested).
/// Exported (additively) for the minimap marker transform (M1a a8).
int scaledSize(int artSize, int scale) {
    const int s = scale < 1 ? 1 : scale;
    return artSize * s;
}

std::vector<UIRect> layoutWidgets(const std::vector<UIWidgetSpec>& widgets, int logicalWidth,
                                  int logicalHeight) {
    std::vector<UIRect> out;
    out.reserve(widgets.size());
    for (const UIWidgetSpec& widget : widgets) {
        const int sw = scaledSize(widget.width, widget.scale);
        const int sh = scaledSize(widget.height, widget.scale);

        int x = 0;
        switch (horiz(widget.anchor)) {
            case Horiz::Left: x = widget.offsetX; break;
            case Horiz::Centre: x = floorDiv2(2 * widget.offsetX + logicalWidth - sw); break;
            case Horiz::Right: x = logicalWidth - widget.offsetX - sw; break;
        }

        int y = 0;
        switch (vert(widget.anchor)) {
            case Vert::Top: y = widget.offsetY; break;
            case Vert::Middle: y = floorDiv2(2 * widget.offsetY + logicalHeight - sh); break;
            case Vert::Bottom: y = logicalHeight - widget.offsetY - sh; break;
        }

        out.push_back(UIRect{x, y, sw, sh});
    }
    return out;
}

std::uint64_t structuralHash(const std::vector<UIRect>& rects) {
    StructuralHasher hasher;
    hasher.u64(rects.size());
    for (const UIRect& rect : rects) {
        hasher.i32(rect.x);
        hasher.i32(rect.y);
        hasher.i32(rect.w);
        hasher.i32(rect.h);
    }
    return hasher.value();
}

const std::vector<UIRect>& UILayoutCache::rects(const std::vector<UIWidgetSpec>& widgets,
                                                int logicalWidth, int logicalHeight) {
    if (hasEntry_ && logicalWidth == logicalWidth_ && logicalHeight == logicalHeight_ &&
        widgets == widgets_) {
        return rects_;
    }
    rects_ = layoutWidgets(widgets, logicalWidth, logicalHeight);
    widgets_ = widgets;
    logicalWidth_ = logicalWidth;
    logicalHeight_ = logicalHeight;
    hasEntry_ = true;
    ++recomputes_;
    return rects_;
}

}  // namespace ply
