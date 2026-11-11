// WorldMap (M1b b3b, E-b3.1): "submitted quads == tiles intersecting camera
// rect, exact" — ARCHITECTURE_TARGET.md §3's culling-correctness clause — plus
// the chunked 16×16 storage and per-chunk dirty flags the roadmap row names,
// chunk-boundary traversal, and the two jobs b3a's layer flags do here:
// kLayerOpaque coverage culling and kLayerAboveEntities pass assignment.
//
// The exact count is pinned two ways: against hand-written literal quad lists
// (so a change in the intersection rule, the y flip, or the order contract
// fails a named pin) and against an INDEPENDENT brute-force scan that walks
// every cell of every layer with its own overlap and coverage logic (so the
// chunk machinery cannot quietly drop or duplicate a tile). The culled
// tile-id list hash (ADR 0014 — structural, never a pixel golden) is pinned
// against the brute list's hash and across a second process.
//
// Budget (ADR 0012): one full internal-resolution view (640×360) is measured
// and pinned under the ≤4000-quad budget, as a live count in the transcript.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/WorldMap.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#define PLY_HAVE_SELF_EXE 1
#else
#define PLY_HAVE_SELF_EXE 0
#endif

namespace {

int failures = 0;

// Live EVIDENCE counters: each is incremented by the pins that actually ran,
// so deleting a case changes the transcript (the "format-string literal"
// defect class fixed twice before — O1 from the b3a verifier read).
int storeMatch = 0, storeRan = 0;
int exactMatch = 0, exactRan = 0;
int boundaryMatch = 0, boundaryRan = 0;
int opaqueMatch = 0, opaqueRan = 0;
int passMatch = 0, passRan = 0;
int dirtyMatch = 0, dirtyRan = 0;
int hashMatch = 0, hashRan = 0;

// ADR 0012's per-internal-frame quad budget.
constexpr int kQuadBudget = 4000;

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
    std::string flagsJson;  // raw JSON value; "" omits the key (b3a default)
    std::vector<int> tiles;
};

/// Builds the native JSON document and loads it through the real loader, so
/// the flags under test are the ones a map author would declare (b3a's
/// token parse). A failed load is a loud failure and returns a shim with the
/// requested shape, so broken input fails pins instead of aborting the suite
/// (the a5.1 harness lesson).
ply::MapData loadOk(int w, int h, const std::vector<LayerSpec>& layers) {
    std::string doc =
        "{\"name\": \"wm\", \"tileset\": \"t\", \"width\": " + std::to_string(w) +
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
        shim.name = "wm";
        shim.tileset = "t";
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

// --- fixture A: 4x3, two layers, literal pins --------------------------------

const std::vector<LayerSpec>& fixtureASpecs() {
    static const std::vector<LayerSpec> specs = {
        {"ground", "", {0, 1, 2, 3, -1, 5, 6, -1, 8, 9, 10, 11}},
        {"canopy", "[\"above-entities\"]",
         {-1, 20, -1, -1, -1, -1, 21, -1, -1, -1, -1, 22}},
    };
    return specs;
}

// --- fixture B: 33x20 (3x2 chunks), dense deterministic ids ------------------

const std::vector<LayerSpec> fixtureBSpecs() {
    std::vector<int> ground, upper;
    for (int cy = 0; cy < 20; ++cy) {
        for (int cx = 0; cx < 33; ++cx) {
            ground.push_back(1 + ((cx * 7 + cy * 13) % 39));
            upper.push_back(1 + ((cx * 11 + cy * 17 + 5) % 39));
        }
    }
    return {{"ground", "", ground}, {"upper", "[\"above-entities\"]", upper}};
}

// --- fixture C: 6x2, five layers — opaque coverage and pass assignment -------

const std::vector<LayerSpec>& fixtureCSpecs() {
    static const std::vector<LayerSpec> specs = {
        // ground: everywhere, draws first (default = below-entities).
        {"ground", "", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}},
        // slab: opaque + below; covers (0,0) and (3,0).
        {"slab", "[\"below-entities\", \"opaque\"]",
         {13, -1, -1, 14, -1, -1, -1, -1, -1, -1, -1, -1}},
        // anim: flags "animated" only — no above bit, so it draws BELOW
        // entities (MapLayer::aboveEntities() is the pass selector).
        {"anim", "[\"animated\"]",
         {15, 16, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}},
        // canopy: above entities, animated (inert until b4).
        {"canopy", "[\"above-entities\", \"animated\"]",
         {17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}},
        // roof: opaque + above; covers (2,0) and (0,1) — including ground
        // BEHIND it across the pass boundary.
        {"roof", "[\"above-entities\", \"opaque\"]",
         {-1, -1, 18, -1, -1, -1, 19, -1, -1, -1, -1, -1}},
    };
    return specs;
}

