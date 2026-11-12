// Cross-chunk neighbour queries (M1b b3c, E-b3.2): "chunk-boundary fixture
// produces no seam in resolved ids" — ARCHITECTURE_TARGET.md §3's WorldMap
// responsibility "cross-chunk neighbour queries for autotiling".
//
// What is pinned: WorldMap::neighbourMaskAt computes the raw 8-neighbour
// same-terrain mask from ANY cell of ANY chunk (map-grid directions, N = the
// row above), with the stated conventions — out-of-map is not same terrain,
// the raw diagonal is truth (canonicalisation is the resolver's job), an
// empty centre has no mask — and WorldMap::resolveAt composes the query with
// AutoTileResolver. THE CRITERION: a terrain region crossing chunk borders
// resolves to EXACTLY the ids it resolves to with no chunks at all — the
// resolved-id field equals an unchunked brute-force reference cell for cell,
// the seam cells literally, and the same shape wholly inside one chunk
// resolves to the same pattern translated (the seam is invisible in the
// output). Edits at a seam update every affected query immediately.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/WorldMap.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran — deleting a
// case changes the transcript).
int maskMatch = 0, maskRan = 0;
int seamMatch = 0, seamRan = 0;
int resolvedMatch = 0, resolvedRan = 0;
int noseamMatch = 0, noseamRan = 0;
int editMatch = 0, editRan = 0;
int edgeMatch = 0, edgeRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

// --- fixtures ----------------------------------------------------------------

ply::TilesetSlice makeTileset() {
    ply::TilesetSlice ts;
    ts.name = "t";
    ts.image = "t.png";
    ts.tileWidth = 16;
    ts.tileHeight = 16;
    for (int i = 0; i < 40; ++i) {
        ply::TileDef d;
        d.id = i;
        d.region = {0, i * 16, 16, 16};
        ts.tiles.push_back(d);
    }
    return ts;
}

struct LayerSpec {
    std::string name;
    std::string flagsJson;
    std::vector<int> tiles;
};

ply::MapData loadOk(int w, int h, const std::vector<LayerSpec>& layers) {
    std::string doc =
        "{\"name\": \"nm\", \"tileset\": \"t\", \"width\": " + std::to_string(w) +
        ", \"height\": " + std::to_string(h) + ", \"layers\": [";
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (i) doc += ", ";
        doc += "{\"name\": \"" + layers[i].name + "\", \"tiles\": [";
        for (std::size_t c = 0; c < layers[i].tiles.size(); ++c) {
            if (c) doc += ", ";
            doc += std::to_string(layers[i].tiles[c]);
        }
        doc += "]";
        if (!layers[i].flagsJson.empty()) doc += ", \"flags\": " + layers[i].flagsJson;
        doc += "}";
    }
    doc += "]}";
    const ply::MapLoadResult r = ply::loadMapFromString(doc);
    if (!r.ok()) {
        std::fprintf(stderr, "  FAIL: document does not load (%s)\n", r.error.message.c_str());
        ++failures;
        ply::MapData shim;
        shim.width = w;
        shim.height = h;
        for (const LayerSpec& spec : layers) {
            ply::MapLayer layer;
            layer.name = spec.name;
            layer.tiles.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), -1);
            shim.layers.push_back(layer);
        }
        return shim;
    }
    return r.map;
}

/// The b1.3 terrain fixture: one tile per canonical mask (tile i carries
/// kBlobMasks[i]) — the complete blob-47 set to resolve against.
const ply::AutoTileResolver& blob47() {
    static const ply::AutoTileResolver resolver = [] {
        const std::string path = std::string(PLY_TEST_DATA_DIR) + "/tileset_blob47.json";
        const ply::TileSetLoadResult loaded = ply::loadTileSetFile(path);
        if (!loaded.ok()) {
            std::fprintf(stderr, "  FAIL: blob47 fixture does not load (%s)\n",
                         loaded.error.message.c_str());
            ++failures;
        }
        return ply::AutoTileResolver::build(loaded.tileset);
    }();
    return resolver;
}

// --- the independent brute-force reference ------------------------------------

