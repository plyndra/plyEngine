// AutotileMap (M1b b3d — the gate-gap pre-decision's glue): terrain fills
// resolve AT LOAD through per-variant-group blob tables and submit RESOLVED
// quads. The pre-decision's done-when: a multi-terrain fixture (2+
// variant-group terrains crossing chunk seams) resolves with no seam,
// submits resolved quads with the exact-count invariant holding on the
// RESOLVED path, and an absent terrain table fails loudly (b2's contract).
//
// Pinned: the variant-group identity UPGRADE (different fill ids of one
// group are the same terrain); the per-group table selection (grass
// resolves against grass art, water against water); the no-seam field
// (chunked resolution == unchunked brute, field + ADR-0014 hash); the
// exact-count on the resolved path (submitResolved == the b3b rule applied
// to resolved ids); and the loud-absent clause (no table -> ok() false,
// tileId -1, nothing submitted).
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/AutotileMap.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

int identityMatch = 0, identityRan = 0;
int resolveMatch = 0, resolveRan = 0;
int seamMatch = 0, seamRan = 0;
int loudMatch = 0, loudRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

/// Safe read: a missing cell reads as an impossible value so a hollow build
/// fails the pin, never the process (the a5.1 lesson — six strikes so far).
int atOr(const std::vector<int>& field, std::size_t index) {
    return index < field.size() ? field[index] : -999999;
}

// --- the multi-terrain fixture (grass + water complete, lava absent) --------

ply::TileSet terrainTileset() {
    ply::TileSet ts;
    ts.name = "terrains";
    ts.image = "t.png";
    ts.tileWidth = 16;
    ts.tileHeight = 16;
    // grass: one tile per canonical mask (the blob47 shape), variant "grass".
    for (int i = 0; i < ply::kBlobMaskCount; ++i) {
        ply::TileData tile;
        tile.id = i;
        tile.region = {0, i * 16, 16, 16};
        tile.mask = ply::kBlobMasks[i];
        tile.variant = "grass";
        ts.tiles.push_back(tile);
    }
    // water: the same 47 masks, variant "water", ids 100..146.
    for (int i = 0; i < ply::kBlobMaskCount; ++i) {
        ply::TileData tile;
        tile.id = 100 + i;
        tile.region = {16, i * 16, 16, 16};
        tile.mask = ply::kBlobMasks[i];
        tile.variant = "water";
        ts.tiles.push_back(tile);
    }
    // lava: a variant group with NO mask roles -> no table -> loud failure.
    ply::TileData lava;
    lava.id = 200;
    lava.region = {32, 0, 16, 16};
    lava.variant = "lava";
    ts.tiles.push_back(lava);
    return ts;
}

/// 33x20 (3x2 chunks): a grass region crossing both seams (fills alternate
/// ids 0 and 5 — one variant group, THE upgrade), a water region, and a
/// lava patch whose table is absent. Background is empty.
ply::MapData terrainMap() {
    ply::MapData map;
    map.name = "m";
    map.tileset = "terrains";
    map.width = 33;
    map.height = 20;
    ply::MapLayer ground;
    ground.name = "ground";
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            if (x >= 10 && x <= 22 && y >= 10 && y <= 17) {
                ground.tiles.push_back((x + y) % 2 == 0 ? 0 : 5);  // grass fills
            } else if (x <= 5 && y <= 5) {
                ground.tiles.push_back(100);  // water
            } else if (x >= 30 && y >= 18) {
                ground.tiles.push_back(200);  // lava (no table)
            } else {
                ground.tiles.push_back(-1);
            }
        }
    }
    map.layers.push_back(ground);
    return map;
}

// --- the unchunked brute reference (its own identity + resolution) -----------

std::string bruteKey(const ply::TileSet& ts, int id) {
    const ply::TileData* tile = ts.findTile(id);
    if (tile == nullptr) return {};
    return tile->variant.empty() ? "id:" + std::to_string(id) : tile->variant;
}

