// Headless map-loader tests (M1a a9): E-a9.1 — map load → save → load
// identical (byte-identical through canonical emit), and the validator
// rejects and names out-of-range tile ids and missing tileset refs — plus the
// world-quad emission whose row flip puts the authored top row at the top of
// the y-up world, graded structurally as "static tiles behind the HUD" via
// a7's SceneRenderer layer order (the HUD is above the upscale and outside
// this scene list, by a7's policy).
//
// ADR 0003: native JSON, no Tiled runtime dependency. Pure data: no texture,
// no GL, no window, no clock.

#include "plyEngine/MapLoader.hpp"
#include "plyEngine/RenderTarget.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

void eq(long long got, long long want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got %lld, want %lld)\n", what, got, want);
        ++failures;
    }
}

void eqRect(const ply::UIRect& got, const ply::UIRect& want, const char* what) {
    if (!(got == want)) {
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d,%d,%d}, want {%d,%d,%d,%d})\n", what, got.x,
                     got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        ++failures;
    }
}

void eqStr(const std::string& got, const std::string& want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got '%s', want '%s')\n", what, got.c_str(),
                     want.c_str());
        ++failures;
    }
}

void eqHash(std::uint64_t got, std::uint64_t want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got 0x%016llx, want 0x%016llx)\n", what,
                     static_cast<unsigned long long>(got),
                     static_cast<unsigned long long>(want));
        ++failures;
    }
}

using ply::MapData;
using ply::MapError;
using ply::MapLoadResult;
using ply::TilesetSlice;
using ply::UIRect;

std::string readFixture(const char* name) {
    std::ifstream in(std::string(PLY_TEST_DATA_DIR) + "/" + name);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

const MapData& fixtureMap() {
    static const MapData map = ply::loadMapFromString(readFixture("map_small.json")).map;
    return map;
}

const TilesetSlice& fixtureTileset() {
    static const TilesetSlice tileset =
        ply::loadTilesetFromString(readFixture("tileset_static.json")).tileset;
    return tileset;
}

// --- the fixtures themselves ----------------------------------------------

void testFixtures() {
    const ply::TilesetLoadResult t =
        ply::loadTilesetFromString(readFixture("tileset_static.json"));
    check(t.ok(), "the tileset fixture loads clean");
    eqStr(t.tileset.name, "static", "tileset name");
    eqStr(t.tileset.image, "tileset_static.png", "tileset image");
    eq(t.tileset.tileWidth, 16, "tile width");
    eq(t.tileset.tileHeight, 16, "tile height");
    eq(t.tileset.tiles.size(), 4, "4 tiles");
    // Sorted by id; regions and solid flags pinned as literals. Guarded so a
    // broken fixture reports failures instead of indexing an empty vector
    // (the a5.1 lesson: an unguarded index turns a red run into a crash).
    if (t.tileset.tiles.size() >= 4) {
        eq(t.tileset.tiles[0].id, 0, "tiles sort by id (0)");
        eqRect(t.tileset.tiles[0].region, UIRect{0, 0, 16, 16}, "tile 0 region");
        eq(t.tileset.tiles[0].solid, 0, "tile 0 is not solid");
        eqRect(t.tileset.tiles[1].region, UIRect{16, 0, 16, 16}, "tile 1 region");
        eq(t.tileset.tiles[1].solid, 1, "tile 1 is solid");
        eq(t.tileset.tiles[2].solid, 1, "tile 2 is solid");
        eqRect(t.tileset.tiles[3].region, UIRect{0, 16, 16, 16}, "tile 3 region");
        eq(t.tileset.tiles[3].solid, 0, "tile 3 is not solid");
    }
    check(t.tileset.findTile(2) != nullptr, "findTile finds id 2");
    check(t.tileset.findTile(4) == nullptr, "findTile misses id 4");
    check(t.tileset.findTile(-1) == nullptr, "findTile misses empty");

    const MapLoadResult m = ply::loadMapFromString(readFixture("map_small.json"));
    check(m.ok(), "the map fixture loads clean");
    eqStr(m.map.name, "small", "map name");
    eqStr(m.map.tileset, "static", "map tileset reference");
    eq(m.map.width, 4, "map width");
    eq(m.map.height, 3, "map height");
    eq(m.map.layers.size(), 1, "one layer");
    if (!m.map.layers.empty() && m.map.layers[0].tiles.size() == 12) {
        eqStr(m.map.layers[0].name, "ground", "layer name");
        eq(m.map.layers[0].tiles[4], -1,
           "a negative cell id normalises to -1 (fixture wrote -7)");
        eq(m.map.layers[0].tiles[5], 3, "cell (1,1) keeps its id");
        eq(m.map.layers[0].tiles[6], -1, "empty cell stays empty");
    }
    eq(m.map.layers.empty() ? 0 : (long long)m.map.layers[0].tiles.size(), 12,
       "layer holds width*height ids");
}

// --- world quads: the row flip --------------------------------------------

std::vector<UIRect> quads() { return ply::mapTileQuads(fixtureMap(), fixtureTileset()); }

void testWorldQuads() {
    const std::vector<UIRect> q = quads();
    eq(q.size(), 9, "9 non-empty cells emit 9 quads (3 empties emit nothing)");

    if (q.size() >= 9) {
        // File order is row-major from the TOP row; the world is y-up, so the
        // top row lands at the highest world y. Pinned as literals at all
        // four corners and the middle: an unflipped loader swaps top and
        // bottom. Guarded (a5.1 lesson) so a stub reports, not crashes.
        eqRect(q[0], UIRect{0, 32, 16, 16}, "top-left cell (0,0) -> world y 32");
        eqRect(q[3], UIRect{48, 32, 16, 16}, "top-right cell (3,0) -> world y 32");
        eqRect(q[4], UIRect{16, 16, 16, 16}, "middle cell (1,1) -> world y 16");
        eqRect(q[5], UIRect{0, 0, 16, 16}, "bottom-left cell (0,2) -> world y 0");
        eqRect(q[8], UIRect{48, 0, 16, 16}, "bottom-right cell (3,2) -> world y 0");

        // Emission order is file order (the top row's four quads first).
        eqRect(q[1], UIRect{16, 32, 16, 16}, "file order keeps the row together");
    }

    // Structural hash over the quad list (a4's rect hash): pinned by literal.
    eqHash(ply::structuralHash(q), 0x7e90f4655928d1acull, "tile quad hash is pinned");
}

// --- E-a9.1: load -> save -> load identical --------------------------------

void testRoundTrip() {
    const MapLoadResult first = ply::loadMapFromString(readFixture("map_small.json"));
    const std::string saved1 = ply::emitMap(first.map);
    const MapLoadResult second = ply::loadMapFromString(saved1);
    const std::string saved2 = ply::emitMap(second.map);

    check(first.map == second.map, "load -> save -> load is structurally identical");
    eqStr(saved2, saved1, "load -> save -> load is byte-identical through canonical emit");
    const std::string saved3 = ply::emitMap(ply::loadMapFromString(saved2).map);
    eqStr(saved3, saved2, "emit is a fixed point");

    // Key order in the source document must not matter: the same map with
    // its keys shuffled emits identical bytes.
    const char* shuffled = R"({"layers": [{"tiles": [0, 1, 2, 0, -7, 3, -1, -1, 0, 0, 0, 0],
        "name": "ground"}], "height": 3, "name": "small", "width": 4, "tileset": "static",
        "an_unknown_future_key": {"props": []}})";
    eqStr(ply::emitMap(ply::loadMapFromString(shuffled).map), saved1,
          "key order is irrelevant and unknown keys are ignored");
}