// --- fixture D: 48x30, two dense layers — the ADR 0012 budget ---------------

const std::vector<LayerSpec> fixtureDSpecs() {
    std::vector<int> ground, upper;
    for (int cy = 0; cy < 30; ++cy) {
        for (int cx = 0; cx < 48; ++cx) {
            ground.push_back(1 + ((cx + cy) % 39));
            upper.push_back(1 + ((cx * 3 + cy) % 39));
        }
    }
    return {{"ground", "", ground}, {"upper", "[\"above-entities\"]", upper}};
}

// --- the independent brute-force expectation --------------------------------

ply::WorldMapQuad quadFor(const ply::TilesetSlice& ts, const ply::MapData& map, int cx,
                          int cy, int id) {
    ply::WorldMapQuad q;
    q.tileId = id;
    q.cellX = cx;
    q.cellY = cy;
    q.rect = {cx * ts.tileWidth, (map.height - 1 - cy) * ts.tileHeight, ts.tileWidth,
              ts.tileHeight};
    return q;
}

/// Positive-area overlap; edge-touching is NOT intersection (a zero-area
/// sliver draws no pixels). This is the criterion's rule, spelled once.
bool overlaps(const ply::UIRect& a, const ply::UIRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

/// Draw order: (pass, authored index) lexicographic — below pass first, then
/// above; authored order within a pass.
bool drawsAfter(const ply::MapData& map, std::size_t later, std::size_t earlier) {
    const int passLater = map.layers[later].aboveEntities() ? 1 : 0;
    const int passEarly = map.layers[earlier].aboveEntities() ? 1 : 0;
    if (passLater != passEarly) return passLater > passEarly;
    return later > earlier;
}

/// A cell of `layer` is covered when a LATER-drawn layer is opaque and has a
/// tile at the same cell. Same-cell coverage is full coverage (one grid).
bool coveredAt(const ply::MapData& map, std::size_t layer, int cx, int cy) {
    const std::size_t index =
        static_cast<std::size_t>(cy) * static_cast<std::size_t>(map.width) +
        static_cast<std::size_t>(cx);
    for (std::size_t other = 0; other < map.layers.size(); ++other) {
        if (!drawsAfter(map, other, layer)) continue;
        if (!map.layers[other].hasFlag(ply::kLayerOpaque)) continue;
        if (index < map.layers[other].tiles.size() && map.layers[other].tiles[index] >= 0) {
            return true;
        }
    }
    return false;
}

/// One pass of the expected submission, built cell by cell with no chunk
/// structure at all — but in the documented order contract (layers authored,
/// chunks top-to-bottom left-to-right, cells row-major within a chunk), which
/// is what the implementation owes and the hash commits to.
std::vector<ply::WorldMapQuad> brutePass(const ply::MapData& map, const ply::TilesetSlice& ts,
                                         const ply::UIRect& cam, bool above) {
    std::vector<ply::WorldMapQuad> out;
    const int cols = (map.width + ply::kChunkSize - 1) / ply::kChunkSize;
    const int rows = (map.height + ply::kChunkSize - 1) / ply::kChunkSize;
    for (std::size_t l = 0; l < map.layers.size(); ++l) {
        if (map.layers[l].aboveEntities() != above) continue;
        for (int cr = 0; cr < rows; ++cr) {
            for (int cc = 0; cc < cols; ++cc) {
                const int cyEnd = std::min(map.height, (cr + 1) * ply::kChunkSize);
                const int cxEnd = std::min(map.width, (cc + 1) * ply::kChunkSize);
                for (int cy = cr * ply::kChunkSize; cy < cyEnd; ++cy) {
                    for (int cx = cc * ply::kChunkSize; cx < cxEnd; ++cx) {
                        const std::size_t index =
                            static_cast<std::size_t>(cy) *
                                static_cast<std::size_t>(map.width) +
                            static_cast<std::size_t>(cx);
                        if (index >= map.layers[l].tiles.size()) continue;
                        const int id = map.layers[l].tiles[index];
                        if (id < 0) continue;
                        if (coveredAt(map, l, cx, cy)) continue;
                        const ply::WorldMapQuad q = quadFor(ts, map, cx, cy, id);
                        if (!overlaps(q.rect, cam)) continue;
                        out.push_back(q);
                    }
                }
            }
        }
    }
    return out;
}

std::vector<int> idsOf(const std::vector<ply::WorldMapQuad>& quads) {
    std::vector<int> ids;
    ids.reserve(quads.size());
    for (const ply::WorldMapQuad& q : quads) ids.push_back(q.tileId);
    return ids;
}

bool hasId(const std::vector<ply::WorldMapQuad>& quads, int id) {
    for (const ply::WorldMapQuad& q : quads) {
        if (q.tileId == id) return true;
    }
    return false;
}

std::uint64_t bruteHash(const std::vector<ply::WorldMapQuad>& below,
                        const std::vector<ply::WorldMapQuad>& above) {
    ply::WorldMapSubmission s;
    s.below = below;
    s.above = above;
    return s.hash();
}

// --- storage and out-of-range guards ----------------------------------------

void testStorage() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    pinAt(storeMatch, storeRan, wm.width() == 33 && wm.height() == 20, "dims 33x20");
    pinAt(storeMatch, storeRan, wm.tileWidth() == 16 && wm.tileHeight() == 16, "tile 16x16");
    pinAt(storeMatch, storeRan, wm.layerCount() == 2, "two layers");
    pinAt(storeMatch, storeRan, wm.chunkColumns() == 3 && wm.chunkRows() == 2, "chunk grid 3x2");
    pinAt(storeMatch, storeRan, wm.chunkCount() == 6, "six chunks");

    // Full sweep: every cell reads back what the document holds, across the
    // chunk seams (cx 15/16/32 and cy 15/16 land in different chunks).
    long long sweep = 0;
    const long long sweepTotal = 2LL * 33LL * 20LL;
    for (std::size_t l = 0; l < 2; ++l) {
        for (int cy = 0; cy < 20; ++cy) {
            for (int cx = 0; cx < 33; ++cx) {
                const int expected = map.layers[l].tiles[static_cast<std::size_t>(cy) * 33 + cx];
                if (wm.tileAt(l, cx, cy) == expected) ++sweep;
            }
        }
    }
    pinAt(storeMatch, storeRan, sweep == sweepTotal, "cell sweep matches the document");

    // Out-of-range probes read empty and never assert (the a5.1 lesson: a
    // caller probing past an edge gets "no tile", and the grader itself
    // must not index past its own fixtures).
    pinAt(storeMatch, storeRan, wm.tileAt(0, -1, 0) == -1, "tileAt x<0 is -1");
    pinAt(storeMatch, storeRan, wm.tileAt(0, 33, 0) == -1, "tileAt x>=width is -1");
    pinAt(storeMatch, storeRan, wm.tileAt(0, 0, -1) == -1, "tileAt y<0 is -1");
    pinAt(storeMatch, storeRan, wm.tileAt(0, 0, 20) == -1, "tileAt y>=height is -1");
    pinAt(storeMatch, storeRan, wm.tileAt(2, 0, 0) == -1, "tileAt layer OOB is -1");
    pinAt(storeMatch, storeRan, wm.layerFlags(2) == 0u, "layerFlags OOB is 0");

    // An out-of-range write is ignored: no crash, no dirty, no change. Build
    // every chunk first (whole-map submit) so the grid is clean and any new
    // dirty flag is attributable to the write below.
    wm.submit({0, 0, 528, 320});
    wm.setTile(0, -1, 0, 5);
    wm.setTile(9, 0, 0, 5);
    pinAt(storeMatch, storeRan, !wm.chunkDirty(0, 0) && !wm.chunkDirty(2, 1),
          "OOB setTile dirties nothing");
    pinAt(storeMatch, storeRan, wm.tileAt(0, 0, 0) == map.layers[0].tiles[0],
          "OOB setTile changes nothing");

    // An empty map chunks into nothing and submits nothing.
    ply::MapData empty;
    ply::WorldMap none = ply::WorldMap::build(empty, ts);
    pinAt(storeMatch, storeRan, none.chunkCount() == 0, "empty map has no chunks");
    pinAt(storeMatch, storeRan,
          none.submit({0, 0, 64, 48}).below.empty() && none.submit({0, 0, 64, 48}).above.empty(),
          "empty map submits nothing");
}