int bruteMask(const ply::MapData& map, const ply::TileSet& ts, int cx, int cy) {
    const std::vector<int>& tiles = map.layers[0].tiles;
    const std::size_t index = static_cast<std::size_t>(cy) * 33 + static_cast<std::size_t>(cx);
    if (index >= tiles.size() || tiles[index] < 0) return ply::kNoTileMask;
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    static const int bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    const std::string key = bruteKey(ts, tiles[index]);
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        const int nx = cx + dx[i];
        const int ny = cy + dy[i];
        if (nx < 0 || ny < 0 || nx >= map.width || ny >= map.height) continue;
        const std::size_t nindex =
            static_cast<std::size_t>(ny) * 33 + static_cast<std::size_t>(nx);
        if (nindex < tiles.size() && tiles[nindex] >= 0 &&
            bruteKey(ts, tiles[nindex]) == key) {
            mask |= bit[i];
        }
    }
    return mask;
}

/// Independent resolution: the group's tile whose mask role is the
/// canonicalised mask (no AutoTileResolver in the reference path).
int bruteResolve(const ply::TileSet& ts, int id, int mask) {
    const ply::TileData* self = ts.findTile(id);
    if (self == nullptr || mask < 0) return -1;
    const std::string key = bruteKey(ts, id);
    const int canonical = ply::canonicalizeBlobMask(mask);
    for (const ply::TileData& tile : ts.tiles) {
        if (bruteKey(ts, tile.id) == key && tile.mask == canonical) return tile.id;
    }
    return -1;  // absent table or absent mask: loud no-tile
}

std::uint64_t fieldHash(const std::vector<int>& field) {
    ply::StructuralHasher hasher;
    hasher.u64(field.size());
    for (int id : field) hasher.i32(id);
    return hasher.value();
}

// --- pins --------------------------------------------------------------------

void testIdentityAndSelection() {
    const ply::TileSet ts = terrainTileset();
    const ply::MapData map = terrainMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);

    pinAt(identityMatch, identityRan,
          autotile.terrainKeyAt(0, 15, 13) == "grass" &&
              autotile.terrainKeyAt(0, 2, 2) == "water" &&
              autotile.terrainKeyAt(0, 31, 19) == "lava",
          "terrain keys: grass / water / lava");

    // THE VARIANT-GROUP UPGRADE: the grass region alternates fill ids 0 and
    // 5 — one variant group, so an interior cell sees same terrain all
    // around (mask 255) where tile-id equality would see a checkerboard.
    pinAt(identityMatch, identityRan, autotile.terrainMaskAt(0, 15, 13) == 255,
          "variant-group identity: mixed fills of one group are one terrain");
    // The grass/water border: different groups are NOT same terrain.
    pinAt(identityMatch, identityRan,
          autotile.terrainMaskAt(0, 2, 2) == 255 && autotile.terrainMaskAt(0, 10, 10) != 255,
          "different groups are different terrain at the border");

    // Every chunked mask equals the brute mask (variant identity, all 660).
    long long agree = 0;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            if (autotile.terrainMaskAt(0, x, y) == bruteMask(map, ts, x, y)) ++agree;
        }
    }
    pinAt(identityMatch, identityRan, agree == 33LL * 20LL,
          "all 660 chunked masks == brute masks (variant identity)");

    // Per-group table selection: mask 255 resolves to each group's own art.
    pinAt(resolveMatch, resolveRan,
          autotile.resolvedAt(0, 15, 13).tileId == 46,
          "grass resolves against the grass table (255 -> 46)");
    pinAt(resolveMatch, resolveRan,
          autotile.resolvedAt(0, 2, 2).tileId == 146,
          "water resolves against the water table (255 -> 146)");
}