// --- the validator and its named errors ------------------------------------

enum class Kind { MapDoc, TilesetDoc, MapFile, Validate };

struct ErrorCase {
    Kind kind;
    const char* payload;
    const char* want;
};

void testNamedErrors() {
    static const ErrorCase kCases[] = {
        {Kind::MapDoc, R"({"name":"m","tileset":"static","height":3,"layers":[]})", "MissingField"},
        {Kind::MapDoc, R"({"name":"m","tileset":"static","width":"four","height":3,"layers":[]})",
         "WrongType"},
        {Kind::MapDoc, R"({"name":"m","tileset":"static","width":0,"height":3,"layers":[]})",
         "NonPositiveSize"},
        {Kind::MapDoc,
         R"({"name":"m","tileset":"static","width":1,"height":1,"layers":[{"name":"","tiles":[0]}]})",
         "EmptyLayerName"},
        {Kind::MapDoc,
         R"({"name":"m","tileset":"static","width":1,"height":1,)"
         R"("layers":[{"name":"ground","tiles":[0]},{"name":"ground","tiles":[0]}]})",
         "DuplicateLayer"},
        {Kind::MapDoc,
         R"({"name":"m","tileset":"static","width":4,"height":3,)"
         R"("layers":[{"name":"ground","tiles":[0,0,0,0,0,0,0,0,0,0,0]}]})",
         "LayerSizeMismatch"},
        {Kind::MapDoc, "{ not json", "MalformedJson"},
        {Kind::MapFile, "/definitely/not/a/map.json", "FileNotReadable"},
        {Kind::TilesetDoc, R"({"name":"","image":"t.png","tile_width":16,"tile_height":16,"tiles":[]})",
         "EmptyTilesetName"},
        {Kind::TilesetDoc,
         R"({"name":"t","image":"t.png","tile_width":16,"tile_height":16,)"
         R"("tiles":[{"id":1,"region":{"x":0,"y":0,"w":16,"h":16}},)"
         R"({"id":1,"region":{"x":16,"y":0,"w":16,"h":16}}]})",
         "DuplicateTileId"},
        {Kind::TilesetDoc,
         R"({"name":"t","image":"t.png","tile_width":16,"tile_height":16,)"
         R"("tiles":[{"id":0,"region":{"x":0,"y":0,"w":0,"h":16}}]})",
         "NegativeRegion"},
        {Kind::TilesetDoc, R"({"name":"t","image":"t.png","tile_width":16,"tile_height":0,"tiles":[]})",
         "NonPositiveSize"},
        // Validator: the map's tileset reference must resolve.
        {Kind::Validate, "", "MissingTileset"},
        {Kind::Validate, "renamed", "MissingTileset"},
        // Validator: an id equal to the tile count is OUT of range — the
        // boundary pin that catches >= vs >.
        {Kind::Validate, "id-4", "OutOfRangeTileId"},
        {Kind::Validate, "id-9", "OutOfRangeTileId"},
    };

    long long ran = 0;
    for (const ErrorCase& c : kCases) {
        MapError error;
        switch (c.kind) {
            case Kind::MapDoc: error = ply::loadMapFromString(c.payload).error; break;
            case Kind::TilesetDoc: error = ply::loadTilesetFromString(c.payload).error; break;
            case Kind::MapFile: {
                const std::string path = std::string(PLY_TEST_DATA_DIR) + c.payload;
                error = ply::loadMapFile(path).error;
                break;
            }
            case Kind::Validate: {
                MapData map = fixtureMap();
                std::vector<TilesetSlice> registered;
                if (!map.layers.empty() && std::string(c.payload) == "id-4")
                    map.layers[0].tiles[5] = 4;
                if (!map.layers.empty() && std::string(c.payload) == "id-9")
                    map.layers[0].tiles[5] = 9;
                if (std::string(c.payload) == "renamed") {
                    TilesetSlice renamed = fixtureTileset();
                    renamed.name = "other";
                    registered.push_back(renamed);
                } else if (std::string(c.payload) == "id-4" ||
                           std::string(c.payload) == "id-9") {
                    registered.push_back(fixtureTileset());
                }
                error = ply::validateMap(map, registered);
                break;
            }
        }
        ++ran;
        if (!error.ok() && std::string(error.name()) == c.want) continue;
        std::fprintf(stderr, "  FAIL: error case '%s' (got %s, want %s)\n", c.payload,
                     error.ok() ? "None" : error.name(), c.want);
        ++failures;
    }
    eq(ran, 16, "the named-error table ran 16 cases (one loop)");
    // The happy path is not an error: the fixture map against its tileset.
    check(ply::validateMap(fixtureMap(), {fixtureTileset()}).ok(),
          "the fixture map lints clean against its tileset");
}

