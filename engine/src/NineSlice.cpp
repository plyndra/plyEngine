#include "plyEngine/NineSlice.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <algorithm>

namespace ply {
namespace {

struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
};

UIQuad makeQuad(Rect target, Rect source) {
    return UIQuad{
        UIVertex{target.x, target.y, source.x, source.y},
        UIVertex{target.x + target.w, target.y, source.x + source.w, source.y},
        UIVertex{target.x + target.w, target.y + target.h, source.x + source.w,
                 source.y + source.h},
        UIVertex{target.x, target.y + target.h, source.x, source.y + source.h},
    };
}

struct Segment {
    int at = 0;   // offset from the run's start, in source pixels
    int len = 0;  // source pixels; target and source are clipped identically
};

/// One segment per stretch; whole tiles plus a clipped final tile for tile mode.
std::vector<Segment> axisSegments(int length, int tile, StretchMode mode) {
    std::vector<Segment> out;
    if (length <= 0) return out;
    if (mode != StretchMode::Tile || tile <= 0) {
        out.push_back(Segment{0, length});
        return out;
    }
    for (int at = 0; at < length; at += tile) {
        out.push_back(Segment{at, std::min(tile, length - at)});
    }
    return out;
}

/// Caps compress by a common factor when the target cannot hold both plus one
/// pixel of centre; the leftover pixel goes to the second cap. Never inverted.
void fitCaps(int target, int& first, int& second) {
    first = std::max(0, first);
    second = std::max(0, second);
    if (first + second < target) return;  // fits, with at least 1 px of centre
    if (target <= 1) {
        first = 0;
        second = 0;
        return;
    }
    const int shared = target - 1;
    if (first + second <= 0) {
        first = 0;
        second = 0;
        return;
    }
    first = static_cast<int>((static_cast<long long>(first) * shared) / (first + second));
    second = shared - first;
}

void emitNine(const UIAtlasRegion& region, int targetWidth, int targetHeight,
              std::vector<UIQuad>& out) {
    const UIMargins m = region.margins;
    int left = m.left, right = m.right, top = m.top, bottom = m.bottom;
    fitCaps(targetWidth, left, right);
    fitCaps(targetHeight, top, bottom);

    const int midW = targetWidth - left - right;   // >= 1 by fitCaps
    const int midH = targetHeight - top - bottom;  // >= 1 by fitCaps
    const int srcMidW = std::max(0, region.w - m.left - m.right);
    const int srcMidH = std::max(0, region.h - m.top - m.bottom);
    const int srcMidX = region.x + m.left;
    const int srcMidY = region.y + m.top;

    // Corners: source size fixed (the full source corner, compressed if needed).
    if (left > 0 && top > 0)
        out.push_back(makeQuad(Rect{0, 0, left, top}, Rect{region.x, region.y, m.left, m.top}));
    if (right > 0 && top > 0)
        out.push_back(makeQuad(Rect{targetWidth - right, 0, right, top},
                               Rect{region.x + region.w - m.right, region.y, m.right, m.top}));
    if (left > 0 && bottom > 0)
        out.push_back(makeQuad(Rect{0, targetHeight - bottom, left, bottom},
                               Rect{region.x, region.y + region.h - m.bottom, m.left, m.bottom}));
    if (right > 0 && bottom > 0)
        out.push_back(makeQuad(Rect{targetWidth - right, targetHeight - bottom, right, bottom},
                               Rect{region.x + region.w - m.right,
                                    region.y + region.h - m.bottom, m.right, m.bottom}));

    const bool tileX = region.edges == StretchMode::Tile;
    const bool tileY = region.edges == StretchMode::Tile;

    // Horizontal edges.
    if (top > 0 && midW > 0) {
        for (const Segment& seg : axisSegments(midW, srcMidW, region.edges)) {
            const int srcX = tileX ? srcMidX + (seg.at % std::max(1, srcMidW)) : srcMidX;
            const int srcW = tileX ? seg.len : srcMidW;
            out.push_back(makeQuad(Rect{left + seg.at, 0, seg.len, top},
                                   Rect{srcX, region.y, srcW, m.top}));
        }
    }
    if (bottom > 0 && midW > 0) {
        for (const Segment& seg : axisSegments(midW, srcMidW, region.edges)) {
            const int srcX = tileX ? srcMidX + (seg.at % std::max(1, srcMidW)) : srcMidX;
            const int srcW = tileX ? seg.len : srcMidW;
            out.push_back(makeQuad(Rect{left + seg.at, targetHeight - bottom, seg.len, bottom},
                                   Rect{srcX, region.y + region.h - m.bottom, srcW, m.bottom}));
        }
    }

    // Vertical edges.
    if (left > 0 && midH > 0) {
        for (const Segment& seg : axisSegments(midH, srcMidH, region.edges)) {
            const int srcY = tileY ? srcMidY + (seg.at % std::max(1, srcMidH)) : srcMidY;
            const int srcH = tileY ? seg.len : srcMidH;
            out.push_back(makeQuad(Rect{0, top + seg.at, left, seg.len},
                                   Rect{region.x, srcY, m.left, srcH}));
        }
    }
    if (right > 0 && midH > 0) {
        for (const Segment& seg : axisSegments(midH, srcMidH, region.edges)) {
            const int srcY = tileY ? srcMidY + (seg.at % std::max(1, srcMidH)) : srcMidY;
            const int srcH = tileY ? seg.len : srcMidH;
            out.push_back(
                makeQuad(Rect{targetWidth - right, top + seg.at, right, seg.len},
                         Rect{region.x + region.w - m.right, srcY, m.right, srcH}));
        }
    }

    // Centre: fills in both axes.
    if (midW > 0 && midH > 0) {
        for (const Segment& sy : axisSegments(midH, srcMidH, region.centre)) {
            for (const Segment& sx : axisSegments(midW, srcMidW, region.centre)) {
                const bool tiledX = region.centre == StretchMode::Tile;
                const bool tiledY = region.centre == StretchMode::Tile;
                const int srcX = tiledX ? srcMidX + (sx.at % std::max(1, srcMidW)) : srcMidX;
                const int srcY = tiledY ? srcMidY + (sy.at % std::max(1, srcMidH)) : srcMidY;
                const int srcW = tiledX ? sx.len : srcMidW;
                const int srcH = tiledY ? sy.len : srcMidH;
                out.push_back(makeQuad(Rect{left + sx.at, top + sy.at, sx.len, sy.len},
                                       Rect{srcX, srcY, srcW, srcH}));
            }
        }
    }
}

void emitThreeH(const UIAtlasRegion& region, int targetWidth, int targetHeight,
                std::vector<UIQuad>& out) {
    const UIMargins m = region.margins;
    int left = m.left, right = m.right;
    fitCaps(targetWidth, left, right);
    const int midW = targetWidth - left - right;
    const int srcMidW = std::max(0, region.w - m.left - m.right);
    const int srcMidX = region.x + m.left;

    if (left > 0)
        out.push_back(makeQuad(Rect{0, 0, left, targetHeight},
                               Rect{region.x, region.y, m.left, region.h}));
    for (const Segment& seg : axisSegments(midW, srcMidW, region.centre)) {
        const bool tiled = region.centre == StretchMode::Tile;
        const int srcX = tiled ? srcMidX + (seg.at % std::max(1, srcMidW)) : srcMidX;
        const int srcW = tiled ? seg.len : srcMidW;
        out.push_back(makeQuad(Rect{left + seg.at, 0, seg.len, targetHeight},
                               Rect{srcX, region.y, srcW, region.h}));
    }
    if (right > 0)
        out.push_back(makeQuad(Rect{targetWidth - right, 0, right, targetHeight},
                               Rect{region.x + region.w - m.right, region.y, m.right, region.h}));
}

void emitThreeV(const UIAtlasRegion& region, int targetWidth, int targetHeight,
                std::vector<UIQuad>& out) {
    const UIMargins m = region.margins;
    int top = m.top, bottom = m.bottom;
    fitCaps(targetHeight, top, bottom);
    const int midH = targetHeight - top - bottom;
    const int srcMidH = std::max(0, region.h - m.top - m.bottom);
    const int srcMidY = region.y + m.top;

    if (top > 0)
        out.push_back(makeQuad(Rect{0, 0, targetWidth, top},
                               Rect{region.x, region.y, region.w, m.top}));
    for (const Segment& seg : axisSegments(midH, srcMidH, region.centre)) {
        const bool tiled = region.centre == StretchMode::Tile;
        const int srcY = tiled ? srcMidY + (seg.at % std::max(1, srcMidH)) : srcMidY;
        const int srcH = tiled ? seg.len : srcMidH;
        out.push_back(makeQuad(Rect{0, top + seg.at, targetWidth, seg.len},
                               Rect{region.x, srcY, region.w, srcH}));
    }
    if (bottom > 0)
        out.push_back(makeQuad(Rect{0, targetHeight - bottom, targetWidth, bottom},
                               Rect{region.x, region.y + region.h - m.bottom, region.w,
                                    m.bottom}));
}

}  // namespace

