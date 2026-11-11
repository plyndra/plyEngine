// TileSet metadata schema (M1b b1.1, E-b1.1): the b1 slice of the native
// JSON tileset format (ADR 0003, ARCHITECTURE_TARGET.md §3/§4) — the a9
// slice's regions + solid flags extended with the per-tile metadata the
// roadmap names: autotile mask, variant group, animation frames + steps,
// per-tile phase offset, tile-local collision rects, palette id.
//
// The house rules for this suite: pure data — no GL context, no window, no
// clock, no float. The fixture tileset is hand-authored (the roadmap's
// "written by hand"), with its tiles deliberately out of id order, one tile
// carrying an unknown future key, and one tile carrying no metadata at all,
// so every load-time rule below is exercised against a document that would
// break if the rule were dropped.

#include "plyEngine/MapLoader.hpp"
#include "plyEngine/TileSet.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "  FAIL: %s\n", what);
    }
}

const std::string& fixtureDocument() {
    static const std::string doc = [] {
        const std::string path = std::string(PLY_TEST_DATA_DIR) + "/tileset_village.json";
        const ply::TileSetLoadResult loaded = ply::loadTileSetFile(path);
        // The fixture must exist on disk for any pin below to mean anything;
        // a missing file reports FileNotReadable and the suite fails here
        // rather than vacuously.
        if (!loaded.ok()) {
            std::fprintf(stderr, "  FAIL: fixture tileset does not load (%s)\n",
                         loaded.error.message.c_str());
        }
        return ply::emitTileSet(loaded.tileset);
    }();
    return doc;
}

// --- the fixture's parsed model -------------------------------------------

