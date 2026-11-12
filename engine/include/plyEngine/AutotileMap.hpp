#pragma once

#include "plyEngine/AutoTileResolver.hpp"
#include "plyEngine/MapLoader.hpp"
#include "plyEngine/StructuralHash.hpp"
#include "plyEngine/TileSet.hpp"
#include "plyEngine/WorldMap.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace ply {

// AutotileMap (M1b b3d — the gate-gap pre-decision's glue, "Autotile
// submission: terrain layer -> resolved ids -> quads, per-terrain blob
// tables selected by variant group"). This is the M1b gate's resolve-at-load
// path: the map document stores TERRAIN FILLS; at load the engine resolves
// every cell through its terrain's blob table and submits RESOLVED quads.
// Pure data: no GL, no window, no clock, no float.
//
// The pre-decision (plyengine::m1b-gate-predecisions, 2026-11-12):
// - TERRAIN IDENTITY upgrades from b3c's tile-id equality to VARIANT-GROUP
//   equality: two fills are the same terrain when their tiles share a
//   variant group (TileSet.hpp: "interchangeable fills for one terrain");
//   a tile without a group is its own terrain (id equality).
// - ONE AutoTileResolver TABLE PER VARIANT GROUP, built at load from the
//   group's mask-role tiles. The table for a cell is selected by the
//   cell's variant group — a grass cell resolves against grass art, a
//   water cell against water art.
// - RESOLVE AT LOAD, not baked: the map file holds fills; the engine
//   resolves them (this module) and submits resolved quads. (Tables are
//   built eagerly at load and each query is a lazy canonicalise+lookup —
//   observably identical to an eagerly materialised field through the
//   query/submission API, the b3d read's O4. Runtime re-resolution on
//   edit arrives with the M2 editor.)
//
// Conventions carried unchanged from WorldMap/b3c: map-grid directions (N
// is the row above), out-of-map neighbours are not same terrain, the raw
// diagonal is truth (canonicalisation is the resolver's job), an empty
// centre has no mask, and the strict positive-area camera intersection
// with opaque coverage and pass order (below/above entities).
//
// Loud failure (b2's contract): a cell whose terrain has no table, or
// whose mask is absent from its table, resolves ok() == false with tileId
// -1 — it submits NOTHING (never a plausible tile) and says so at the
// query.

class AutotileMap {
public:
    /// Builds from a loaded map and its FULL tileset (variant groups + mask
    /// roles). Every variant group that has mask-role tiles becomes a
    /// resolver table; groups without one resolve loudly-failed.
    static AutotileMap build(const MapData& map, const TileSet& tileset);

    /// The terrain identity of a cell's tile: its variant group, or the
    /// tile id itself when the tile declares no group.
    std::string terrainKeyAt(std::size_t layer, int cx, int cy) const;

    /// The raw 8-neighbour same-terrain mask (b3c's conventions) under
    /// VARIANT-GROUP identity: the b3c query's upgrade.
    int terrainMaskAt(std::size_t layer, int cx, int cy) const;

    /// The resolved art tile for a cell: the terrain's table resolves the
    /// mask. ok() == false (tileId -1, the canonical mask named) when the
    /// terrain has no table or the mask is absent — loud, never a fallback.
    TileQuery resolvedAt(std::size_t layer, int cx, int cy) const;

    /// The RESOLVED quads for a camera rect: WorldMap::submit's exact-count
    /// rules (strict positive-area intersection, opaque coverage, pass
    /// order and submission order) with each quad's tileId replaced by its
    /// resolved art id. Cells that fail to resolve submit nothing.
    WorldMapSubmission submitResolved(const UIRect& camera);

    /// The ADR-0014 structural hash of a layer's resolved-id field
    /// (row-major, the full map) — the no-seam measurement.
    std::uint64_t resolvedFieldHash(std::size_t layer);

private:
    WorldMap world_;
    std::unordered_map<std::string, AutoTileResolver> tables_;
    std::vector<std::vector<std::string>> terrainKeys_;  // [layer][cell]
};

}  // namespace ply