// --- "static tiles render behind the HUD" ---------------------------------

void testScenePlacement() {
    ply::SceneRenderer renderer;
    renderer.submitLayer(ply::SceneLayer::GroundTiles, quads());
    renderer.submit(ply::SceneLayer::World, UIRect{0, 0, 8, 8});
    renderer.submit(ply::SceneLayer::Overhead, UIRect{4, 4, 2, 2});
    const ply::SceneSubmission scene = renderer.submission(1);

    eq(scene.layers.size(), 3, "three layers in the submission");
    // GroundTiles is a7's bottom layer: the tiles sit under World/Overhead,
    // and the HUD is drawn after the upscale, outside this list entirely.
    check(scene.layers.front().layer == ply::SceneLayer::GroundTiles,
          "static tiles are the bottom (first) scene layer");
    eq(scene.layers.front().quads.size(), 9, "all 9 tile quads land in GroundTiles");
}

// One line of measured values, printed so the ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence() {
    const TilesetSlice& t = fixtureTileset();
    const MapData& m = fixtureMap();
    const std::vector<UIRect> q = quads();
    const std::string saved = ply::emitMap(m);

    std::printf(
        "EVIDENCE map_loader tiles=%zu/%d map=%dx%d layers=%zu quads=%zu empty=%zu "
        "corners=%d,%d/%d,%d/%d,%d/%d,%d mid=%d,%d roundtrip=%s errors=16 ground=%zu/first "
        "map_hash=0x%016llx\n",
        t.tiles.size(), t.tiles[1].solid + t.tiles[2].solid, m.width, m.height, m.layers.size(),
        q.size(), m.layers[0].tiles.size() - q.size(), q[0].x, q[0].y, q[3].x, q[3].y, q[5].x,
        q[5].y, q[8].x, q[8].y, q[4].x, q[4].y,
        ply::emitMap(ply::loadMapFromString(saved).map) == saved ? "identical" : "DIFFER",
        q.size(),
        static_cast<unsigned long long>(ply::structuralHash(q)));
}

}  // namespace

int main() {
    testFixtures();
    testWorldQuads();
    testRoundTrip();
    testNamedErrors();
    testScenePlacement();

    if (failures == 0) {
        if (quads().size() == 9 && fixtureTileset().tiles.size() == 4) {
            printEvidence();
        } else {
            std::fprintf(stderr, "  FAIL: evidence preconditions\n");
            return 1;
        }
        std::printf("map_loader tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "map_loader tests: %d failure(s)\n", failures);
    return 1;
}