void testNoSeamAndExactCount() {
    const ply::TileSet ts = terrainTileset();
    const ply::MapData map = terrainMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);

    // THE PRE-DECISION'S CRITERION: the resolved field equals the unchunked
    // brute (variant identity + per-group resolution) cell for cell, with
    // the ADR-0014 hashes equal — the region crosses BOTH chunk seams and
    // no seam appears in the resolved ids.
    std::vector<int> field;
    std::vector<int> brute;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            const ply::TileQuery query = autotile.resolvedAt(0, x, y);
            field.push_back(query.ok() ? query.tileId : -1);
            const std::size_t index = static_cast<std::size_t>(y) * 33 + static_cast<std::size_t>(x);
            brute.push_back(bruteResolve(ts, map.layers[0].tiles[index],
                                         bruteMask(map, ts, x, y)));
        }
    }
    pinAt(seamMatch, seamRan, field == brute, "resolved field == unchunked brute (no seam)");
    pinAt(seamMatch, seamRan, fieldHash(field) == fieldHash(brute),
          "resolved field hashes equal (ADR 0014)");

    // The exact-count invariant on the RESOLVED path: every non-empty
    // cell that resolves submits exactly one quad (no opaque layers here),
    // and the quad list equals the brute resolved list.
    ply::WorldMapSubmission submission = autotile.submitResolved({0, 0, 528, 320});
    std::vector<int> expectedIds;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            const std::size_t index = static_cast<std::size_t>(y) * 33 + static_cast<std::size_t>(x);
            const int id = brute[index];
            if (id >= 0) expectedIds.push_back(id);
        }
    }
    std::vector<int> gotIds;
    for (const ply::WorldMapQuad& quad : submission.below) gotIds.push_back(quad.tileId);
    for (const ply::WorldMapQuad& quad : submission.above) gotIds.push_back(quad.tileId);
    // Content + count (the submission ORDER is b3b's chunk-order contract,
    // pinned there; this unit pins what the resolved path carries).
    std::sort(gotIds.begin(), gotIds.end());
    std::sort(expectedIds.begin(), expectedIds.end());
    pinAt(seamMatch, seamRan, gotIds == expectedIds,
          "resolved quads carry exactly the brute resolved ids");
    pinAt(seamMatch, seamRan, gotIds.size() == expectedIds.size(),
          "the count is exact on the resolved path");
}

void testAbsentTableIsLoud() {
    const ply::TileSet ts = terrainTileset();
    const ply::MapData map = terrainMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);

    // lava has NO mask-role tiles -> no table -> loud failure, never a tile.
    const ply::TileQuery query = autotile.resolvedAt(0, 31, 19);
    pinAt(loudMatch, loudRan, !query.ok() && query.tileId == -1,
          "absent terrain table fails loudly (ok false, tileId -1)");
    pinAt(loudMatch, loudRan, query.mask == ply::kNoTileMask || query.mask >= 0,
          "the loud failure names a mask (never a silent default)");

    // And lava cells submit NOTHING (the b2 rule: draw nothing, say so).
    ply::WorldMapSubmission submission = autotile.submitResolved({0, 0, 528, 320});
    bool lavaAbsent = true;
    for (const ply::WorldMapQuad& quad : submission.below) {
        if (quad.cellX >= 30 && quad.cellY >= 18) lavaAbsent = false;
    }
    for (const ply::WorldMapQuad& quad : submission.above) {
        if (quad.cellX >= 30 && quad.cellY >= 18) lavaAbsent = false;
    }
    pinAt(loudMatch, loudRan, lavaAbsent, "absent-table cells submit nothing");

    // An empty cell: no mask, loud resolve.
    pinAt(loudMatch, loudRan,
          autotile.terrainMaskAt(0, 8, 8) == ply::kNoTileMask &&
              !autotile.resolvedAt(0, 8, 8).ok(),
          "empty cells: no mask, loud resolve");
}

void printEvidence() {
    const ply::TileSet ts = terrainTileset();
    const ply::MapData map = terrainMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);
    std::vector<int> field;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            const ply::TileQuery query = autotile.resolvedAt(0, x, y);
            field.push_back(query.ok() ? query.tileId : -1);
        }
    }
    std::printf(
        "EVIDENCE autotile_map terrains=2 cells=33x20 no_seam=identical exact=%d/%d "
        "absent=loud variants=upgraded identity=%d/%d resolve=%d/%d loud=%d/%d "
        "hash=%016llx\n",
        seamMatch, seamRan, identityMatch, identityRan, resolveMatch, resolveRan, loudMatch,
        loudRan, static_cast<unsigned long long>(fieldHash(field)));
}

}  // namespace

int main() {
    testIdentityAndSelection();
    testNoSeamAndExactCount();
    testAbsentTableIsLoud();

    if (failures == 0) {
        printEvidence();
        std::printf("autotile_map tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "autotile_map tests: %d failure(s)\n", failures);
    return 1;
}
