// PropLayer (M1b b5.1–b5.4 as one subsystem, E-b5.x): "prop collision matches
// art footprint; validator rejects props outside bounds; shadow anchored at
// feet; occlusion flips exactly at the player's y" — plus the roadmap rows'
// "fixture props load with correct rects" (b5.1 data + loader: multi-tile,
// pivot, layer flag).
//
// Four pin groups, one per row: (1) data + loader + round-trip + loud errors;
// (2) collision rects transformed exactly onto the art footprint (partial
// tiles, default = whole footprint); (3) the generated shadow anchored at the
// FEET (the pivot), never the art centre; (4) the occlusion predicate flips
// exactly at the player's y, gated by the flag, with the b3a layer tokens
// selecting the draw pass.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/MapLoader.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran).
int dataMatch = 0, dataRan = 0;
int collisionMatch = 0, collisionRan = 0;
int shadowMatch = 0, shadowRan = 0;
int occlusionMatch = 0, occlusionRan = 0;
int errorMatch = 0, errorRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

// Exact-value write check: `"key": value` with a non-digit boundary, so
// "width": 1 never matches "width": 10 (the map's own key).
bool writesInt(const std::string& doc, const char* key, int value) {
    const std::string needle = std::string("\"") + key + "\": " + std::to_string(value);
    std::size_t pos = doc.find(needle);
    while (pos != std::string::npos) {
        const std::size_t after = pos + needle.size();
        if (after >= doc.size() ||
            !std::isdigit(static_cast<unsigned char>(doc[after]))) {
            return true;
        }
        pos = doc.find(needle, after);
    }
    return false;
}

/// Safe indexed read: an OOB index reads as an impossible rect so a hollow
/// or broken build fails the pin instead of aborting — the a5.1 lesson
/// (this suite's first red run segfaulted on collisionRects(0)[0] against
/// the stub, and the harness was guarded BEFORE the implementation landed).
ply::UIRect rectAt(const std::vector<ply::UIRect>& rects, std::size_t i) {
    return i < rects.size() ? rects[i] : ply::UIRect{-999999, -999999, -999999, -999999};
}

// --- fixtures ----------------------------------------------------------------

ply::TilesetSlice makeTileset() {
    ply::TilesetSlice ts;
    ts.name = "t";
    ts.image = "t.png";
    ts.tileWidth = 16;
    ts.tileHeight = 16;
    for (int i = 0; i < 8; ++i) {
        ply::TileDef d;
        d.id = i;
        d.region = {0, i * 16, 16, 16};
        ts.tiles.push_back(d);
    }
    return ts;
}

/// The fixture prop array (document frame: map px, y-down; pivot = feet).
/// oak: 2x3 tiles, trunk collision, interaction; sign: 1x1 defaults + above;
/// roof: 3x2, above + occlusion, bottom-strip collision.
const char* kPropsJson =
    "[{\"name\": \"oak\", \"tile\": 2, \"x\": 48, \"y\": 96, \"width\": 2, \"height\": 3,"
    " \"collision\": [{\"x\": 8, \"y\": 36, \"w\": 16, \"h\": 12}], \"interaction\": \"chop\"},"
    " {\"name\": \"sign\", \"tile\": 3, \"x\": 120, \"y\": 100,"
    " \"layer\": [\"above-entities\"]},"
    " {\"name\": \"roof\", \"tile\": 5, \"x\": 80, \"y\": 40, \"width\": 3, \"height\": 2,"
    " \"layer\": [\"above-entities\"], \"occlusion\": true,"
    " \"collision\": [{\"x\": 0, \"y\": 16, \"w\": 48, \"h\": 16}]}]";

/// A 10x8 map document with the given props JSON spliced in (raw — it may be
/// the fixture array, a broken value, or absent).
std::string docWith(const std::string& propsJson) {
    std::string doc =
        "{\"name\": \"pm\", \"tileset\": \"t\", \"width\": 10, \"height\": 8,"
        " \"layers\": [{\"name\": \"ground\", \"tiles\": [";
    for (int i = 0; i < 80; ++i) {
        if (i) doc += ", ";
        doc += "0";
    }
    doc += "]}]";
    if (!propsJson.empty()) doc += ", \"props\": " + propsJson;
    doc += "}";
    return doc;
}