/// Mask computed straight off the document's tile arrays — no chunks, its own
/// bounds logic. Same written conventions as the query (direction table
/// repeated here so the grader does not lean on the implementation's).
int bruteMask(const ply::MapData& map, std::size_t layer, int cx, int cy) {
    if (layer >= map.layers.size() || cx < 0 || cy < 0 || cx >= map.width || cy >= map.height) {
        return ply::kNoTileMask;
    }
    const std::vector<int>& tiles = map.layers[layer].tiles;
    const std::size_t index =
        static_cast<std::size_t>(cy) * static_cast<std::size_t>(map.width) +
        static_cast<std::size_t>(cx);
    if (index >= tiles.size() || tiles[index] < 0) return ply::kNoTileMask;
    const int centre = tiles[index];
    static const int dx[8] = {0, 1, 1, 1, 0, -1, -1, -1};
    static const int dy[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
    static const int bit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
    int mask = 0;
    for (int i = 0; i < 8; ++i) {
        const int nx = cx + dx[i];
        const int ny = cy + dy[i];
        if (nx < 0 || ny < 0 || nx >= map.width || ny >= map.height) continue;
        const std::size_t nindex =
            static_cast<std::size_t>(ny) * static_cast<std::size_t>(map.width) +
            static_cast<std::size_t>(nx);
        if (nindex < tiles.size() && tiles[nindex] == centre) mask |= bit[i];
    }
    return mask;
}

std::vector<int> fieldFromQueries(const ply::WorldMap& wm, std::size_t layer,
                                  const ply::AutoTileResolver& resolver) {
    std::vector<int> field;
    for (int cy = 0; cy < wm.height(); ++cy) {
        for (int cx = 0; cx < wm.width(); ++cx) {
            const ply::TileQuery q = wm.resolveAt(layer, cx, cy, resolver);
            field.push_back(q.ok() ? q.tileId : -1);
        }
    }
    return field;
}

std::vector<int> bruteField(const ply::MapData& map, std::size_t layer,
                            const ply::AutoTileResolver& resolver) {
    std::vector<int> field;
    for (int cy = 0; cy < map.height; ++cy) {
        for (int cx = 0; cx < map.width; ++cx) {
            const ply::TileQuery q = resolver.resolve(bruteMask(map, layer, cx, cy));
            field.push_back(q.ok() ? q.tileId : -1);
        }
    }
    return field;
}

std::uint64_t fieldHash(const std::vector<int>& field) {
    ply::StructuralHasher h;
    h.u64(field.size());
    for (int id : field) h.i32(id);
    return h.value();
}

// --- conventions: masks, edges, diagonals, empties ----------------------------

void testConventions() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::AutoTileResolver& resolver = blob47();

    // (a) An isolated cell: no same-terrain neighbour anywhere -> mask 0,
    // and mask 0 resolves to blob tile 0 (the isolated tile).
    {
        std::vector<int> tiles(25, -1);
        tiles[2 * 5 + 2] = 7;
        const ply::MapData map = loadOk(5, 5, {{"g", "", tiles}});
        const ply::WorldMap wm = ply::WorldMap::build(map, ts);
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 2, 2) == 0, "isolated cell mask 0");
        const ply::TileQuery q = wm.resolveAt(0, 2, 2, resolver);
        pinAt(resolvedMatch, resolvedRan, q.ok() && q.tileId == 0 && q.mask == 0,
              "isolated resolves to blob tile 0");
        // An empty cell has no terrain: kNoTileMask out, loud failure out of
        // resolveAt — never a plausible fallback tile.
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 1, 1) == ply::kNoTileMask,
              "empty centre has no mask");
        const ply::TileQuery e = wm.resolveAt(0, 1, 1, resolver);
        pinAt(resolvedMatch, resolvedRan, !e.ok() && e.tileId == -1,
              "empty centre resolves loudly, no tile");
    }

    // (b) A full block: the interior cell sees all eight (mask 255 -> tile
    // 46); the MAP EDGE is a terrain edge — out-of-map neighbours are not
    // same terrain (corner (0,0): only E,SE,S -> mask 28 -> tile 10).
    {
        std::vector<int> tiles(25, 7);
        const ply::MapData map = loadOk(5, 5, {{"g", "", tiles}});
        const ply::WorldMap wm = ply::WorldMap::build(map, ts);
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 2, 2) == 255, "interior mask 255");
        pinAt(resolvedMatch, resolvedRan,
              wm.resolveAt(0, 2, 2, resolver).tileId == 46, "interior resolves to tile 46");
        pinAt(edgeMatch, edgeRan, wm.neighbourMaskAt(0, 0, 0) == 28,
              "corner mask 28 (out-of-map is not same terrain)");
        pinAt(resolvedMatch, resolvedRan,
              wm.resolveAt(0, 0, 0, resolver).tileId == 10, "corner resolves to tile 10");
        pinAt(edgeMatch, edgeRan, wm.neighbourMaskAt(0, 2, 0) == 124, "top-edge mask 124");
        pinAt(resolvedMatch, resolvedRan,
              wm.resolveAt(0, 2, 0, resolver).tileId == 31, "top edge resolves to tile 31");
    }

    // (c) Two terrains: a different tile id is NOT the same terrain, in
    // either direction across the border.
    {
        std::vector<int> tiles;
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 5; ++x) tiles.push_back(x < 2 ? 7 : 2);
        const ply::MapData map = loadOk(5, 5, {{"g", "", tiles}});
        const ply::WorldMap wm = ply::WorldMap::build(map, ts);
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 1, 2) == 241,
              "grass side of the border: mask 241");
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 2, 2) == 31,
              "water side of the border: mask 31");
        pinAt(resolvedMatch, resolvedRan,
              wm.resolveAt(0, 1, 2, resolver).tileId == 42, "grass border resolves to 42");
        pinAt(resolvedMatch, resolvedRan,
              wm.resolveAt(0, 2, 2, resolver).tileId == 12, "water border resolves to 12");
    }

    // (d) The RAW diagonal is truth: a same-terrain diagonal sets its bit
    // even when its edge neighbours differ (mask 2), and canonicalisation
    // onto the blob-47 set is the RESOLVER's job (2 -> 0 -> tile 0).
    {
        std::vector<int> tiles(25, -1);
        tiles[2 * 5 + 2] = 7;
        tiles[1 * 5 + 3] = 7;  // NE of (2,2)
        tiles[2 * 5 + 3] = 2;  // E of (2,2): different terrain
        const ply::MapData map = loadOk(5, 5, {{"g", "", tiles}});
        const ply::WorldMap wm = ply::WorldMap::build(map, ts);
        pinAt(maskMatch, maskRan, wm.neighbourMaskAt(0, 2, 2) == 2,
              "raw diagonal kept without its edges (mask 2)");
        const ply::TileQuery q = wm.resolveAt(0, 2, 2, resolver);
        pinAt(resolvedMatch, resolvedRan, q.ok() && q.tileId == 0 && q.mask == 0,
              "the resolver canonicalises 2 -> 0 -> tile 0");
    }

    // (e) Masks are per-layer: the same coordinates on two layers read their
    // own layer's tiles.
    {
        std::vector<int> g(25, -1), w(25, -1);
        g[2 * 5 + 2] = 7;
        w[2 * 5 + 2] = 5;
        w[2 * 5 + 3] = 5;  // same terrain on layer 1 only
        const ply::MapData map = loadOk(5, 5, {{"g", "", g}, {"w", "", w}});
        const ply::WorldMap wm = ply::WorldMap::build(map, ts);
        pinAt(maskMatch, maskRan,
              wm.neighbourMaskAt(0, 2, 2) == 0 && wm.neighbourMaskAt(1, 2, 2) == 4,
              "masks read their own layer (0 vs E bit)");
    }
}

