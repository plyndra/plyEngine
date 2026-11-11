// TileLayer (M1b b3a, E-b3a.1): "Int32 tile ids per cell; per-layer flags
// (below/above entities, animated, opaque)" — ARCHITECTURE_TARGET.md §3, and
// the roadmap row "TileLayer (ids + flags) | layer flags survive round-trip".
//
// This suite owns the FLAGS half of the layer (the ids half is a9's
// map_loader suite) and the interaction with the map format: the default, each
// token, combinations, per-layer independence, byte-level round-trip, the a9
// compatibility rule (a flags-less document emits no flags key), and the two
// loud failures — an unknown token and a layer claiming both below and above
// entities.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/MapLoader.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

/// toString may legitimately return nullptr; the grader must not construct a
/// std::string from it (the a5.1 harness lesson — a stub must fail its pins,
/// not abort the suite).
std::string tokenOr(unsigned flag, const char* fallback) {
    const char* token = ply::toString(flag);
    return token != nullptr ? std::string(token) : std::string(fallback);
}

void check(bool condition, const char* what) {
    if (!condition) {
        ++failures;
        std::fprintf(stderr, "  FAIL: %s\n", what);
    }
}

// --- a small map document builder ------------------------------------------

/// One 2x1 map with the given per-layer flag JSON fragments (the fragment is
/// spliced in raw, so it can be a well-formed array, a malformed value, or
/// absent). Layer names are A, B, C ... in declaration order.
std::string mapDoc(const std::vector<std::string>& flagFragments) {
    static const char* kNames[] = {"A", "B", "C", "D", "E"};
    std::string doc = "{\"name\": \"m\", \"tileset\": \"t\", \"width\": 2, \"height\": 1, \"layers\": [";
    for (std::size_t i = 0; i < flagFragments.size(); ++i) {
        if (i) doc += ", ";
        doc += std::string("{\"name\": \"") + kNames[i] + "\", \"tiles\": [0, 0]";
        if (!flagFragments[i].empty()) doc += ", \"flags\": " + flagFragments[i];
        doc += "}";
    }
    doc += "]}";
    return doc;
}

ply::MapData loadOk(const std::vector<std::string>& flags) {
    const ply::MapLoadResult r = ply::loadMapFromString(mapDoc(flags));
    if (!r.ok()) {
        // Record the failure AND return a layer per requested layer, so a
        // broken load shows up as failed pins rather than as an out-of-bounds
        // read in the caller (the a5.1 harness lesson: a mutation must not
        // abort the suite, because a crash is not a FAIL).
        std::fprintf(stderr, "  FAIL: document does not load (%s)\n", r.error.message.c_str());
        ply::MapData shim;
        shim.layers.resize(flags.size());
        return shim;
    }
    return r.map;
}

// --- defaults and tokens ---------------------------------------------------

void testDefaultAndTokens() {
    // An absent flags key is ground: the default must be below-entities, and
    // it must be a single flag, not "all flags" (which would make the default
    // state indistinguishable from a maximally-flagged layer).
    const ply::MapData plain = loadOk({""});
    check(plain.layers.size() == 1, "one layer");
    check(plain.layers[0].flags == ply::kLayerBelowEntities,
          "absent flags defaults to below-entities");
    check(plain.layers[0].aboveEntities() == false, "default is not above");
    check(plain.layers[0].hasFlag(ply::kLayerBelowEntities), "default has below");
    check(!plain.layers[0].hasFlag(ply::kLayerAnimated), "default is not animated");
    check(!plain.layers[0].hasFlag(ply::kLayerOpaque), "default is not opaque");

    // Each token on its own, pinned by the value it sets. Note that
    // "below-entities" stated explicitly equals the default, and
    // "above-entities" REPLACES it rather than adding to it — a layer is one
    // draw order, not two.
    const ply::MapData below = loadOk({"[\"below-entities\"]"});
    check(below.layers[0].flags == ply::kLayerBelowEntities, "explicit below == default");

    const ply::MapData above = loadOk({"[\"above-entities\"]"});
    check(above.layers[0].flags == ply::kLayerAboveEntities,
          "above-entities alone replaces the below default");
    check(!above.layers[0].hasFlag(ply::kLayerBelowEntities), "above is not also below");
    check(above.layers[0].aboveEntities(), "aboveEntities()");

    const ply::MapData animated = loadOk({"[\"animated\"]"});
    check(animated.layers[0].flags == ply::kLayerAnimated, "animated alone");

    const ply::MapData opaque = loadOk({"[\"opaque\"]"});
    check(opaque.layers[0].flags == ply::kLayerOpaque, "opaque alone");

    // An empty array is the same statement as no key.
    const ply::MapData empty = loadOk({"[]"});
    check(empty.layers[0].flags == ply::kLayerBelowEntities, "empty flags array == default");

    // The token table itself, both directions, including the negatives.
    check(tokenOr(ply::kLayerBelowEntities, "") == "below-entities", "below token");
    check(tokenOr(ply::kLayerAboveEntities, "") == "above-entities", "above token");
    check(tokenOr(ply::kLayerAnimated, "") == "animated", "animated token");
    check(tokenOr(ply::kLayerOpaque, "") == "opaque", "opaque token");
    check(ply::toString(0) == nullptr, "no token for 0");
    check(ply::toString(ply::kLayerAllFlags) == nullptr, "no token for a combination");
    check(ply::parseLayerFlag("animated") == ply::kLayerAnimated, "parse animated");
    check(ply::parseLayerFlag("Animated") == 0, "tokens are case-sensitive (mixed case)");
    check(ply::parseLayerFlag("ANIMATED") == 0, "tokens are case-sensitive (upper case)");
    check(ply::parseLayerFlag("ABOVE-ENTITIES") == 0,
          "tokens are case-sensitive (upper case, second token)");
    check(ply::parseLayerFlag("above") == 0, "prefixes are not tokens");
    check(ply::parseLayerFlag("") == 0, "empty is not a token");
}