std::vector<UIQuad> nineSliceGeometry(const UIAtlasRegion& region, int targetWidth,
                                      int targetHeight) {
    std::vector<UIQuad> out;
    if (targetWidth <= 0 || targetHeight <= 0) return out;

    if (!region.hasMargins || region.slice == SliceMode::Plain) {
        out.push_back(makeQuad(Rect{0, 0, targetWidth, targetHeight},
                               Rect{region.x, region.y, region.w, region.h}));
        return out;
    }

    switch (region.slice) {
        case SliceMode::Nine: emitNine(region, targetWidth, targetHeight, out); break;
        case SliceMode::ThreeH: emitThreeH(region, targetWidth, targetHeight, out); break;
        case SliceMode::ThreeV: emitThreeV(region, targetWidth, targetHeight, out); break;
        case SliceMode::Plain:
            out.push_back(makeQuad(Rect{0, 0, targetWidth, targetHeight},
                                   Rect{region.x, region.y, region.w, region.h}));
            break;
    }
    return out;
}

std::uint64_t structuralHash(const std::vector<UIQuad>& quads) {
    StructuralHasher hasher;
    hasher.u64(quads.size());
    for (const UIQuad& quad : quads) {
        for (const UIVertex* vertex :
             {&quad.topLeft, &quad.topRight, &quad.bottomRight, &quad.bottomLeft}) {
            hasher.i32(vertex->x);
            hasher.i32(vertex->y);
            hasher.i32(vertex->u);
            hasher.i32(vertex->v);
        }
    }
    return hasher.value();
}

}  // namespace ply