ply::MapData loadProps(const std::string& propsJson, bool expectOk) {
    const ply::MapLoadResult r = ply::loadMapFromString(docWith(propsJson));
    if (expectOk != r.ok()) {
        std::fprintf(stderr, "  FAIL: load %s but expected %s (%s)\n",
                     r.ok() ? "ok" : "rejected", expectOk ? "ok" : "rejected",
                     r.error.message.c_str());
        ++failures;
    }
    return r.map;
}

/// Load-failure pin: the document is rejected with `code` and the map is
/// left EMPTY (the O4 uniform-rejection rule).
void pinReject(const std::string& propsJson, ply::MapErrorCode code, const char* what) {
    const ply::MapLoadResult r = ply::loadMapFromString(docWith(propsJson));
    const bool ok = !r.ok() && r.error.code == code && r.map.layers.empty() &&
                    r.map.props.empty();
    pinAt(errorMatch, errorRan, ok, what);
    if (!ok && r.ok()) std::fprintf(stderr, "  FAIL: %s loaded instead\n", what);
}

// --- (1) data + loader: fixture props load with correct rects -----------------

void testLoadAndRects() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadProps(kPropsJson, true);
    pinAt(dataMatch, dataRan, map.props.size() == 3, "three props load");

    const ply::PropLayer layer = ply::PropLayer::build(map, ts);
    pinAt(dataMatch, dataRan, layer.size() == 3, "PropLayer holds three");

    // oak: pivot (48, 96) doc -> world feet (48, 32) on a 128-px-tall map;
    // art 2x3 tiles = 32x48 px with the feet at bottom-centre: {32, 32, 32, 48}.
    pinAt(dataMatch, dataRan,
          layer.artRect(0) == ply::UIRect({32, 32, 32, 48}), "oak art rect literal");
    pinAt(dataMatch, dataRan,
          layer.pivotY(0) == 32 && layer.prop(0).x == 48 && layer.prop(0).y == 96,
          "oak pivot stored doc-frame, feet line world 32");
    pinAt(dataMatch, dataRan,
          layer.prop(0).widthTiles == 2 && layer.prop(0).heightTiles == 3,
          "oak multi-tile footprint 2x3");
    pinAt(dataMatch, dataRan,
          layer.prop(0).interaction == "chop", "oak interaction carried");

    // sign: every OPTIONAL key absent -> the documented defaults (its layer
    // token is explicit in the fixture and pinned below).
    const ply::Prop& sign = layer.prop(1);
    pinAt(dataMatch, dataRan,
          sign.widthTiles == 1 && sign.heightTiles == 1 && !sign.occlusion &&
              sign.collision.empty() && sign.interaction.empty(),
          "sign defaults: 1x1, no occlusion, no collision, no interaction");
    pinAt(dataMatch, dataRan,
          sign.flags == ply::kLayerAboveEntities, "sign layer token parses");
    pinAt(dataMatch, dataRan,
          layer.artRect(1) == ply::UIRect({112, 28, 16, 16}), "sign art rect literal");
    pinAt(dataMatch, dataRan,
          layer.drawsAboveEntities(1) && !layer.drawsAboveEntities(0),
          "layer token selects the pass (oak below, sign above)");
    pinAt(dataMatch, dataRan,
          layer.prop(2).aboveEntities() && layer.prop(2).occlusion,
          "roof carries above-entities + occlusion");

    // Round-trip: load -> emit -> load model-identical, emit a fixed point,
    // emit -> load -> emit byte-identical.
    const std::string emitted = ply::emitMap(map);
    const ply::MapData reloaded = ply::loadMapFromString(emitted).map;
    pinAt(dataMatch, dataRan, reloaded == map, "props round-trip model-identical");
    pinAt(dataMatch, dataRan, ply::emitMap(reloaded) == emitted, "emit is a fixed point");
    pinAt(dataMatch, dataRan,
          ply::loadMapFromString(ply::emitMap(reloaded)).map == reloaded,
          "emit -> load -> emit byte-identical");

    // a9 compatibility: a props-less map carries no props key at all.
    const ply::MapData plain = loadProps("", true);
    const std::string plainEmitted = ply::emitMap(plain);
    pinAt(dataMatch, dataRan, plain.props.empty(), "props-less map loads with no props");
    pinAt(dataMatch, dataRan,
          plainEmitted.find("props") == std::string::npos,
          "a9 byte shape: no props key when there are none");
    // Canonical byte shape: defaults are omitted on emit (the b3a byte-shape
    // rule) — a 1x1 footprint and a false occlusion are never written.
    pinAt(dataMatch, dataRan,
          !writesInt(emitted, "width", 1) && !writesInt(emitted, "height", 1) &&
              emitted.find("\"occlusion\": false") == std::string::npos,
          "emit omits prop defaults (no width/height 1, no occlusion false)");
}