// --- exact counts: fixture A literal pins + fixture D budget ----------------

void testExactCounts() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData mapA = loadOk(4, 3, fixtureASpecs());
    ply::WorldMap a = ply::WorldMap::build(mapA, ts);

    // Literal quads spell their rects outright, so a wrong y flip or cell
    // size fails the literal, not just the brute comparison.
    const auto q = [](int id, int cx, int cy, int x, int y) {
        ply::WorldMapQuad quad;
        quad.tileId = id;
        quad.cellX = cx;
        quad.cellY = cy;
        quad.rect = {x, y, 16, 16};
        return quad;
    };

    // Whole map: the literal submission, in contract order (one chunk,
    // row-major cells; ground layer before canopy layer).
    {
        const ply::UIRect cam{0, 0, 64, 48};
        const ply::WorldMapSubmission s = a.submit(cam);
        const std::vector<ply::WorldMapQuad> expBelow = {
            q(0, 0, 0, 0, 32),  q(1, 1, 0, 16, 32), q(2, 2, 0, 32, 32), q(3, 3, 0, 48, 32),
            q(5, 1, 1, 16, 16), q(6, 2, 1, 32, 16), q(8, 0, 2, 0, 0),   q(9, 1, 2, 16, 0),
            q(10, 2, 2, 32, 0), q(11, 3, 2, 48, 0),
        };
        const std::vector<ply::WorldMapQuad> expAbove = {
            q(20, 1, 0, 16, 32),
            q(21, 2, 1, 32, 16),
            q(22, 3, 2, 48, 0),
        };
        pinAt(exactMatch, exactRan, s.below == expBelow, "A whole-map below literal");
        pinAt(exactMatch, exactRan, s.above == expAbove, "A whole-map above literal");
        pinAt(exactMatch, exactRan,
              s.below.size() + s.above.size() == 13 &&
                  s.below.size() + s.above.size() ==
                      brutePass(mapA, ts, cam, false).size() +
                          brutePass(mapA, ts, cam, true).size(),
              "A whole-map count 13 == brute");
        pinAt(exactMatch, exactRan,
              s.hash() == bruteHash(brutePass(mapA, ts, cam, false),
                                    brutePass(mapA, ts, cam, true)),
              "A whole-map hash == brute hash");
    }

    // Strict overlap at the bottom row's upper edge: camera height 16 ends
    // exactly at y=16 where row cy=1 begins — zero-area touching submits
    // nothing.
    {
        const ply::WorldMapSubmission s = a.submit({16, 0, 16, 16});
        pinAt(exactMatch, exactRan,
              s.below == std::vector<ply::WorldMapQuad>{q(9, 1, 2, 16, 0)} && s.above.empty(),
              "A edge-touch 16x16 is one quad");
    }
    // One px more and row cy=1 enters — the same camera with height 17.
    {
        const ply::WorldMapSubmission s = a.submit({16, 0, 16, 17});
        pinAt(exactMatch, exactRan,
              s.below == std::vector<ply::WorldMapQuad>{q(5, 1, 1, 16, 16), q(9, 1, 2, 16, 0)},
              "A edge-overlap 16x17 is two quads");
    }

    // Holes: the cy=1 row has empty cells at (0,1) and (3,1) — empty cells
    // intersect the camera and submit nothing.
    {
        const ply::UIRect cam{0, 16, 64, 16};
        const ply::WorldMapSubmission s = a.submit(cam);
        pinAt(exactMatch, exactRan,
              s.below == std::vector<ply::WorldMapQuad>{q(5, 1, 1, 16, 16), q(6, 2, 1, 32, 16)} &&
                  s.above == std::vector<ply::WorldMapQuad>{q(21, 2, 1, 32, 16)},
              "A holes row: only non-empty cells");
    }

    // A sub-tile camera inside one cell.
    {
        const ply::WorldMapSubmission s = a.submit({8, 40, 2, 2});
        pinAt(exactMatch, exactRan,
              s.below == std::vector<ply::WorldMapQuad>{q(0, 0, 0, 0, 32)} && s.above.empty(),
              "A sub-tile camera: one quad");
    }

    // Off the map entirely: nothing, and the empty hash is the brute empty
    // hash (the hash commits to "nothing", not to "whatever we built").
    {
        const ply::UIRect cam{100, 100, 10, 10};
        const ply::WorldMapSubmission s = a.submit(cam);
        pinAt(exactMatch, exactRan,
              s.below.empty() && s.above.empty(), "A off-map camera: nothing");
        pinAt(exactMatch, exactRan, s.hash() == bruteHash({}, {}),
              "A off-map hash == empty brute hash");
    }

    // Negative camera coords: the map seen from outside its bottom-left.
    // The camera reaches cx=1 (x 16..20 overlap) and cy=1 (y 16..20).
    {
        const ply::UIRect cam{-20, -20, 40, 40};
        const ply::WorldMapSubmission s = a.submit(cam);
        pinAt(exactMatch, exactRan,
              s.below == std::vector<ply::WorldMapQuad>{q(5, 1, 1, 16, 16), q(8, 0, 2, 0, 0),
                                                        q(9, 1, 2, 16, 0)} &&
                  s.above.empty(),
              "A negative camera clips to the corner cells");
        pinAt(exactMatch, exactRan,
              s.below == brutePass(mapA, ts, cam, false) &&
                  s.above == brutePass(mapA, ts, cam, true),
              "A negative camera == brute");
    }

    // Fixture D: the ADR 0012 budget over one full internal-resolution view.
    const ply::MapData mapD = loadOk(48, 30, fixtureDSpecs());
    ply::WorldMap d = ply::WorldMap::build(mapD, ts);
    {
        const ply::UIRect cam{0, 0, 640, 360};
        const ply::WorldMapSubmission s = d.submit(cam);
        const std::size_t total = s.below.size() + s.above.size();
        const std::size_t brute =
            brutePass(mapD, ts, cam, false).size() + brutePass(mapD, ts, cam, true).size();
        pinAt(exactMatch, exactRan, s.below.size() == 920, "D below 40x23 == 920");
        pinAt(exactMatch, exactRan, s.above.size() == 920, "D above 40x23 == 920");
        pinAt(exactMatch, exactRan, total == 1840 && total == brute, "D total 1840 == brute");
        pinAt(exactMatch, exactRan,
              s.below == brutePass(mapD, ts, cam, false) &&
                  s.above == brutePass(mapD, ts, cam, true),
              "D full-view lists == brute");
        pinAt(exactMatch, exactRan, total <= static_cast<std::size_t>(kQuadBudget),
              "D full view under the ADR 0012 quad budget");
    }
    {
        // A camera that starts mid-tile and runs past the map's right edge.
        const ply::UIRect cam{5, 5, 640, 360};
        const ply::WorldMapSubmission s = d.submit(cam);
        pinAt(exactMatch, exactRan,
              s.below == brutePass(mapD, ts, cam, false) &&
                  s.above == brutePass(mapD, ts, cam, true),
              "D offset camera == brute");
        pinAt(exactMatch, exactRan,
              s.below.size() + s.above.size() <= static_cast<std::size_t>(kQuadBudget),
              "D offset view under the quad budget");
    }
}