void testFixtureModel() {
    const ply::TileSetLoadResult loaded = ply::loadTileSetFromString(fixtureDocument());
    check(loaded.ok(), "fixture loads");
    // The guard is on the tile count, not only ok(): an ok()-with-empty-model
    // stub must fail its pins, not walk an empty vector (the a5.1 lesson).
    if (!loaded.ok() || loaded.tileset.tiles.size() != 11) return;
    const ply::TileSet& ts = loaded.tileset;

    check(ts.name == "village", "name");
    check(ts.image == "tileset_village.png", "image");
    check(ts.tileWidth == 16 && ts.tileHeight == 16, "tile size");
    check(ts.tiles.size() == 11, "11 tiles");

    // Sorted by id at load; the fixture is authored out of order on purpose.
    bool sorted = true;
    for (std::size_t i = 1; i < ts.tiles.size(); ++i) {
        sorted = sorted && ts.tiles[i - 1].id < ts.tiles[i].id;
    }
    check(sorted, "tiles sorted by id");
    for (int id = 0; id <= 10; ++id) {
        check(ts.tiles[static_cast<std::size_t>(id)].id == id, "id sequence 0..10");
    }

    // id 1: the full grass edge tile, pinned field by field.
    const ply::TileData& t1 = ts.tiles[1];
    check(t1.region.x == 16 && t1.region.y == 0 && t1.region.w == 16 && t1.region.h == 16,
          "id 1 region");
    check(!t1.solid, "id 1 not solid");
    check(t1.mask == 5, "id 1 mask (N+E same-terrain)");
    check(t1.variant == "grass", "id 1 variant");
    check(t1.palette == "overworld", "id 1 palette");
    check(t1.animation.empty(), "id 1 static art");
    check(t1.phase == 0, "id 1 phase default");
    check(t1.collision.empty(), "id 1 no partial collision");

    // The other grass masks are pinned literals, each a distinct blob case.
    check(ts.tiles[0].mask == 0, "id 0 mask (isolated)");
    check(ts.tiles[2].mask == 16, "id 2 mask (S only)");
    check(ts.tiles[3].mask == 31, "id 3 mask (N,NE,E,SE,S)");
    check(ts.tiles[4].mask == 255, "id 4 mask (all neighbours same)");

    // id 5: no metadata at all — every default in one place.
    const ply::TileData& t5 = ts.tiles[5];
    check(t5.solid, "id 5 solid");
    check(t5.mask == ply::kNoTileMask && t5.mask == -1, "id 5 no autotile role");
    check(t5.variant.empty() && t5.palette.empty(), "id 5 no variant, no palette");
    check(t5.animation.empty() && t5.collision.empty() && t5.phase == 0, "id 5 no metadata");

    // id 6: two partial collision rects, tile-local, in authored order.
    const ply::TileData& t6 = ts.tiles[6];
    check(t6.solid && t6.collision.size() == 2, "id 6 two collision rects");
    check(t6.collision[0].x == 0 && t6.collision[0].y == 0 && t6.collision[0].w == 16 &&
              t6.collision[0].h == 4,
          "id 6 first collision rect");
    check(t6.collision[1].x == 0 && t6.collision[1].y == 12 && t6.collision[1].w == 16 &&
              t6.collision[1].h == 4,
          "id 6 second collision rect");

    // id 7: palette only (the cave tile shares no variant, no mask), and it
    // carries NO solid key — the default must be not solid, pinned here
    // because every other solid pin in this suite reads an explicit key.
    check(ts.tiles[7].palette == "cave" && ts.tiles[7].variant.empty() &&
              ts.tiles[7].mask == ply::kNoTileMask,
          "id 7 palette-only tile");
    check(!ts.tiles[7].solid && !ts.tiles[8].solid && !ts.tiles[10].solid,
          "absent solid key defaults to not solid");

    // id 8: animated water — four frames, each held 12 sim steps, phase 3.
    const ply::TileData& t8 = ts.tiles[8];
    check(t8.animation.size() == 4, "id 8 four frames");
    bool steps = true;
    for (const ply::TileAnimFrame& f : t8.animation) steps = steps && f.steps == 12;
    check(steps, "id 8 every frame 12 steps");
    check(t8.animation[0].region.x == 0 && t8.animation[1].region.x == 16 &&
              t8.animation[2].region.x == 32 && t8.animation[3].region.x == 48,
          "id 8 frame regions in strip order");
    check(t8.animation[0].region.y == 32 && t8.animation[0].region.w == 16 &&
              t8.animation[0].region.h == 16,
          "id 8 frame region geometry");
    check(t8.phase == 3, "id 8 phase offset");
    check(t8.variant == "water", "id 8 variant");

    // id 9: a solid fence whose blocking part is the bottom strip.
    const ply::TileData& t9 = ts.tiles[9];
    check(t9.solid && t9.collision.size() == 1 && t9.collision[0].y == 10 &&
              t9.collision[0].h == 6,
          "id 9 fence collision");

    // Lookup.
    check(ts.findTile(8) != nullptr && ts.findTile(11) == nullptr, "findTile");

    // The unknown future key on id 10 is ignored, not an error, and dropped
    // by emit (a newer document loads in this build; emit writes what the
    // model knows).
    check(ts.tiles[10].mask == 5, "id 10 loads with its metadata");
}

// --- round-trip ------------------------------------------------------------

void testRoundTrip() {
    const ply::TileSet first = ply::loadTileSetFromString(fixtureDocument()).tileset;
    check(!first.tiles.empty(), "fixture model for round-trip");
    if (first.tiles.empty()) return;

    const std::string emitted = ply::emitTileSet(first);
    const ply::TileSet second = ply::loadTileSetFromString(emitted).tileset;
    check(second == first, "load → emit → load is model-identical");
    // Emit is a fixed point: emitting the reloaded document changes nothing.
    check(ply::emitTileSet(second) == emitted, "emit is a fixed point");
    // And the document bytes survive one more cycle untouched.
    check(ply::emitTileSet(ply::loadTileSetFromString(ply::emitTileSet(second)).tileset) ==
              ply::emitTileSet(second),
          "emit → load → emit byte-identical");

    // Shuffled key order cannot matter: the fixture itself shuffles keys per
    // tile (id 3 leads with region, id 9 leads with collision) and shuffles
    // tile order too, so the sorted-by-id load already proves key and array
    // order are data, not contract.
}

