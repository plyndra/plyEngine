#pragma once

#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <vector>

namespace ply {

/// Minimap geometry (M1a a8, UI_FRAMEWORK.md §5): "render-target downscale +
/// marker transforms" — the world surface downscaled onto a minimap, and the
/// transform that puts world positions on it.
///
/// Coordinate model, from the two specs that define its ends: the world is
/// y-up (OrthoCamera.hpp) while minimap px are y-down screen pixels like the
/// rest of the UI (UI_FRAMEWORK.md §1), so every mapping here mirrors the y
/// axis. The world's px grid is [0, worldWidth-1] x [0, worldHeight-1] — the
/// a7 internal-resolution surface at 1 unit = 1 px (ADR 0012) — and it maps
/// onto the minimap grid [0, mapWidth-1] x [0, mapHeight-1] at scale 1.
///
/// Everything here is pure data: integer arithmetic only (i64 intermediates),
/// no GL, no window, no clock, no float anywhere in the path. A caller
/// holding float world positions floors them first (a7's snapCameraAxis — the
/// one sanctioned double); the float stops at that snap and never reaches
/// this code.
struct MinimapSpec {
    int worldWidth = 0, worldHeight = 0;
    int mapWidth = 0, mapHeight = 0;

    bool operator==(const MinimapSpec&) const = default;
};

/// One minimap pixel position, y-down.
struct MinimapPoint {
    int x = 0, y = 0;

    bool operator==(const MinimapPoint&) const = default;
};

/// One minimap marker: the world position it marks (world px, y-up) and the
/// marker art at scale 1 (minimap px).
struct MinimapMarker {
    int x = 0, y = 0;
    int artW = 1, artH = 1;

    bool operator==(const MinimapMarker&) const = default;
};

/// World (x, y) -> minimap px at integer `scale`, where each map px is
/// `scale` output px (the minimap's UI scale — integer 1/2/3 like every
/// widget, never fractional; a scale below 1 is treated as 1).
///
///   mx = floor(x * mapWidth  * scale / worldWidth)
///   my = mapHeight * scale - 1 - floor(y * mapHeight * scale / worldHeight)
///
/// Contract (UI_FRAMEWORK.md §2 rules 2-3):
///  - The scale multiplies BEFORE the division rounds — floor(x*k*s/W), never
///    floor(x*k/W)*s — the same rule as layoutWidgets' art scaling.
///  - The division floors toward negative infinity, not toward zero, so the
///    transform is monotone through the world origin: a position left of or
///    below the world edge maps left of / below the map, never onto its edge
///    px. Clamping into the map is the draw policy, not this function's.
///  - y mirrors, because the world is y-up and the minimap is y-down.
///  - Invariant (mapWidth*scale <= worldWidth on x, likewise y): the world
///    grid's corner px land exactly on the minimap grid's corner px, and the
///    grid's exclusive far corner (worldWidth, worldHeight) lands one px
///    outside the map — which is what turns an edge off-by-one into a pin
///    failure.
///  - A non-positive world extent maps that axis to 0 (there is no extent to
///    distribute); a non-positive map extent maps that axis to 0 (there is no
///    grid to land on). The other axis is unaffected.
MinimapPoint worldToMinimap(const MinimapSpec& spec, int worldX, int worldY, int scale);

/// The render-target downscale: a world rect -> the minimap rect covering
/// every world px of it — the mirror of a7's upscaleRect (which multiplies
/// every field; this maps each corner through worldToMinimap and takes the
/// covering rect). For a 1x1 world rect this is exactly the px
/// worldToMinimap returns, so there is one mapping, not two. A non-positive
/// extent yields extent 0 on that axis at the transformed origin; the other
/// axis still maps.
UIRect minimapRect(const MinimapSpec& spec, const UIRect& worldRect, int scale);

/// The marker transform at integer `scale`: the marker art multiplied by
/// `scale` (scaledSize — the scale multiplies the art before any rounding,
/// and non-positive art clamps to 1 so a marker always occupies at least one
/// px) and centred on worldToMinimap's px via floorDiv2's floor halving, so
/// an odd size resolves down deterministically with no half pixel.
UIRect minimapMarkerRect(const MinimapSpec& spec, const MinimapMarker& marker, int scale);

/// FNV-1a 64 over the marker list: count, then each marker as i32 x, y, artW,
/// artH. StructuralHasher with explicit little-endian integers, never
/// `std::hash` — stable across runs, processes, hosts and builds (ADR 0014).
std::uint64_t structuralHash(const std::vector<MinimapMarker>& markers);

}  // namespace ply
