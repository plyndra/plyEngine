#include "plyEngine/Minimap.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <cstdint>

namespace ply {

namespace {

/// Floor division by a positive divisor: rounds toward negative infinity,
/// unlike C++'s truncation. Every division in the minimap path goes through
/// this one function, so the floor-not-trunc rule has a single site (the same
/// discipline as a4's floorDiv2 — this one is the ratio form).
std::int64_t floorDiv(std::int64_t numerator, std::int64_t denominator) {
    std::int64_t q = numerator / denominator;
    if (numerator % denominator != 0 && numerator < 0) --q;
    return q;
}

}  // namespace

MinimapPoint worldToMinimap(const MinimapSpec& spec, int worldX, int worldY, int scale) {
    const int s = scale < 1 ? 1 : scale;
    MinimapPoint p;
    // Per axis: no world extent to distribute, or no map grid to land on,
    // maps that axis to 0. The other axis is unaffected.
    if (spec.worldWidth > 0 && spec.mapWidth > 0) {
        p.x = static_cast<int>(floorDiv(static_cast<std::int64_t>(worldX) * spec.mapWidth * s,
                                        spec.worldWidth));
    }
    if (spec.worldHeight > 0 && spec.mapHeight > 0) {
        // y mirrors: the world is y-up, minimap px are y-down. The scale
        // multiplies before the division rounds.
        p.y = spec.mapHeight * s - 1 -
              static_cast<int>(floorDiv(static_cast<std::int64_t>(worldY) * spec.mapHeight * s,
                                        spec.worldHeight));
    }
    return p;
}

UIRect minimapRect(const MinimapSpec& spec, const UIRect& worldRect, int scale) {
    // The covering rect of every world px in the input: the transform is
    // monotone on x and anti-monotone on y, so the extremes are the corners.
    const MinimapPoint origin = worldToMinimap(spec, worldRect.x, worldRect.y, scale);
    UIRect out{origin.x, origin.y, 0, 0};
    if (worldRect.w > 0) {
        const MinimapPoint far =
            worldToMinimap(spec, worldRect.x + worldRect.w - 1, worldRect.y, scale);
        out.w = far.x - origin.x + 1;
    }
    if (worldRect.h > 0) {
        const MinimapPoint top =
            worldToMinimap(spec, worldRect.x, worldRect.y + worldRect.h - 1, scale);
        out.y = top.y;
        out.h = origin.y - top.y + 1;
    }
    return out;
}

UIRect minimapMarkerRect(const MinimapSpec& spec, const MinimapMarker& marker, int scale) {
    // The art scales before the centring halving rounds (scaledSize is a4's
    // rule); non-positive art clamps to 1 so a marker always shows.
    const int w = scaledSize(marker.artW < 1 ? 1 : marker.artW, scale);
    const int h = scaledSize(marker.artH < 1 ? 1 : marker.artH, scale);
    const MinimapPoint p = worldToMinimap(spec, marker.x, marker.y, scale);
    return UIRect{p.x - floorDiv2(w), p.y - floorDiv2(h), w, h};
}

std::uint64_t structuralHash(const std::vector<MinimapMarker>& markers) {
    StructuralHasher hasher;
    hasher.u64(markers.size());
    for (const MinimapMarker& marker : markers) {
        hasher.i32(marker.x);
        hasher.i32(marker.y);
        hasher.i32(marker.artW);
        hasher.i32(marker.artH);
    }
    return hasher.value();
}

}  // namespace ply