// --- (2) collision matches the art footprint ---------------------------------

void testCollision() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadProps(kPropsJson, true);
    const ply::PropLayer layer = ply::PropLayer::build(map, ts);

    // oak's trunk: art-local (8, 36, 16, 12) on a 32x48 art at world
    // {32, 32, 32, 48} -> world {40, 32, 16, 12} — the art's bottom 12 px
    // (the feet region): in y-up the rect's y IS its bottom edge, and both
    // bottom edges sit on the feet line (world y 32).
    pinAt(collisionMatch, collisionRan, layer.collisionRects(0).size() == 1,
          "oak one collision rect");
    pinAt(collisionMatch, collisionRan,
          rectAt(layer.collisionRects(0), 0) == ply::UIRect({40, 32, 16, 12}),
          "oak trunk lands on the art bottom-centre");
    pinAt(collisionMatch, collisionRan,
          rectAt(layer.collisionRects(0), 0).y == layer.artRect(0).y,
          "trunk sits on the art's bottom edge (feet)");

    // sign: no collision authored -> the whole art footprint.
    pinAt(collisionMatch, collisionRan,
          layer.collisionRects(1).size() == 1 &&
              rectAt(layer.collisionRects(1), 0) == layer.artRect(1),
          "default collision IS the art footprint");

    // roof's bottom strip: (0, 16, 48, 16) on a 48x32 art at world
    // {56, 88, 48, 32} -> {56, 88, 48, 16} (the art's lower half).
    pinAt(collisionMatch, collisionRan,
          layer.collisionRects(2).size() == 1 &&
              rectAt(layer.collisionRects(2), 0) == ply::UIRect({56, 88, 48, 16}),
          "roof strip lands on the art's lower half");

    // Every collision rect is inside its art rect (the footprint rule).
    bool inside = true;
    for (std::size_t p = 0; p < layer.size(); ++p) {
        const ply::UIRect art = layer.artRect(p);
        for (const ply::UIRect& r : layer.collisionRects(p)) {
            inside = inside && r.x >= art.x && r.y >= art.y &&
                     r.x + r.w <= art.x + art.w && r.y + r.h <= art.y + art.h;
        }
    }
    pinAt(collisionMatch, collisionRan, inside, "every collision rect fits its art");

    // Loud at load: a rect leaving the art is NegativeRegion (intrinsic
    // geometry — negative origin or non-positive size).
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8,"
              " \"collision\": [{\"x\": -1, \"y\": 0, \"w\": 8, \"h\": 8}]}]",
              ply::MapErrorCode::NegativeRegion, "collision x<0 -> NegativeRegion");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8,"
              " \"collision\": [{\"x\": 0, \"y\": 0, \"w\": 0, \"h\": 8}]}]",
              ply::MapErrorCode::NegativeRegion, "collision w=0 -> NegativeRegion");
}

// --- (3) shadow anchored at feet ---------------------------------------------

void testShadow() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadProps(kPropsJson, true);
    const ply::PropLayer layer = ply::PropLayer::build(map, ts);

    // oak: shadow width = art width 32, height = tileHeight/2 = 8, centred
    // ON THE PIVOT (world (48, 32)) -> {32, 28, 32, 8}. The art centre is
    // (48, 56): the shadow is NOT there.
    pinAt(shadowMatch, shadowRan,
          layer.shadowRect(0) == ply::UIRect({32, 28, 32, 8}), "oak shadow literal at feet");
    pinAt(shadowMatch, shadowRan,
          layer.shadowRect(0).x + layer.shadowRect(0).w / 2 == 48 &&
              layer.shadowRect(0).y + layer.shadowRect(0).h / 2 == 32,
          "oak shadow centred on the pivot (feet)");
    pinAt(shadowMatch, shadowRan,
          layer.shadowRect(0).y + layer.shadowRect(0).h / 2 != 56,
          "shadow is not at the art centre");

    // The rule is generated, not authored: same formula on every prop.
    pinAt(shadowMatch, shadowRan,
          layer.shadowRect(1) == ply::UIRect({112, 24, 16, 8}), "sign shadow literal");
    pinAt(shadowMatch, shadowRan,
          layer.shadowRect(2) == ply::UIRect({56, 84, 48, 8}), "roof shadow literal");

    // The shadow spans the full art width and its centre sits ON the art's
    // bottom edge (the feet — in y-up the art's y IS its bottom edge).
    bool anchored = true;
    for (std::size_t p = 0; p < layer.size(); ++p) {
        const ply::UIRect art = layer.artRect(p);
        const ply::UIRect sh = layer.shadowRect(p);
        anchored = anchored && sh.w == art.w && sh.y + sh.h / 2 == art.y;
    }
    pinAt(shadowMatch, shadowRan, anchored, "shadow width == art width, feet-aligned");
}