// --- a9 compatibility ------------------------------------------------------

void testA9Compatibility() {
    // The a9 slice document loads through the b1 loader with every metadata
    // field at its default...
    const std::string a9doc =
        "{\n"
        "  \"name\": \"static\",\n"
        "  \"image\": \"tileset_static.png\",\n"
        "  \"tile_width\": 16,\n"
        "  \"tile_height\": 16,\n"
        "  \"tiles\": [\n"
        "    {\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"solid\": false},\n"
        "    {\"id\": 1, \"region\": {\"x\": 16, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"solid\": true},\n"
        "    {\"id\": 2, \"region\": {\"x\": 32, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"solid\": true},\n"
        "    {\"id\": 3, \"region\": {\"x\": 0, \"y\": 16, \"w\": 16, \"h\": 16}, "
        "\"solid\": false}\n"
        "  ]\n"
        "}\n";
    const ply::TileSetLoadResult loaded = ply::loadTileSetFromString(a9doc);
    check(loaded.ok(), "a9 slice loads through the b1 loader");
    check(loaded.tileset.tiles.size() == 4, "a9 slice tile count");
    if (!loaded.ok() || loaded.tileset.tiles.size() != 4) return;
    check(loaded.tileset.tiles[1].solid && loaded.tileset.tiles[1].region.x == 16 &&
              loaded.tileset.tiles[1].region.y == 0,
          "a9 tile 1");
    check(loaded.tileset.tiles[1].mask == ply::kNoTileMask &&
              loaded.tileset.tiles[1].variant.empty() &&
              loaded.tileset.tiles[1].animation.empty() &&
              loaded.tileset.tiles[1].collision.empty() &&
              loaded.tileset.tiles[1].palette.empty() && loaded.tileset.tiles[1].phase == 0,
          "a9 slice carries no b1 metadata");

    // ...and the b1 emit of a defaults-only tileset is exactly the a9
    // grammar, so the a9 loader still parses what b1 writes (and the a9
    // model round-trips through both loaders unchanged).
    const std::string emitted = ply::emitTileSet(loaded.tileset);
    // The a9-grammar emission is pinned on the BYTES: a defaults-only
    // tileset emits none of the b1 metadata keys (else the a9 build's
    // documents would grow keys it must keep ignoring forever).
    for (const char* key : {"mask", "variant", "animation", "phase", "collision", "palette"}) {
        check(emitted.find(key) == std::string::npos,
              (std::string("a9 emission carries no '") + key + "' key").c_str());
    }
    const ply::TilesetLoadResult viaA9 = ply::loadTilesetFromString(emitted);
    check(viaA9.ok(), "b1 emit of a defaults-only tileset parses as the a9 slice");
    check(viaA9.tileset.tiles.size() == 4 && viaA9.tileset.name == "static" &&
              viaA9.tileset.tileWidth == 16 && viaA9.tileset.tileHeight == 16,
          "a9 model survives b1 emit → a9 load");

    // The committed a9 fixture file also loads through the b1 loader.
    const ply::TileSetLoadResult committed =
        ply::loadTileSetFile(std::string(PLY_TEST_DATA_DIR) + "/tileset_static.json");
    check(committed.ok() && committed.tileset.tiles.size() == 4,
          "committed a9 fixture loads through the b1 loader");
}

// --- the canonical blob-47 table (b1.2) ------------------------------------