// --- chunk-boundary traversal: fixture B ------------------------------------

void testBoundary() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    // Cameras around the chunk seams (x = 256 and y = 64 in world px). The
    // criterion is TRAVERSAL exactness: the same quads a brute-force scan
    // finds, none dropped and none duplicated where chunks meet. (The
    // RESOLUTION seam — autotile ids across chunk borders — is E-b3.2, b3c.)
    struct Case {
        ply::UIRect cam;
        std::size_t expectTotal;
        const char* what;
    };
    const Case cases[] = {
        {{240, 160, 32, 32}, 8, "straddle the x=256 seam (4 cells x 2 layers)"},
        {{240, 160, 16, 32}, 4, "left half up to the seam (2 cells x 2 layers)"},
        {{256, 160, 16, 32}, 4, "right half from the seam (2 cells x 2 layers)"},
        {{0, 56, 16, 16}, 4, "straddle the y=64 seam (2 cells x 2 layers)"},
        {{240, 56, 32, 16}, 8, "corner of both seams (4 cells x 2 layers)"},
    };
    std::size_t halvesSum = 0;
    for (std::size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const Case& c = cases[i];
        const ply::WorldMapSubmission s = wm.submit(c.cam);
        const std::vector<ply::WorldMapQuad> bBelow = brutePass(map, ts, c.cam, false);
        const std::vector<ply::WorldMapQuad> bAbove = brutePass(map, ts, c.cam, true);
        const std::size_t total = s.below.size() + s.above.size();
        pinAt(boundaryMatch, boundaryRan, total == c.expectTotal, c.what);
        pinAt(boundaryMatch, boundaryRan,
              s.below == bBelow && s.above == bAbove, "B seam case lists == brute");
        pinAt(boundaryMatch, boundaryRan,
              s.hash() == bruteHash(bBelow, bAbove), "B seam case hash == brute hash");
        if (i == 1 || i == 2) halvesSum += total;  // the two one-sided halves
    }
    // No gap and no duplicate at the seam: the two one-sided halves of the
    // straddle camera sum to exactly the straddling count.
    pinAt(boundaryMatch, boundaryRan, halvesSum == 8, "seam halves sum to the straddle count");

    // A camera exactly as wide as one tile ON the seam boundary pair: the
    // tile left of the seam is submitted for x<=255 only, never for x>=256.
    {
        const ply::WorldMapSubmission s = wm.submit({240, 160, 15, 32});
        pinAt(boundaryMatch, boundaryRan,
              s.below.size() + s.above.size() == 4, "seam sliver stops before x=256");
    }
    {
        const ply::WorldMapSubmission s = wm.submit({257, 160, 15, 32});
        pinAt(boundaryMatch, boundaryRan,
              s.below.size() + s.above.size() == 4, "seam sliver starts after x=256");
    }
}

