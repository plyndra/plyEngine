#pragma once

#include "plyEngine/UIAtlas.hpp"

#include <cstdint>
#include <vector>

namespace ply {

/// One UI quad. `x`/`y` are target pixels and `u`/`v` are source pixels, both
/// **pre-scale**: the renderer applies the integer scale, so geometry carries no
/// float and the structural hash never depends on floating-point arithmetic.
struct UIVertex {
    int x = 0, y = 0;
    int u = 0, v = 0;

    bool operator==(const UIVertex&) const = default;
};

/// Corners in winding order: top-left, top-right, bottom-right, bottom-left.
struct UIQuad {
    UIVertex topLeft, topRight, bottomRight, bottomLeft;

    bool operator==(const UIQuad&) const = default;
};

/// Emits the quads that draw `region` into a `targetWidth` x `targetHeight` rect
/// measured in source pixels.
///
/// Contract:
///  - Corners keep their source size; edges and centre fill the space between
///    them, stretched or tiled per the region's modes.
///  - A tiled run emits whole tiles from the region's start corner and clips the
///    final tile at the target edge. Target and source are clipped by the same
///    amount, so tiled art stays 1:1 and never squashes at the far edge.
///  - A target too small to hold both caps plus one pixel of centre compresses
///    the caps by a common factor instead of inverting them; the leftover pixel
///    goes to the second cap.
///  - `Plain`, or a region with no margins, is one stretched quad.
///  - Quad order is part of the contract, because it is part of the hash:
///    top-left, top-right, bottom-left, bottom-right corner; top edge, bottom
///    edge, left edge, right edge; centre last. Tiled runs ascend from the
///    region's start.
///  - A non-positive target emits nothing.
std::vector<UIQuad> nineSliceGeometry(const UIAtlasRegion& region, int targetWidth,
                                      int targetHeight);

/// FNV-1a 64 over the quad list: quad count, then each quad's four vertices in
/// contract order, each vertex as i32 x, y, u, v. Integer-only by construction.
std::uint64_t structuralHash(const std::vector<UIQuad>& quads);

}  // namespace ply