void testCanonicalMasks() {
    // The table is pinned by its characterization, re-derived HERE in the
    // grader independently of the header's data: exactly the masks in 0..255
    // where every set diagonal bit implies both adjacent edge bits. A wrong
    // or incomplete hardcoded table fails this identity, not just a count.
    std::vector<int> derived;
    for (int m = 0; m < 256; ++m) {
        const bool ne = (m & 2) != 0 && !(m & 1 && m & 4);
        const bool se = (m & 8) != 0 && !(m & 4 && m & 16);
        const bool sw = (m & 32) != 0 && !(m & 16 && m & 64);
        const bool nw = (m & 128) != 0 && !(m & 64 && m & 1);
        if (!(ne || se || sw || nw)) derived.push_back(m);
    }
    check(derived.size() == 47, "the characterization yields 47 masks");
    check(static_cast<int>(derived.size()) == ply::kBlobMaskCount,
          "kBlobMaskCount is 47");

    bool identical = static_cast<int>(derived.size()) == ply::kBlobMaskCount;
    for (int i = 0; identical && i < ply::kBlobMaskCount; ++i) {
        identical = ply::kBlobMasks[i] == derived[static_cast<std::size_t>(i)];
    }
    check(identical, "kBlobMasks == the corner-constraint set, element by element");

    bool sorted = true;
    for (int i = 1; i < ply::kBlobMaskCount; ++i) {
        sorted = sorted && ply::kBlobMasks[i - 1] < ply::kBlobMasks[i];
    }
    check(sorted, "kBlobMasks strictly ascending (binary-searchable)");

    // Spread pins: the empty blob and the full blob are in; representative
    // edge-only and diagonal cases are in at their exact values.
    check(ply::kBlobMasks[0] == 0, "isolated blob is canonical");
    check(ply::kBlobMasks[ply::kBlobMaskCount - 1] == 255,
          "full blob is canonical");
    for (const int m : {5, 16, 31}) {
        bool found = false;
        for (const int k : ply::kBlobMasks) found = found || k == m;
        check(found, (std::string("mask ") + std::to_string(m) +
                      " (a fixture mask) is canonical").c_str());
    }
}

// --- the mask-table wiring (b1.3) ------------------------------------------

const std::string& blob47Document() {
    static const std::string doc = [] {
        const std::string path =
            std::string(PLY_TEST_DATA_DIR) + "/tileset_blob47.json";
        const ply::TileSetLoadResult loaded = ply::loadTileSetFile(path);
        if (!loaded.ok()) {
            std::fprintf(stderr, "  FAIL: blob47 fixture does not load (%s)\n",
                         loaded.error.message.c_str());
        }
        return ply::emitTileSet(loaded.tileset);
    }();
    return doc;
}