void testCombinations() {
    // above + animated + opaque: the canopy that sways and hides what is
    // beneath it. Order in the array must not matter (it is a set).
    const ply::MapData canopy =
        loadOk({"[\"above-entities\", \"animated\", \"opaque\"]"});
    const unsigned expected =
        ply::kLayerAboveEntities | ply::kLayerAnimated | ply::kLayerOpaque;
    check(canopy.layers[0].flags == expected, "above+animated+opaque");
    check(canopy.layers[0].aboveEntities(), "the canopy draws above entities");
    check(canopy.layers[0].hasFlag(ply::kLayerAnimated), "the canopy animates");
    check(canopy.layers[0].hasFlag(ply::kLayerOpaque), "the canopy is opaque");

    const ply::MapData reordered =
        loadOk({"[\"opaque\", \"animated\", \"above-entities\"]"});
    check(reordered.layers[0].flags == expected, "flag order does not matter");

    // A repeated token is idempotent, not an error.
    const ply::MapData repeated = loadOk({"[\"animated\", \"animated\"]"});
    check(repeated.layers[0].flags == ply::kLayerAnimated, "repeated token is idempotent");
}

void testPerLayerIndependence() {
    // Three layers, three different flag sets, in authored order. The flags
    // belong to the layer, not to the map: one layer's canopy must not make
    // its neighbour above-entities.
    const ply::MapData map = loadOk({"", "[\"above-entities\", \"opaque\"]", "[\"animated\"]"});
    check(map.layers.size() == 3, "three layers");
    check(map.layers[0].flags == ply::kLayerBelowEntities, "layer 0 keeps the default");
    check(map.layers[1].flags == (ply::kLayerAboveEntities | ply::kLayerOpaque),
          "layer 1 above+opaque");
    check(map.layers[2].flags == ply::kLayerAnimated, "layer 2 animated");
    check(map.layers[0].name == "A" && map.layers[1].name == "B" && map.layers[2].name == "C",
          "layer order preserved");
    // Distinctness: the three flag values are actually different (a shared
    // global would make them equal and this whole suite vacuous).
    check(map.layers[0].flags != map.layers[1].flags &&
              map.layers[1].flags != map.layers[2].flags &&
              map.layers[0].flags != map.layers[2].flags,
          "the three layers carry three distinct flag sets");
}

// --- round-trip ------------------------------------------------------------

void testRoundTrip() {
    const ply::MapData map = loadOk({"", "[\"above-entities\", \"animated\"]"});
    const std::string emitted = ply::emitMap(map);

    // Model-identical, and emit is a fixed point.
    const ply::MapData reloaded = ply::loadMapFromString(emitted).map;
    check(reloaded == map, "load -> emit -> load is model-identical");
    check(ply::emitMap(reloaded) == emitted, "emit is a fixed point");
    check(ply::emitMap(ply::loadMapFromString(ply::emitMap(reloaded)).map) == emitted,
          "emit -> load -> emit byte-identical");

    // The flags really are in the bytes (a model-identical round-trip through
    // a loader that DROPPED the flags would fail the equality above only if
    // the non-default layer differed — so also pin the tokens textually).
    check(emitted.find("\"flags\"") != std::string::npos, "emit writes the flags key");
    check(emitted.find("above-entities") != std::string::npos, "emit writes above-entities");
    check(emitted.find("animated") != std::string::npos, "emit writes animated");

    // A defaults-only map emits no flags key at all: the a9 grammar, so an
    // a9 document round-trips byte-identically through the b3a build.
    const ply::MapData plain = loadOk({""});
    const std::string plainEmitted = ply::emitMap(plain);
    check(plainEmitted.find("flags") == std::string::npos,
          "a defaults-only layer emits no flags key (a9 grammar)");
    check(ply::emitMap(ply::loadMapFromString(plainEmitted).map) == plainEmitted,
          "the flags-less document is still a fixed point");
}