// --- the chunk-boundary fixture: no seam in resolved ids ---------------------

std::vector<int> regionMapTiles(int rx0, int ry0) {
    // 33x20 (3x2 chunks), a 13x8 terrain-7 region at (rx0, ry0), terrain 3
    // everywhere else. The region at (10, 10) crosses the x=256 and y=64
    // chunk seams; the same shape at (17, 2) lies wholly inside chunk (1,0).
    std::vector<int> tiles;
    for (int y = 0; y < 20; ++y) {
        for (int x = 0; x < 33; ++x) {
            const bool inRegion = x >= rx0 && x < rx0 + 13 && y >= ry0 && y < ry0 + 8;
            tiles.push_back(inRegion ? 7 : 3);
        }
    }
    return tiles;
}

void testChunkSeams() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::AutoTileResolver& resolver = blob47();
    const ply::MapData map = loadOk(33, 20, {{"g", "", regionMapTiles(10, 10)}});
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    pinAt(edgeMatch, edgeRan,
          wm.width() == 33 && wm.height() == 20 && wm.chunkCount() == 6,
          "fixture chunks into 3x2");

    // Literal seam masks: (15,13) and (16,13) sit either side of the x=256
    // seam, both interior to the region — their E/W reads CROSS the seam and
    // must still see same terrain (255 -> tile 46).
    pinAt(seamMatch, seamRan,
          wm.neighbourMaskAt(0, 15, 13) == 255 && wm.neighbourMaskAt(0, 16, 13) == 255,
          "seam-interior masks 255 on both sides");
    pinAt(resolvedMatch, resolvedRan,
          wm.resolveAt(0, 15, 13, resolver).tileId == 46 &&
              wm.resolveAt(0, 16, 13, resolver).tileId == 46,
          "seam-interior resolves 46 on both sides");
    pinAt(seamMatch, seamRan, wm.neighbourMaskAt(0, 16, 10) == 124,
          "seam edge mask 124 (W+SW cross the seam)");
    // (16,10): the region's top edge AT the seam column — W and SW read
    // across the seam (mask 124 -> tile 31).
    pinAt(resolvedMatch, resolvedRan,
          wm.resolveAt(0, 16, 10, resolver).tileId == 31, "seam edge resolves to tile 31");
    // (15,17): the region's bottom-left corner region cell, with NE/E reads
    // crossing the x=256 chunk seam from chunk (0,1) to (1,1) (mask 199 ->
    // tile 36). No named literal pin exercises a cross-y-SEAM read — the
    // y=64 seam is covered by the 660-mask sweep, the field equality and
    // the translation pin (the b3c read's O1).
    pinAt(seamMatch, seamRan, wm.neighbourMaskAt(0, 15, 17) == 199,
          "x-seam corner mask 199 (NE+E cross the seam)");
    pinAt(resolvedMatch, resolvedRan,
          wm.resolveAt(0, 15, 17, resolver).tileId == 36, "y-seam corner resolves to tile 36");

    // EVERY cell: the chunked queries equal the unchunked brute reference —
    // masks and the full resolved-id field. This is the criterion.
    long long maskAgree = 0;
    for (int cy = 0; cy < 20; ++cy) {
        for (int cx = 0; cx < 33; ++cx) {
            if (wm.neighbourMaskAt(0, cx, cy) == bruteMask(map, 0, cx, cy)) ++maskAgree;
        }
    }
    pinAt(maskMatch, maskRan, maskAgree == 33LL * 20LL,
          "all 660 chunked masks == brute masks");
    const std::vector<int> field = fieldFromQueries(wm, 0, resolver);
    const std::vector<int> expected = bruteField(map, 0, resolver);
    pinAt(noseamMatch, noseamRan, field == expected,
          "resolved-id field == unchunked brute field (no seam)");
    pinAt(noseamMatch, noseamRan,
          fieldHash(field) == fieldHash(expected),
          "resolved-id field hash == brute hash (ADR 0014)");

    // THE SEAM IS INVISIBLE: the same 13x8 shape wholly inside chunk (1,0)
    // resolves to the same pattern, translated — so the pattern differences
    // at the seam can only come from the seam, not the shape.
    const ply::MapData mapB = loadOk(33, 20, {{"g", "", regionMapTiles(17, 2)}});
    ply::WorldMap wmB = ply::WorldMap::build(mapB, ts);
    const std::vector<int> fieldB = fieldFromQueries(wmB, 0, resolver);
    // Region A bbox +1 margin: x 9..23, y 9..18. Region B: x 16..30, y 1..10.
    std::vector<int> gridA, gridB;
    for (int y = 9; y <= 18; ++y)
        for (int x = 9; x <= 23; ++x) gridA.push_back(field[static_cast<std::size_t>(y) * 33 + x]);
    for (int y = 1; y <= 10; ++y)
        for (int x = 16; x <= 30; ++x)
            gridB.push_back(fieldB[static_cast<std::size_t>(y) * 33 + x]);
    pinAt(noseamMatch, noseamRan, gridA.size() == 150 && gridA == gridB,
          "seam-crossing shape == chunk-interior shape, translated");
    pinAt(noseamMatch, noseamRan, fieldHash(gridA) == fieldHash(gridB),
          "translated grids hash equal");
}