// --- opaque coverage and pass assignment: fixture C -------------------------

void testOpaqueAndPasses() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(6, 2, fixtureCSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);
    const ply::UIRect cam{0, 0, 96, 32};
    const ply::WorldMapSubmission s = wm.submit(cam);

    const auto q = [&](int id, int cx, int cy) {
        return quadFor(ts, map, cx, cy, id);
    };

    // The whole fixture, in contract order (layers authored, cells row-major):
    // ground keeps only what no opaque layer above covers, slab culls ground
    // where it draws, anim (flags "animated" only) draws BELOW entities and
    // survives its own opaque slab, canopy survives the empty roof cell, and
    // roof culls ground across the pass boundary.
    const std::vector<ply::WorldMapQuad> expBelow = {
        // ground: (0,0) culled by slab, (2,0)+(3,0) by roof/slab, (0,1) by roof.
        q(2, 1, 0), q(5, 4, 0), q(6, 5, 0), q(8, 1, 1), q(9, 2, 1), q(10, 3, 1), q(11, 4, 1),
        q(12, 5, 1),
        // slab: opaque, draws over ground; nothing later covers (0,0)/(3,0).
        q(13, 0, 0), q(14, 3, 0),
        // anim: flags "animated" only -> below-entities pass; the slab before
        // it does not cull it (opaque never culls what draws after it).
        q(15, 0, 0), q(16, 1, 0),
    };
    const std::vector<ply::WorldMapQuad> expAbove = {
        q(17, 0, 0),  // canopy: roof is EMPTY at (0,0) -> nothing covered
        q(18, 2, 0),  // roof
        q(19, 0, 1),
    };
    pinAt(opaqueMatch, opaqueRan, s.below == expBelow, "C below literal (coverage applied)");
    pinAt(opaqueMatch, opaqueRan, s.above == expAbove, "C above literal");

    // The coverage rules, named pin by named pin.
    const std::vector<int> belowIds = idsOf(s.below);
    const std::vector<int> aboveIds = idsOf(s.above);
    const auto has = [](const std::vector<int>& v, int id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    };
    pinAt(opaqueMatch, opaqueRan, !has(belowIds, 1),
          "slab opaque culls ground at (0,0) in-pass");
    pinAt(opaqueMatch, opaqueRan, !has(belowIds, 4),
          "slab opaque culls ground at (3,0) in-pass");
    pinAt(opaqueMatch, opaqueRan, !has(belowIds, 3),
          "roof opaque culls ground at (2,0) across passes");
    pinAt(opaqueMatch, opaqueRan, !has(belowIds, 7),
          "roof opaque culls ground at (0,1) across passes");
    pinAt(opaqueMatch, opaqueRan, has(belowIds, 15) && has(belowIds, 16),
          "slab does not cull anim drawn after it");
    pinAt(opaqueMatch, opaqueRan, has(belowIds, 2),
          "empty slab cell covers nothing (ground survives at (1,0))");
    pinAt(opaqueMatch, opaqueRan, has(aboveIds, 17),
          "empty roof cell covers nothing (canopy survives at (0,0))");
    pinAt(opaqueMatch, opaqueRan, has(belowIds, 13) && has(belowIds, 14),
          "the opaque slab itself draws (culls below, not itself)");

    // == brute over the same camera, so coverage is exactly the rule above.
    pinAt(opaqueMatch, opaqueRan,
          s.below == brutePass(map, ts, cam, false) && s.above == brutePass(map, ts, cam, true),
          "C lists == brute");
    pinAt(opaqueMatch, opaqueRan,
          s.hash() == bruteHash(brutePass(map, ts, cam, false), brutePass(map, ts, cam, true)),
          "C hash == brute hash");

    // Pass assignment, named: "animated" alone has no above bit and draws
    // below (MapLayer::aboveEntities() is the selector); canopy/roof draw
    // above; everything below precedes everything above by construction.
    pinAt(passMatch, passRan, has(belowIds, 15) && !has(aboveIds, 15),
          "flags 'animated' only -> below pass");
    pinAt(passMatch, passRan, has(aboveIds, 17) && !has(belowIds, 17),
          "canopy (above+animated) -> above pass");
    pinAt(passMatch, passRan, has(aboveIds, 18) && has(aboveIds, 19),
          "roof (above+opaque) -> above pass");
    pinAt(passMatch, passRan, s.below.size() == 12 && s.above.size() == 3,
          "pass counts 12 below / 3 above");

    // A hand-built layer with flags 0 (never document-reachable: the loader
    // defaults to below-entities) still draws below — the selector rule.
    {
        ply::MapData raw;
        raw.width = 2;
        raw.height = 1;
        ply::MapLayer layer;
        layer.name = "raw";
        layer.tiles = {30, 31};
        layer.flags = 0;
        raw.layers.push_back(layer);
        ply::WorldMap rawWm = ply::WorldMap::build(raw, ts);
        const ply::WorldMapSubmission rs = rawWm.submit({0, 0, 32, 16});
        pinAt(passMatch, passRan,
              idsOf(rs.below) == std::vector<int>({30, 31}) && rs.above.empty(),
              "hand-built flags 0 -> below pass");
    }

    // Two identical layers differing only in the below/above token land in
    // different passes — the flags, not the name or order, decide.
    {
        const ply::MapData under = loadOk(2, 1, {{"x", "[\"below-entities\"]", {30, 31}}});
        const ply::MapData over = loadOk(2, 1, {{"x", "[\"above-entities\"]", {30, 31}}});
        const ply::WorldMapSubmission us =
            ply::WorldMap::build(under, ts).submit({0, 0, 32, 16});
        const ply::WorldMapSubmission os =
            ply::WorldMap::build(over, ts).submit({0, 0, 32, 16});
        pinAt(passMatch, passRan,
              idsOf(us.below) == std::vector<int>({30, 31}) && us.above.empty(),
              "same tiles, below token -> below pass");
        pinAt(passMatch, passRan,
              os.below.empty() && idsOf(os.above) == std::vector<int>({30, 31}),
              "same tiles, above token -> above pass");
        pinAt(passMatch, passRan, us.hash() != os.hash(),
              "pass split is committed in the hash");
    }
}