void testMaskTableWiring() {
    // THE table identity, asserted against the fixture: the blob47 fixture
    // carries exactly one tile per canonical mask, in kBlobMasks order, so
    // fixture tile i's mask must equal kBlobMasks[i] for all 47 — the loader's
    // validation table and the resolver's future lookup table are one table,
    // and the fixture is where the two sides meet (the pre-decision record).
    const ply::TileSetLoadResult loaded = ply::loadTileSetFromString(blob47Document());
    check(loaded.ok(), "blob47 fixture loads");
    // The guard is the tile count, not only ok(): an ok()-with-empty-model
    // stub must fail its pins, not walk an empty vector (the a5.1 lesson).
    if (!loaded.ok() || loaded.tileset.tiles.size() != 47) return;
    const ply::TileSet& ts = loaded.tileset;

    bool idsInOrder = true;
    for (int i = 0; i < 47; ++i) {
        idsInOrder = idsInOrder && ts.tiles[static_cast<std::size_t>(i)].id == i;
    }
    check(idsInOrder, "blob47 ids are 0..46 in table order");

    bool identity = true;
    for (int i = 0; i < 47; ++i) {
        identity = identity && ts.tiles[static_cast<std::size_t>(i)].mask ==
                                ply::kBlobMasks[i];
    }
    check(identity, "fixture tile i's mask == kBlobMasks[i] for all 47");

    bool uniform = true;
    for (const ply::TileData& t : ts.tiles) {
        uniform = uniform && t.variant == "grass" && t.palette == "overworld" &&
                  !t.solid && t.animation.empty() && t.collision.empty() &&
                  t.region.w == 16 && t.region.h == 16;
    }
    check(uniform, "blob47 tiles are one uniform grass terrain skeleton");

    // The predicate: exactly the 47 canonical masks are canonical;
    // kNoTileMask (-1) is the ABSENCE of a role, not a role.
    bool predicate = true;
    for (int i = 0; i < 47; ++i) {
        predicate = predicate && ply::isCanonicalBlobMask(ply::kBlobMasks[i]);
    }
    check(predicate, "isCanonicalBlobMask accepts every table entry");
    check(!ply::isCanonicalBlobMask(ply::kNoTileMask),
          "kNoTileMask is not canonical (no role is the absence of one)");
    for (const int bad : {3, 2, 130, 256, -2}) {
        check(!ply::isCanonicalBlobMask(bad),
              (std::string("mask ") + std::to_string(bad) + " is not canonical").c_str());
    }

    // The index: blobMaskIndex inverts the table exactly, which is what the
    // resolver will consume as its canonical position.
    bool index = true;
    for (int i = 0; i < 47; ++i) {
        index = index && ply::blobMaskIndex(ply::kBlobMasks[i]) == i;
    }
    check(index, "blobMaskIndex(kBlobMasks[i]) == i for all 47");
    check(ply::blobMaskIndex(ply::kNoTileMask) == -1, "index of no-role is -1");
    for (const int bad : {3, 2, 130, 256, -2}) {
        check(ply::blobMaskIndex(bad) == -1,
              (std::string("index of mask ") + std::to_string(bad) + " is -1").c_str());
    }
    // The two views agree on the fixture: the index of every fixture tile's
    // mask is that tile's id (the fixture IS the table, materialised).
    bool inversion = true;
    for (int i = 0; i < 47; ++i) {
        inversion = inversion && ply::blobMaskIndex(ts.tiles[static_cast<std::size_t>(i)].mask) == i;
    }
    check(inversion, "every fixture tile's mask indexes back to its id");

    // Loader identity, both directions: the blob47 fixture loading proves
    // the loader accepts everything the predicate accepts (all 47 validated
    // at load through the same predicate); the battery's UnknownMask cases
    // prove it rejects everything the predicate rejects. And emit is
    // stable for the terrain skeleton too.
    const std::string emitted = ply::emitTileSet(ts);
    check(ply::loadTileSetFromString(emitted).tileset == ts,
          "blob47 round-trips through emit");
}

// --- structural errors (the minimal set b1.1 validates; the dedicated
// invalid-case battery is b1.2) ---------------------------------------------

struct ErrorCase {
    const char* name;
    std::string doc;
    ply::MapErrorCode code;
};

