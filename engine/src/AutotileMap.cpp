// AutotileMap (M1b b3d): terrain fills resolve at load through per-variant-
// group blob tables and submit RESOLVED quads. The contract — variant-group
// identity, per-group table selection, loud absent tables — is documented in
// AutotileMap.hpp (the gate-gap pre-decision); this file is that contract.
#include "plyEngine/AutotileMap.hpp"

#include <algorithm>

namespace ply {

namespace {
/// The terrain identity: the tile's variant group, or its own id when the
/// tile declares no group (the b3c rule's upgrade path).
std::string terrainKey(const TileSet& tileset, int id) {
    const TileData* tile = tileset.findTile(id);
    if (tile == nullptr) return {};
    return tile->variant.empty() ? "id:" + std::to_string(id) : tile->variant;
}
}  // namespace

AutotileMap AutotileMap::build(const MapData& map, const TileSet& tileset) {
    AutotileMap result;

    // The chunked storage behind every query (the b3b/b3c path): derive the
    // a9 slice from the full tileset.
    TilesetSlice slice;
    slice.name = tileset.name;
    slice.image = tileset.image;
    slice.tileWidth = tileset.tileWidth;
    slice.tileHeight = tileset.tileHeight;
    for (const TileData& tile : tileset.tiles) {
        TileDef def;
        def.id = tile.id;
        def.region = tile.region;
        def.solid = tile.solid;
        slice.tiles.push_back(def);
    }
    result.world_ = WorldMap::build(map, slice);

    // One resolver table PER VARIANT GROUP at load (the pre-decision): the
    // group's mask-role tiles only.
    std::unordered_map<std::string, TileSet> groups;
    for (const TileData& tile : tileset.tiles) {
        const std::string key = terrainKey(tileset, tile.id);
        TileSet& group = groups[key];
        group.name = key;
        group.tileWidth = tileset.tileWidth;
        group.tileHeight = tileset.tileHeight;
        if (tile.mask != kNoTileMask) group.tiles.push_back(tile);
    }
    for (const auto& [key, group] : groups) {
        if (!group.tiles.empty()) {
            result.tables_.emplace(key, AutoTileResolver::build(group));
        }
    }

    // The per-cell terrain keys (the identity lookup is precomputed — the
    // resolve-at-load path does no string work per query beyond the key).
    result.terrainKeys_.resize(map.layers.size());
    for (std::size_t l = 0; l < map.layers.size(); ++l) {
        result.terrainKeys_[l].assign(
            static_cast<std::size_t>(std::max(map.width, 0)) *
                static_cast<std::size_t>(std::max(map.height, 0)),
            std::string{});
        for (int y = 0; y < map.height; ++y) {
            for (int x = 0; x < map.width; ++x) {
                const std::size_t index =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(map.width) +
                    static_cast<std::size_t>(x);
                if (index < map.layers[l].tiles.size()) {
                    result.terrainKeys_[l][index] =
                        terrainKey(tileset, map.layers[l].tiles[index]);
                }
            }
        }
    }
    return result;
}

std::string AutotileMap::terrainKeyAt(std::size_t layer, int cx, int cy) const {
    if (layer >= terrainKeys_.size() || cx < 0 || cy < 0 || cx >= world_.width() ||
        cy >= world_.height()) {
        return {};
    }
    const std::size_t index =
        static_cast<std::size_t>(cy) * static_cast<std::size_t>(world_.width()) +
        static_cast<std::size_t>(cx);
    return index < terrainKeys_[layer].size() ? terrainKeys_[layer][index] : std::string{};
}

int AutotileMap::terrainMaskAt(std::size_t layer, int cx, int cy) const {
    const int centre = world_.tileAt(layer, cx, cy);
    if (centre < 0) return kNoTileMask;  // empty or out of range: no terrain
    const std::string key = terrainKeyAt(layer, cx, cy);
    static constexpr int kDX[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static constexpr int kDY[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    static constexpr int kBit[8] = {kNeighbourN,  kNeighbourNE, kNeighbourE, kNeighbourSE,
                                    kNeighbourS,  kNeighbourSW, kNeighbourW, kNeighbourNW};
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        // Out-of-map and empty reads are "no terrain" (never the same);
        // the raw diagonal is truth (the resolver canonicalises).
        if (world_.tileAt(layer, cx + kDX[i], cy + kDY[i]) < 0) continue;
        if (terrainKeyAt(layer, cx + kDX[i], cy + kDY[i]) == key) mask |= kBit[i];
    }
    return mask;
}

TileQuery AutotileMap::resolvedAt(std::size_t layer, int cx, int cy) const {
    const int mask = terrainMaskAt(layer, cx, cy);
    const auto table = tables_.find(terrainKeyAt(layer, cx, cy));
    if (mask < 0 || table == tables_.end()) {
        // Loud: no terrain, or a terrain with no table (b2's contract —
        // name the mask, never a plausible tile).
        return TileQuery{mask, -1};
    }
    return table->second.resolve(mask);
}

WorldMapSubmission AutotileMap::submitResolved(const UIRect& camera) {
    const WorldMapSubmission raw = world_.submit(camera);
    WorldMapSubmission out;
    for (const WorldMapQuad& quad : raw.below) {
        const TileQuery query = resolvedAt(quad.layer, quad.cellX, quad.cellY);
        if (!query.ok()) continue;  // draw nothing, say so at the query
        WorldMapQuad resolved = quad;
        resolved.tileId = query.tileId;
        out.below.push_back(resolved);
    }
    for (const WorldMapQuad& quad : raw.above) {
        const TileQuery query = resolvedAt(quad.layer, quad.cellX, quad.cellY);
        if (!query.ok()) continue;
        WorldMapQuad resolved = quad;
        resolved.tileId = query.tileId;
        out.above.push_back(resolved);
    }
    return out;
}

std::uint64_t AutotileMap::resolvedFieldHash(std::size_t layer) {
    StructuralHasher hasher;
    hasher.u64(static_cast<std::uint64_t>(world_.width()));
    hasher.u64(static_cast<std::uint64_t>(world_.height()));
    for (int y = 0; y < world_.height(); ++y) {
        for (int x = 0; x < world_.width(); ++x) {
            const TileQuery query = resolvedAt(layer, x, y);
            hasher.i32(query.ok() ? query.tileId : -1);
        }
    }
    return hasher.value();
}

}  // namespace ply