// --- per-chunk dirty flags --------------------------------------------------

void testDirtyFlags() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);

    const ply::UIRect whole{0, 0, 528, 320};
    const ply::UIRect corner{0, 300, 16, 16};  // inside chunk (0,0) only

    // At build time every chunk's derived lists are missing: all dirty.
    bool allDirty = true;
    for (int cy = 0; cy < 2; ++cy)
        for (int cx = 0; cx < 3; ++cx) allDirty = allDirty && wm.chunkDirty(cx, cy);
    pinAt(dirtyMatch, dirtyRan, allDirty, "all chunks dirty at build");

    pinAt(dirtyMatch, dirtyRan, wm.submit(whole).below.size() > 0, "whole-map submit draws");
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 6, "first submit builds 6 chunks");
    wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 6, "repeat submit rebuilds nothing");

    wm.setTile(0, 15, 15, 33);
    pinAt(dirtyMatch, dirtyRan, wm.chunkDirty(0, 0), "setTile (15,15) dirties chunk (0,0)");
    pinAt(dirtyMatch, dirtyRan,
          !wm.chunkDirty(1, 0) && !wm.chunkDirty(0, 1) && !wm.chunkDirty(2, 1),
          "and only that chunk");
    wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 7, "one edit -> one rebuild");

    wm.setTile(0, 16, 16, 34);
    pinAt(dirtyMatch, dirtyRan,
          wm.chunkDirty(1, 1) && !wm.chunkDirty(0, 0) && !wm.chunkDirty(1, 0),
          "setTile (16,16) dirties the (1,1) chunk across the seam");
    wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 8, "seam-crossing edit -> one rebuild");

    wm.setTile(0, 17, 16, 35);
    wm.setTile(0, 18, 16, 36);
    wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 9, "two edits in one chunk -> one rebuild");

    // A dirty chunk outside the camera is not built: culling is lazy.
    wm.setTile(0, 32, 0, 37);  // chunk (2,0)
    wm.submit(corner);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 9, "dirty chunk outside camera not built");
    pinAt(dirtyMatch, dirtyRan, wm.chunkDirty(2, 0), "it stays dirty until asked for");
    wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan, wm.chunkBuilds() == 10, "whole-map submit rebuilds it once");

    // The edits themselves: reads reflect writes across the seams.
    pinAt(dirtyMatch, dirtyRan,
          wm.tileAt(0, 15, 15) == 33 && wm.tileAt(0, 16, 16) == 34 && wm.tileAt(0, 32, 0) == 37,
          "setTile writes land in the right chunks");
    const ply::WorldMapSubmission s = wm.submit(whole);
    pinAt(dirtyMatch, dirtyRan,
          hasId(s.below, 33) && hasId(s.below, 34) && hasId(s.below, 37),
          "the new tiles submit after their rebuild");
}