/// The consolidated invalid-case battery (b1.2): every way the tileset
/// loader can reject a document, each with its named token. The four
/// map-side tokens (EmptyLayerName, DuplicateLayer, LayerSizeMismatch,
/// MissingTileset) are NOT reachable from a tileset document, and the
/// coverage loop below pins that boundary.
const std::vector<ErrorCase>& errorCases() {
    static const std::vector<ErrorCase> cases = {
        {"malformed", "{not json", ply::MapErrorCode::MalformedJson},
        {"root array", "[]", ply::MapErrorCode::WrongType},
        {"missing name",
         "{\"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, \"tiles\": []}",
         ply::MapErrorCode::MissingField},
        {"name wrong type",
         "{\"name\": 5, \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": []}",
         ply::MapErrorCode::WrongType},
        {"empty name",
         "{\"name\": \"\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": []}",
         ply::MapErrorCode::EmptyTilesetName},
        {"missing tiles",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16}",
         ply::MapErrorCode::MissingField},
        {"zero tile size",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 0, \"tile_height\": 16, "
         "\"tiles\": []}",
         ply::MapErrorCode::NonPositiveSize},
        {"missing region",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0}]}",
         ply::MapErrorCode::MissingField},
        {"region zero width",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 0, \"h\": 16}}]}",
         ply::MapErrorCode::NegativeRegion},
        {"duplicate id",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}}, "
         "{\"id\": 0, \"region\": {\"x\": 16, \"y\": 0, \"w\": 16, \"h\": 16}}]}",
         ply::MapErrorCode::DuplicateTileId},
        {"mask above 255",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"mask\": 256}]}",
         ply::MapErrorCode::MaskOutOfRange},
        {"mask below -1",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"mask\": -2}]}",
         ply::MapErrorCode::MaskOutOfRange},
        {"animation zero steps",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"animation\": [{\"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"steps\": 0}]}]}",
         ply::MapErrorCode::NonPositiveSize},
        {"animation negative steps",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"animation\": [{\"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"steps\": -3}]}]}",
         ply::MapErrorCode::NonPositiveSize},
        {"collision rect out of the tile",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"collision\": [{\"x\": 0, \"y\": 15, \"w\": 16, \"h\": 2}]}]}",
         ply::MapErrorCode::NegativeRegion},
        // --- b1.2: the dedicated battery's own cases ---
        {"unknown mask 3 (NE without E)",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"mask\": 3}]}",
         ply::MapErrorCode::UnknownMask},
        {"unknown mask 2 (NE alone)",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"mask\": 2}]}",
         ply::MapErrorCode::UnknownMask},
        {"unknown mask 130 (NW without W or N)",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
         "\"mask\": 130}]}",
         ply::MapErrorCode::UnknownMask},
        {"negative tile id",
         "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
         "\"tiles\": [{\"id\": -5, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}}]}",
         ply::MapErrorCode::OutOfRangeTileId},
    };
    return cases;
}

void testStructuralErrors() {
    const std::vector<ErrorCase>& cases = errorCases();
    for (const ErrorCase& c : cases) {
        const ply::TileSetLoadResult r = ply::loadTileSetFromString(c.doc);
        check(!r.ok(), (std::string(c.name) + " rejected").c_str());
        check(r.error.code == c.code,
              (std::string(c.name) + " names " + ply::toString(c.code)).c_str());
        // A failed load returns an empty result, not a partial tileset.
        check(r.tileset.tiles.empty() && r.tileset.name.empty(),
              (std::string(c.name) + " leaves an empty result").c_str());
    }
    // Coverage: every token the tileset loader can produce has at least one
    // case above; the four map-side tokens are NOT reachable from a tileset
    // document, pinned by name so the boundary is explicit.
    std::size_t covered = 0;
    for (int t = 1; t <= static_cast<int>(ply::MapErrorCode::UnknownMask); ++t) {
        const ply::MapErrorCode token = static_cast<ply::MapErrorCode>(t);
        bool found = false;
        for (const ErrorCase& c : cases) found = found || c.code == token;
        if (token == ply::MapErrorCode::EmptyLayerName ||
            token == ply::MapErrorCode::DuplicateLayer ||
            token == ply::MapErrorCode::LayerSizeMismatch ||
            token == ply::MapErrorCode::MissingTileset) {
            check(!found, (std::string(ply::toString(token)) +
                           " is map-side, unreachable from a tileset").c_str());
        } else if (token == ply::MapErrorCode::FileNotReadable) {
            // Only the file API can produce this one; covered below.
            check(!found, "FileNotReadable lives on the file API, not the table");
            ++covered;
        } else {
            check(found, (std::string("token ") + ply::toString(token) +
                          " has a named case").c_str());
            ++covered;
        }
    }
    const ply::TileSetLoadResult unreadable =
        ply::loadTileSetFile("/nonexistent/tileset.json");
    check(!unreadable.ok() && unreadable.error.code == ply::MapErrorCode::FileNotReadable,
          "unreadable file names FileNotReadable");
    check(unreadable.tileset.tiles.empty(), "unreadable file leaves an empty result");
    check(covered == 11, "11 loadable tokens covered");

    check(ply::toString(ply::MapErrorCode::MaskOutOfRange) ==
              std::string("MaskOutOfRange"),
          "new token is stable");
    check(ply::toString(ply::MapErrorCode::UnknownMask) ==
              std::string("UnknownMask"),
          "UnknownMask token is stable");

    // Tolerance, positively: an unknown key on the document root and on a
    // tile both load.
    const ply::TileSetLoadResult tolerant = ply::loadTileSetFromString(
        "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
        "\"future_root_key\": 1, \"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, "
        "\"w\": 16, \"h\": 16}, \"future_tile_key\": true}]}");
    check(tolerant.ok() && tolerant.tileset.tiles.size() == 1, "unknown keys ignored");

    // Positive control beside the battery: mask 7 (N+NE+E — NE's corner
    // constraint satisfied) IS canonical and loads; id 0 is a valid id.
    const ply::TileSetLoadResult positive = ply::loadTileSetFromString(
        "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
        "\"tiles\": [{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"mask\": 7}]}");
    check(positive.ok() && positive.tileset.tiles[0].mask == 7,
          "mask 7 (canonical) loads; id 0 is a valid id");
}

