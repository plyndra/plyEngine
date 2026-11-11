// AutoTileResolver (M1b b2, E-b2.1): the blob-47 table, resolved at load and
// cached. ARCHITECTURE_TARGET.md §3: "(neighbour mask, layer, tileset) -> tile
// id, using the blob-47 table; resolved at load, cached"; the roadmap row's
// acceptance: "all 47 cases resolve; unknown case errors".
//
// House rules for this suite: pure data — no GL context, no window, no clock,
// no float. The table's canonical set is b1.3's kBlobMasks and is not
// re-derived here; what this suite owns is the MAPPING (raw 8-neighbour mask
// -> canonical mask -> tile id), the first-by-id variant policy (operator
// decision 2026-11-11), the load-time cache, and the loud failure when a
// configuration has no tile.

#include "plyEngine/AutoTileResolver.hpp"
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

// --- fixtures --------------------------------------------------------------

/// The b1.3 terrain fixture: one tile per canonical mask, ids 0..46 in
/// kBlobMasks order — the complete terrain set.
const ply::TileSet& blob47Tileset() {
    static const ply::TileSet ts = [] {
        const std::string path = std::string(PLY_TEST_DATA_DIR) + "/tileset_blob47.json";
        const ply::TileSetLoadResult loaded = ply::loadTileSetFile(path);
        if (!loaded.ok()) {
            std::fprintf(stderr, "  FAIL: blob47 fixture does not load (%s)\n",
                         loaded.error.message.c_str());
        }
        return loaded.tileset;
    }();
    return ts;
}

/// A tileset document from inline JSON (the error-battery precedent): used
/// for the partial / duplicate-mask / role-less cases, where a committed
/// fixture file would obscure the point being pinned.
ply::TileSet tilesetFrom(const char* tiles) {
    const std::string doc = std::string("{\"name\": \"t\", \"image\": \"i.png\", "
                                        "\"tile_width\": 16, \"tile_height\": 16, "
                                        "\"tiles\": [") + tiles + "]}";
    const ply::TileSetLoadResult loaded = ply::loadTileSetFromString(doc);
    if (!loaded.ok()) {
        std::fprintf(stderr, "  FAIL: inline tileset does not load (%s)\n",
                     loaded.error.message.c_str());
    }
    return loaded.tileset;
}

std::string tile(int id, int mask) {
    return "{\"id\": " + std::to_string(id) +
           ", \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, \"mask\": " +
           std::to_string(mask) + "}";
}

// --- canonicalisation ------------------------------------------------------

void testCanonicalisation() {
    // Every canonical mask is a fixed point: canonicalising it changes
    // nothing (a table entry always resolves to itself).
    bool fixedPoints = true;
    for (int i = 0; i < ply::kBlobMaskCount; ++i) {
        fixedPoints = fixedPoints && ply::canonicalizeBlobMask(ply::kBlobMasks[i]) ==
                                        ply::kBlobMasks[i];
    }
    check(fixedPoints, "every canonical mask is a fixed point");

    // Every one of the 256 raw masks canonicalises INTO the 47-entry set:
    // the documented closure property ("all 47 cases resolve" is about the
    // canonical set; this is what makes every 8-neighbour configuration
    // resolvable by it).
    // The guard is explicit: a stub returning kNoTileMask (-1) must fail the
    // closure pin, not index seen[-1] (the a5.1 harness lesson).
    bool closed = true;
    int distinct = 0;
    std::vector<int> seen(256, 0);
    for (int m = 0; m < 256; ++m) {
        const int c = ply::canonicalizeBlobMask(m);
        const bool canonical = c >= 0 && c < 256 && ply::isCanonicalBlobMask(c);
        closed = closed && canonical;
        if (canonical && !seen[static_cast<std::size_t>(c)]++) ++distinct;
    }
    check(closed, "canonicalizeBlobMask lands in kBlobMasks for all 256 inputs");
    check(distinct == 47, "the 256 raw masks collapse onto exactly 47 classes");

    // The rule itself, pinned on the three shapes it distinguishes: a
    // diagonal WITH both edges survives, a diagonal missing one edge is
    // dropped, and a diagonal missing both is dropped. Only the diagonal bit
    // is removed - edges are never invented.
    check(ply::canonicalizeBlobMask(ply::kNeighbourN | ply::kNeighbourE |
                                    ply::kNeighbourNE) ==
              (ply::kNeighbourN | ply::kNeighbourE | ply::kNeighbourNE),
          "NE with N and E survives");
    check(ply::canonicalizeBlobMask(ply::kNeighbourN | ply::kNeighbourNE) ==
              ply::kNeighbourN,
          "NE without E is dropped; N is kept");
    check(ply::canonicalizeBlobMask(ply::kNeighbourNE) == 0,
          "NE alone is dropped entirely");
    check(ply::canonicalizeBlobMask(0) == 0, "0 is canonical");
    check(ply::canonicalizeBlobMask(255) == 255, "255 is canonical");
    check(ply::canonicalizeBlobMask(3) == 1, "3 (N+NE) -> 1 (N): never 3");
    check(ply::canonicalizeBlobMask(130) == 0, "130 (NE+NW) -> 0: both corners dropped");

    // Out of range is not a mask at all.
    check(ply::canonicalizeBlobMask(-1) == ply::kNoTileMask, "-1 is not a mask");
    check(ply::canonicalizeBlobMask(256) == ply::kNoTileMask, "256 is not a mask");
}