// --- structural hash (ADR 0014) --------------------------------------------

std::uint64_t fixtureBHash() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);
    return wm.submit({0, 0, 528, 320}).hash();
}

void testHash() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wm = ply::WorldMap::build(map, ts);
    const ply::UIRect cam{0, 0, 528, 320};

    const std::uint64_t first = wm.submit(cam).hash();
    pinAt(hashMatch, hashRan, wm.submit(cam).hash() == first, "hash reproducible on resubmit");

    const std::uint64_t expected =
        bruteHash(brutePass(map, ts, cam, false), brutePass(map, ts, cam, true));
    pinAt(hashMatch, hashRan, first == expected, "B whole-map hash == brute hash");

    // One tile's id changes -> the hash moves; restore -> it returns.
    wm.setTile(0, 4, 4, 38);
    pinAt(hashMatch, hashRan, wm.submit(cam).hash() != first, "hash moves on a changed tile");
    wm.setTile(0, 4, 4, 1 + ((4 * 7 + 4 * 13) % 39));
    pinAt(hashMatch, hashRan, wm.submit(cam).hash() == first, "hash returns on restore");

#if PLY_HAVE_SELF_EXE
    // The same hash from a second process (E-a4.3's precedent): structural
    // hashes must not fold a host's std::hash or padding into the value.
    {
        char path[4096];
        const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (len > 0) {
            path[len] = '\0';
            const std::string command =
                std::string("'") + path + "' --worldmap-hash-child";
            FILE* pipe = popen(command.c_str(), "r");
            unsigned long long childHash = 0;
            const int read = pipe ? std::fscanf(pipe, "WORLDMAP_HASH %llx", &childHash) : 0;
            const int status = pipe ? pclose(pipe) : -1;
            pinAt(hashMatch, hashRan,
                  read == 1 && status == 0 && childHash == first,
                  "hash identical in a second process");
        } else {
            pinAt(hashMatch, hashRan, false, "second process hash (no /proc/self/exe)");
        }
    }