// --- (4) occlusion flips exactly at the player's y ---------------------------

void testOcclusion() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadProps(kPropsJson, true);
    const ply::PropLayer layer = ply::PropLayer::build(map, ts);

    // roof's feet line is world y 88: the ordering flips exactly there.
    pinAt(occlusionMatch, occlusionRan, layer.pivotY(2) == 88, "roof feet line 88");
    pinAt(occlusionMatch, occlusionRan, !layer.occludes(2, 87), "player at 87: not behind");
    pinAt(occlusionMatch, occlusionRan, layer.occludes(2, 88),
          "player AT 88: the flip lands exactly on the player's y");
    pinAt(occlusionMatch, occlusionRan, layer.occludes(2, 89), "player at 89: behind");

    // The flag gates everything: oak (no occlusion flag) never occludes,
    // at any player y.
    pinAt(occlusionMatch, occlusionRan,
          !layer.occludes(0, 31) && !layer.occludes(0, 32) && !layer.occludes(0, 100000),
          "no occlusion flag -> never occludes");

    // Draw-pass selection (the b3a tokens, same grammar): below-entities and
    // above-entities props land in their passes.
    pinAt(occlusionMatch, occlusionRan,
          !layer.drawsAboveEntities(0) && layer.drawsAboveEntities(1) &&
              layer.drawsAboveEntities(2),
          "pass assignment follows the layer flag");

    // The layer flag grammar is b3a's, with b3a's loud failures.
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8,"
              " \"layer\": [\"abov\"]}]",
              ply::MapErrorCode::UnknownLayerFlag, "prop unknown layer token");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8,"
              " \"layer\": [\"below-entities\", \"above-entities\"]}]",
              ply::MapErrorCode::ConflictingLayerFlags, "prop below+above conflict");
}

// --- the loud battery (b5.1 schema) ------------------------------------------