void printEvidence() {
    const ply::TileSet ts = ply::loadTileSetFromString(fixtureDocument()).tileset;
    const std::string emitted = ply::emitTileSet(ts);
    const ply::TileSet reloaded = ply::loadTileSetFromString(emitted).tileset;

    // The wiring transcript: the blob47 fixture's mask sequence vs the table.
    int wiringMatch = 0;
    const ply::TileSetLoadResult blob47 = ply::loadTileSetFromString(blob47Document());
    if (blob47.ok() && blob47.tileset.tiles.size() == 47) {
        for (int i = 0; i < 47; ++i) {
            wiringMatch += blob47.tileset.tiles[static_cast<std::size_t>(i)].mask ==
                           ply::kBlobMasks[i];
        }
    }

    std::size_t maskCount = 0;
    std::size_t collisionCount = 0;
    for (const ply::TileData& t : ts.tiles) {
        if (t.mask != ply::kNoTileMask) ++maskCount;
        collisionCount += t.collision.size();
    }

    std::printf(
        "EVIDENCE tileset tiles=%zu sorted=1/1 ids=0..10 masks=%zu anim=%zux%d "
        "phase=%d collision_rects=%zu defaults=1 roundtrip=%s fixed=%s a9_compat=%s "
        "blob=%d errors=%zu blob47=%d identity=%d/47\n",
        ts.tiles.size(), maskCount, ts.tiles[8].animation.size(), ts.tiles[8].animation[0].steps,
        ts.tiles[8].phase, collisionCount,
        reloaded == ts ? "identical" : "DIFFER",
        ply::emitTileSet(reloaded) == emitted ? "identical" : "DIFFER",
        ply::loadTilesetFromString(ply::emitTileSet(
                                       ply::loadTileSetFile(std::string(PLY_TEST_DATA_DIR) +
                                                            "/tileset_static.json")
                                           .tileset))
                      .ok()
                  ? "identical"
                  : "DIFFER",
        ply::kBlobMaskCount, errorCases().size(), blob47.ok() ? 47 : 0, wiringMatch);
}

}  // namespace

int main() {
    testFixtureModel();
    testRoundTrip();
    testA9Compatibility();
    testCanonicalMasks();
    testMaskTableWiring();
    testStructuralErrors();

    if (failures == 0) {
        printEvidence();
        std::printf("tileset tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "tileset tests: %d failure(s)\n", failures);
    return 1;
}