#endif
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData mapB = loadOk(33, 20, fixtureBSpecs());
    ply::WorldMap wmB = ply::WorldMap::build(mapB, ts);
    const ply::WorldMapSubmission whole = wmB.submit({0, 0, 528, 320});

    const ply::MapData mapD = loadOk(48, 30, fixtureDSpecs());
    ply::WorldMap wmD = ply::WorldMap::build(mapD, ts);
    const ply::WorldMapSubmission view = wmD.submit({0, 0, 640, 360});
    const std::size_t budget = view.below.size() + view.above.size();

    std::printf(
        "EVIDENCE worldmap cells=%dx%d chunks=%zu store=%d/%d exact=%d/%d boundary=%d/%d "
        "opaque=%d/%d pass=%d/%d dirty=%d/%d hashpin=%d/%d budget=%zu/%d hash=%016llx\n",
        wmB.width(), wmB.height(), wmB.chunkCount(), storeMatch, storeRan, exactMatch, exactRan,
        boundaryMatch, boundaryRan, opaqueMatch, opaqueRan, passMatch, passRan, dirtyMatch,
        dirtyRan, hashMatch, hashRan, budget, kQuadBudget,
        static_cast<unsigned long long>(whole.hash()));
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--worldmap-hash-child") {
        std::printf("WORLDMAP_HASH %016llx\n",
                    static_cast<unsigned long long>(fixtureBHash()));
        return 0;
    }

    testStorage();
    testExactCounts();
    testBoundary();
    testOpaqueAndPasses();
    testDirtyFlags();
    testHash();

    if (failures == 0) {
        printEvidence();
        std::printf("worldmap tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "worldmap tests: %d failure(s)\n", failures);
    return 1;
}