// --- loud failures ---------------------------------------------------------

void testLoudFailures() {
    // An unknown token is a named error, not a silent drop: a typo in an
    // authoring tool must not quietly turn a canopy into ground.
    struct Case {
        const char* name;
        std::string fragment;
        ply::MapErrorCode code;
    };
    const std::vector<Case> cases = {
        {"unknown token", "[\"above-entites\"]", ply::MapErrorCode::UnknownLayerFlag},
        {"unknown token beside a known one", "[\"animated\", \"glowing\"]",
         ply::MapErrorCode::UnknownLayerFlag},
        {"empty token", "[\"\"]", ply::MapErrorCode::UnknownLayerFlag},
        {"numeric token", "[3]", ply::MapErrorCode::WrongType},
        {"flags not an array", "\"animated\"", ply::MapErrorCode::WrongType},
        {"flags null", "null", ply::MapErrorCode::WrongType},
        {"below and above", "[\"below-entities\", \"above-entities\"]",
         ply::MapErrorCode::ConflictingLayerFlags},
        {"above and below", "[\"above-entities\", \"below-entities\"]",
         ply::MapErrorCode::ConflictingLayerFlags},
    };
    for (const Case& c : cases) {
        const ply::MapLoadResult r = ply::loadMapFromString(mapDoc({c.fragment}));
        check(!r.ok(), (std::string(c.name) + " rejected").c_str());
        check(r.error.code == c.code,
              (std::string(c.name) + " names " + ply::toString(c.code)).c_str());
        check(r.map.layers.empty() && r.map.name.empty(),
              (std::string(c.name) + " leaves an empty result").c_str());
    }

    // The tokens are stable parts of the contract.
    check(ply::toString(ply::MapErrorCode::UnknownLayerFlag) ==
              std::string("UnknownLayerFlag"),
          "UnknownLayerFlag token");
    check(ply::toString(ply::MapErrorCode::ConflictingLayerFlags) ==
              std::string("ConflictingLayerFlags"),
          "ConflictingLayerFlags token");

    // The error message names the offending token, so a map author can find
    // it (the a1 discipline: match on the token, but the detail must be
    // useful).
    const ply::MapLoadResult typo = ply::loadMapFromString(mapDoc({"[\"glowing\"]"}));
    check(typo.error.message.find("glowing") != std::string::npos,
          "the UnknownLayerFlag message names the token");

    // Positive control beside the battery: a layer may be below+animated+
    // opaque (only below/above conflict), which also guards against a
    // too-broad conflict check.
    const ply::MapData belowAnimated = loadOk({"[\"below-entities\", \"animated\"]"});
    check(belowAnimated.layers[0].flags ==
              (ply::kLayerBelowEntities | ply::kLayerAnimated),
          "below+animated is legal");
}

void printEvidence() {
    const ply::MapData map = loadOk({"", "[\"above-entities\", \"animated\"]"});
    const std::string emitted = ply::emitMap(map);
    const ply::MapData reloaded = ply::loadMapFromString(emitted).map;
    const std::string plainEmitted = ply::emitMap(loadOk({""}));

    std::printf(
        "EVIDENCE tile_layer layers=%zu default=%u layer1=%u roundtrip=%s fixed=%s "
        "tokens=%d a9_flags_key=%s conflicts=2 unknown=3\n",
        map.layers.size(), map.layers[0].flags, map.layers[1].flags,
        reloaded == map ? "identical" : "DIFFER",
        ply::emitMap(reloaded) == emitted ? "identical" : "DIFFER",
        4,  // below, above, animated, opaque — each pinned by toString above
        plainEmitted.find("flags") == std::string::npos ? "absent" : "PRESENT");
}

}  // namespace

int main() {
    testDefaultAndTokens();
    testCombinations();
    testPerLayerIndependence();
    testRoundTrip();
    testLoudFailures();

    if (failures == 0) {
        printEvidence();
        std::printf("tile_layer tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "tile_layer tests: %d failure(s)\n", failures);
    return 1;
}