void testErrors() {
    pinReject("[{\"tile\": 1, \"x\": 8, \"y\": 8}]", ply::MapErrorCode::MissingField,
              "missing name");
    pinReject("[{\"name\": \"p\", \"x\": 8, \"y\": 8}]", ply::MapErrorCode::MissingField,
              "missing tile");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"y\": 8}]", ply::MapErrorCode::MissingField,
              "missing x");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8}]", ply::MapErrorCode::MissingField,
              "missing y");
    pinReject("[{\"name\": \"\", \"tile\": 1, \"x\": 8, \"y\": 8}]",
              ply::MapErrorCode::EmptyPropName, "empty name");
    pinReject("[{\"name\": \"a\", \"tile\": 1, \"x\": 8, \"y\": 8},"
              " {\"name\": \"a\", \"tile\": 1, \"x\": 9, \"y\": 9}]",
              ply::MapErrorCode::DuplicateProp, "duplicate name");
    pinReject("[{\"name\": 5, \"tile\": 1, \"x\": 8, \"y\": 8}]",
              ply::MapErrorCode::WrongType, "name not a string");
    pinReject("[{\"name\": \"p\", \"tile\": \"1\", \"x\": 8, \"y\": 8}]",
              ply::MapErrorCode::WrongType, "tile not an int");
    pinReject("[{\"name\": \"p\", \"tile\": -1, \"x\": 8, \"y\": 8}]",
              ply::MapErrorCode::OutOfRangeTileId, "negative prop tile id");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8.5, \"y\": 8}]",
              ply::MapErrorCode::WrongType, "x not an int");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"width\": 0}]",
              ply::MapErrorCode::NonPositiveSize, "width 0");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"height\": -2}]",
              ply::MapErrorCode::NonPositiveSize, "height negative");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"occlusion\": 1}]",
              ply::MapErrorCode::WrongType, "occlusion not a bool");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"collision\": 5}]",
              ply::MapErrorCode::WrongType, "collision not an array");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"collision\": [5]}]",
              ply::MapErrorCode::WrongType, "collision entry not an object");
    pinReject("[{\"name\": \"p\", \"tile\": 1, \"x\": 8, \"y\": 8, \"interaction\": 7}]",
              ply::MapErrorCode::WrongType, "interaction not a string");
    pinReject("5", ply::MapErrorCode::WrongType, "props not an array");
    pinReject("[5]", ply::MapErrorCode::WrongType, "prop not an object");

    // validateMap (E-b5.x's validator clause): out-of-range prop tile and
    // prop collisions outside the map's bounds, both named. Each case is
    // built only when the fixture actually parsed (a hollow build fails the
    // pins, never the process — the a5.1 guard).
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData ok = loadProps(kPropsJson, true);
    pinAt(errorMatch, errorRan,
          ply::validateMap(ok, {ts}).ok(), "fixture map validates");

    const std::size_t kNeeded = ok.props.size() >= 3 ? 3 : 0;
    pinAt(errorMatch, errorRan, kNeeded == 3, "validator cases: fixture parsed");
    if (kNeeded == 3 && ok.props[0].collision.size() == 1) {
        ply::MapData badTile = ok;
        badTile.props[0].tile = 99;
        pinAt(errorMatch, errorRan,
              ply::validateMap(badTile, {ts}).code == ply::MapErrorCode::OutOfRangeTileId,
              "validator names an out-of-range prop tile");

        ply::MapData outOfBounds = ok;
        outOfBounds.props[1].x = 158;  // sign 16-px collision -> world x 150..166 > 160
        pinAt(errorMatch, errorRan,
              ply::validateMap(outOfBounds, {ts}).code == ply::MapErrorCode::PropOutOfBounds,
              "validator rejects a prop collision outside the map");

        ply::MapData outOfFootprint = ok;
        outOfFootprint.props[0].collision[0].y = 40;  // 40+12 > artH 48
        pinAt(errorMatch, errorRan,
              ply::validateMap(outOfFootprint, {ts}).code == ply::MapErrorCode::NegativeRegion,
              "validator rejects a collision outside the art footprint");

        // THE FOUR BOUNDS GUARDS, one pin each (the b5 read's F1: only the
        // RIGHT edge was pinned above, and disabling the worldX<0/worldY<0
        // sub-guards ran GREEN — a mutation there would have landed
        // silently). Each prop is a default-footprint 1x1 whose art exits
        // the map through one edge of the 160x128 world.
        const auto pinBoundsEdge = [&](const char* propsJson, const char* what) {
            const ply::MapLoadResult r = ply::loadMapFromString(docWith(propsJson));
            pinAt(errorMatch, errorRan,
                  r.ok() && ply::validateMap(r.map, {ts}).code ==
                                ply::MapErrorCode::PropOutOfBounds,
                  what);
        };
        pinBoundsEdge("[{\"name\": \"west\", \"tile\": 1, \"x\": 2, \"y\": 100}]",
                      "collision exits LEFT of the map (worldX < 0 guard)");
        pinBoundsEdge("[{\"name\": \"south\", \"tile\": 1, \"x\": 80, \"y\": 140}]",
                      "collision exits BELOW the map (worldY < 0 guard)");
        pinBoundsEdge("[{\"name\": \"north\", \"tile\": 1, \"x\": 80, \"y\": -10}]",
                      "collision exits ABOVE the map (top guard)");
    }
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const ply::TilesetSlice ts = makeTileset();
    const ply::MapData map = loadProps(kPropsJson, true);
    const ply::PropLayer layer = ply::PropLayer::build(map, ts);
    const std::string plainEmitted = ply::emitMap(loadProps("", true));
    std::printf(
        "EVIDENCE prop_layer props=%zu roundtrip=identical defaults=1 data=%d/%d "
        "collision=%d/%d shadow=%d/%d occlusion=%d/%d errors=%d/%d a9_props_key=%s\n",
        map.props.size(), dataMatch, dataRan, collisionMatch, collisionRan, shadowMatch,
        shadowRan, occlusionMatch, occlusionRan, errorMatch, errorRan,
        plainEmitted.find("props") == std::string::npos ? "absent" : "PRESENT");
}

}  // namespace

int main() {
    testLoadAndRects();
    testCollision();
    testShadow();
    testOcclusion();
    testErrors();

    if (failures == 0) {
        printEvidence();
        std::printf("prop_layer tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "prop_layer tests: %d failure(s)\n", failures);
    return 1;
}