void testNeighbourMaskFromBits() {
    check(ply::neighbourMaskFromBits(true, false, false, false, false, false, false, false) ==
              ply::kNeighbourN,
          "N bit");
    check(ply::neighbourMaskFromBits(false, false, false, false, true, false, false, false) ==
              ply::kNeighbourS,
          "S bit");
    check(ply::neighbourMaskFromBits(false, false, false, false, false, false, true, false) ==
              ply::kNeighbourW,
          "W bit");
    check(ply::neighbourMaskFromBits(false, false, false, false, false, false, false, true) ==
              ply::kNeighbourNW,
          "NW bit");
    // All eight set is 255 and all clear is 0 — the two ends of the blob set.
    check(ply::neighbourMaskFromBits(true, true, true, true, true, true, true, true) == 255,
          "all eight neighbours = 255");
    check(ply::neighbourMaskFromBits(false, false, false, false, false, false, false, false) ==
              0,
          "no neighbours = 0");
    // The bit values are the file format's (kBlobMasks), not the conventional
    // 8-way numbering: pinned so a "tidy-up" cannot silently renumber them.
    check(ply::kNeighbourN == 1 && ply::kNeighbourNE == 2 && ply::kNeighbourE == 4 &&
              ply::kNeighbourSE == 8 && ply::kNeighbourS == 16 &&
              ply::kNeighbourSW == 32 && ply::kNeighbourW == 64 &&
              ply::kNeighbourNW == 128,
          "neighbour bit values match the tileset format");
}

// --- the resolved table ----------------------------------------------------

void testCompleteTerrain() {
    const ply::AutoTileResolver resolver = ply::AutoTileResolver::build(blob47Tileset());
    check(resolver.complete(), "blob47 terrain set is complete");
    check(resolver.firstMissingMask() == -1, "complete set has no missing mask");
    check(resolver.resolvedCount() == 47, "47 masks resolved");

    // All 47 cases resolve, to the fixture's own tile: id == canonical index
    // (the fixture is the table materialised, b1.3).
    bool all47 = true;
    for (int i = 0; i < 47; ++i) {
        const ply::TileQuery q = resolver.resolve(ply::kBlobMasks[i]);
        all47 = all47 && q.ok() && q.tileId == i && q.mask == ply::kBlobMasks[i];
    }
    check(all47, "all 47 canonical masks resolve to their fixture tile");

    // And the table is the index, directly: tileAt(i) == i.
    bool direct = true;
    for (int i = 0; i < 47; ++i) {
        direct = direct && resolver.tileAt(static_cast<std::size_t>(i)) == i;
    }
    check(direct, "tileAt(canonical index) is the fixture tile id");

    // EVERY raw 8-neighbour mask resolves too, via canonicalisation: no
    // configuration can draw a hole in a complete terrain set.
    bool allRaw = true;
    int rawResolved = 0;
    for (int m = 0; m < 256; ++m) {
        const ply::TileQuery q = resolver.resolve(m);
        allRaw = allRaw && q.ok() && q.mask == ply::canonicalizeBlobMask(m);
        if (q.ok()) ++rawResolved;
    }
    check(allRaw, "all 256 raw masks resolve in a complete terrain set");
    check(rawResolved == 256, "256 raw masks resolved");

    // Out-of-range is not resolved, and says so (kNoTileMask, not a hole).
    check(!resolver.resolve(-1).ok(), "-1 does not resolve");
    check(!resolver.resolve(-1).ok() && resolver.resolve(-1).mask == ply::kNoTileMask,
          "-1 reports kNoTileMask");
    check(!resolver.resolve(256).ok(), "256 does not resolve");

    // No tile claims two masks: the fixture is one-tile-per-mask.
    bool singleVariant = true;
    for (int i = 0; i < 47; ++i) {
        singleVariant = singleVariant && resolver.variantCount(ply::kBlobMasks[i]) == 1;
    }
    check(singleVariant, "every mask has exactly one variant in the fixture");
    check(resolver.variantCount(3) == 0 && resolver.variantCount(-1) == 0,
          "non-canonical masks have no variants");
}