// --- edits at a seam propagate immediately -----------------------------------

void testSeamEdits() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::AutoTileResolver& resolver = blob47();
    const ply::MapData map = loadOk(33, 20, {{"g", "", regionMapTiles(10, 10)}});
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    // Poke a different-terrain cell AT the seam: the neighbour chunk's cell
    // (15,13) must see it immediately — no rebuild, no flush, no seam lag.
    wm.setTile(0, 16, 13, 3);
    pinAt(editMatch, editRan, wm.neighbourMaskAt(0, 15, 13) == 251,
          "seam edit visible at once: raw mask 251");
    const ply::TileQuery q = wm.resolveAt(0, 15, 13, resolver);
    pinAt(editMatch, editRan, q.ok() && q.tileId == 42,
          "seam edit re-resolves at once (251 canonicalises to 241 -> 42)");
    pinAt(editMatch, editRan, wm.neighbourMaskAt(0, 16, 13) == 0,
          "the edited cell itself is isolated now");
    pinAt(editMatch, editRan, wm.resolveAt(0, 16, 13, resolver).tileId == 0,
          "and resolves to the isolated tile");
    // Restore: the seam heals with no lag.
    wm.setTile(0, 16, 13, 7);
    pinAt(editMatch, editRan, wm.neighbourMaskAt(0, 15, 13) == 255 &&
                                  wm.resolveAt(0, 15, 13, resolver).tileId == 46,
          "restore heals the seam at once");

    // Out-of-range probes: no terrain, no crash, loud resolve.
    pinAt(edgeMatch, edgeRan,
          wm.neighbourMaskAt(1, 0, 0) == ply::kNoTileMask &&
              wm.neighbourMaskAt(0, -1, 0) == ply::kNoTileMask &&
              wm.neighbourMaskAt(0, 33, 19) == ply::kNoTileMask,
          "OOB queries are kNoTileMask");
    pinAt(edgeMatch, edgeRan,
          !wm.resolveAt(0, -1, 0, resolver).ok() &&
              !wm.resolveAt(2, 0, 0, resolver).ok(),
          "OOB resolve fails loudly");
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::AutoTileResolver& resolver = blob47();
    const ply::MapData map = loadOk(33, 20, {{"g", "", regionMapTiles(10, 10)}});
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    // The raw-diagonal measurement, live (conventions pin (d)).
    std::vector<int> dg(25, -1);
    dg[2 * 5 + 2] = 7;
    dg[1 * 5 + 3] = 7;
    dg[2 * 5 + 3] = 2;
    const ply::MapData diagMap = loadOk(5, 5, {{"g", "", dg}});
    ply::WorldMap diagWm = ply::WorldMap::build(diagMap, ts);

    std::printf(
        "EVIDENCE neighbour cells=%dx%d chunks=%zu masks=%d/%d seam=%d/%d resolved=%d/%d "
        "noseam=%d/%d edits=%d/%d edge=%d/%d raw_diag=%d hash=%016llx\n",
        wm.width(), wm.height(), wm.chunkCount(), maskMatch, maskRan, seamMatch, seamRan,
        resolvedMatch, resolvedRan, noseamMatch, noseamRan, editMatch, editRan, edgeMatch,
        edgeRan, diagWm.neighbourMaskAt(0, 2, 2),
        static_cast<unsigned long long>(
            fieldHash(fieldFromQueries(wm, 0, resolver))));
}

}  // namespace

int main() {
    testConventions();
    testChunkSeams();
    testSeamEdits();

    if (failures == 0) {
        printEvidence();
        std::printf("neighbour tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "neighbour tests: %d failure(s)\n", failures);
    return 1;
}