// --- variant policy: first-by-id (operator decision 2026-11-11) ------------

void testVariantPolicy() {
    // Two tiles claim mask 5, one claims 7; the lowest id wins mask 5.
    const ply::TileSet ts = tilesetFrom((tile(9, 5) + ", " + tile(3, 5) + ", " + tile(7, 7)).c_str());
    const ply::AutoTileResolver resolver = ply::AutoTileResolver::build(ts);

    check(resolver.resolve(5).ok() && resolver.resolve(5).tileId == 3,
          "first-by-id: mask 5 resolves to tile 3, not tile 9");
    check(resolver.resolve(7).tileId == 7, "single-variant mask 7 resolves to tile 7");
    check(resolver.variantCount(5) == 2, "mask 5 has two variants");
    check(resolver.variantCount(7) == 1, "mask 7 has one variant");
    check(!resolver.complete(), "a two-mask set is not complete");
    check(resolver.firstMissingMask() == 0,
          "the first missing mask is 0, the table's first entry");

    // Tiles with no autotile role are ignored entirely: they neither resolve
    // nor make a set complete, and they do not claim mask -1.
    const ply::TileSet roleless = tilesetFrom(
        "{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}}");
    const ply::AutoTileResolver noRoles = ply::AutoTileResolver::build(roleless);
    check(noRoles.resolvedCount() == 0, "a role-less tileset resolves nothing");
    check(!noRoles.complete(), "a role-less tileset is not complete");
    check(noRoles.firstMissingMask() == 0, "role-less first missing mask is 0");
    check(!noRoles.resolve(0).ok(), "role-less resolve(0) does not resolve");
}

// --- an incomplete terrain set fails loudly, never draws a hole ------------

void testIncompleteSet() {
    // A grass set with only 5 masks: the 5 are resolvable, and the other 42
    // name their first absence instead of returning a plausible tile.
    std::string tiles;
    const int present[] = {0, 5, 16, 31, 255};
    for (int i = 0; i < 5; ++i) {
        if (i) tiles += ", ";
        tiles += tile(i, present[i]);
    }
    const ply::AutoTileResolver resolver =
        ply::AutoTileResolver::build(tilesetFrom(tiles.c_str()));

    bool presentResolve = true;
    for (int i = 0; i < 5; ++i) presentResolve = presentResolve && resolver.resolve(present[i]).ok();
    check(presentResolve, "the five declared masks resolve");
    check(resolver.resolvedCount() == 5, "five masks resolved");
    check(!resolver.complete(), "five masks are not a complete terrain set");
    check(resolver.firstMissingMask() == 1,
          "the first missing canonical mask is 1 (0 is present)");

    // The specific hole: mask 4 (E only) is absent, so both the raw mask and
    // its canonical form fail, and the query names the canonical mask.
    const ply::TileQuery q = resolver.resolve(ply::kNeighbourE);
    check(!q.ok(), "an absent mask does not resolve");
    check(q.mask == ply::kNeighbourE, "the failed query names the mask it wanted");
    check(q.tileId == -1, "the failed query carries no tile id");

    // A raw mask that canonicalises to a present mask DOES resolve even in an
    // incomplete set: canonicalisation is applied before the lookup, so an
    // impossible corner falls back to a representable one. Mask 1 is absent
    // here, so raw 3 (N+NE) canonicalises to 1 and fails; raw 33 (31+NE)
    // canonicalises to 31, which is present, and resolves.
    check(!resolver.resolve(3).ok(), "raw 3 -> canonical 1, absent in this set");
    check(resolver.resolve(31 | 2).ok(),
          "raw 33 (31 + NE) canonicalises to 31, which is present");
}

// --- a large variant library: the count must not wrap --------------------

void testLargeVariantSet() {
    // F1 (found by the b2 verifier read, not by the unit's suite): the
    // variant counter was an unsigned char, so the 256th claimant of one mask
    // wrapped it to 0, the wrapped 0 re-triggered the first-wins branch, and
    // first-by-id silently returned a LATER tile. A terrain set is not
    // supposed to carry hundreds of variants of one mask, but the resolver's
    // first-by-id contract must not depend on that: the policy is pinned here
    // past the old 255 ceiling. 300 claimants, ids 0..299, so the winner must
    // be id 0 and the count must be 300. With the old unsigned char this test
    // fails with tileId 256 and variantCount 44 - the exact measured shape of
    // the bug.
    std::string tiles;
    for (int id = 0; id < 300; ++id) {
        if (id) tiles += ", ";
        tiles += tile(id, 5);
    }
    const ply::AutoTileResolver resolver =
        ply::AutoTileResolver::build(tilesetFrom(tiles.c_str()));

    check(resolver.variantCount(5) == 300, "300 claimants are all counted");
    check(resolver.resolve(5).ok(), "a 300-variant mask resolves");
    check(resolver.resolve(5).tileId == 0, "first-by-id still wins past 255 variants");
    check(resolver.resolve(5).tileId != 256,
          "the wrapped-to-256 winner of the old counter is gone");
    check(resolver.resolvedCount() == 1, "one mask resolved");
    // The table is unaffected by the count: the mask-5 slot holds id 0.
    const int slot5 = ply::blobMaskIndex(5);
    check(slot5 >= 0 && resolver.tileAt(static_cast<std::size_t>(slot5)) == 0,
          "the mask-5 slot holds id 0");
}

void printEvidence() {
    const ply::AutoTileResolver resolver = ply::AutoTileResolver::build(blob47Tileset());

    int rawResolved = 0;
    int distinctCanonical = 0;
    std::vector<int> seen(256, 0);
    for (int m = 0; m < 256; ++m) {
        if (resolver.resolve(m).ok()) ++rawResolved;
        const int c = ply::canonicalizeBlobMask(m);
        if (c >= 0 && c < 256 && !seen[static_cast<std::size_t>(c)]++) ++distinctCanonical;
    }

    std::printf(
        "EVIDENCE autotile masks=%d resolved=%zu complete=%d missing=%d raw=%d classes=%d "
        "fixed=%d ne_keep=%d ne_drop=%d first_id=%d variants=%zu\n",
        ply::kBlobMaskCount, resolver.resolvedCount(), resolver.complete() ? 1 : 0,
        resolver.firstMissingMask(), rawResolved, distinctCanonical,
        ply::canonicalizeBlobMask(255) == 255 ? 1 : 0,
        ply::canonicalizeBlobMask(1 | 4 | 2) == (1 | 4 | 2) ? 1 : 0,
        ply::canonicalizeBlobMask(1 | 2) == 1 ? 1 : 0,
        ply::AutoTileResolver::build(tilesetFrom((tile(9, 5) + ", " + tile(3, 5)).c_str()))
            .resolve(5)
            .tileId,
        resolver.variantCount(5));
}

}  // namespace

int main() {
    testCanonicalisation();
    testNeighbourMaskFromBits();
    testCompleteTerrain();
    testVariantPolicy();
    testIncompleteSet();
    testLargeVariantSet();

    if (failures == 0) {
        printEvidence();
        std::printf("autotile tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "autotile tests: %d failure(s)\n", failures);
    return 1;
}
