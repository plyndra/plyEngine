// TileAnimator (M1b b4, E-b4.1): "animated tile frame after n steps; phases
// differ per configured offset" — ARCHITECTURE_TARGET.md §3's "Per-tile frame
// sequences advanced from the sim clock; phase offset per cell so water
// doesn't pulse in lockstep".
//
// Pinned: the frame index after n steps over b1's fixture animation (tile 8:
// 4 frames x 12 steps, phase 3) as literal table entries at and across every
// frame boundary; variable per-frame step widths (no equal division); the
// per-cell phase offset semantics (phase == time shift, configured per tile
// AND per cell, two cells offset exactly as configured); static art (one
// frame); the loud unknown tile (frame -1, no plausible frame); exact
// reduction for huge clocks (past 2^31 — the b2 F1 lesson) and negative
// steps; and the loader-rejected steps<=0 state defended for hand-built sets.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/TileAnimator.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran).
int frameMatch = 0, frameRan = 0;
int phaseMatch = 0, phaseRan = 0;
int wrapMatch = 0, wrapRan = 0;
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

/// The b1.1 village fixture: tile 8 is animated water — 4 frames x 12 sim
/// steps each, phase 3 (its regions in strip order at x 0/16/32/48, y 32).
const ply::TileSet& village() {
    static const ply::TileSet ts = [] {
        const std::string path = std::string(PLY_TEST_DATA_DIR) + "/tileset_village.json";
        const ply::TileSetLoadResult loaded = ply::loadTileSetFile(path);
        if (!loaded.ok()) {
            std::fprintf(stderr, "  FAIL: village fixture does not load (%s)\n",
                         loaded.error.message.c_str());
            ++failures;
        }
        return loaded.tileset;
    }();
    return ts;
}

/// An ad-hoc tileset document (the autotile suite's tilesetFrom precedent).
ply::TileSet tilesetFrom(const std::string& tiles) {
    const std::string doc =
        "{\"name\": \"t\", \"image\": \"i.png\", \"tile_width\": 16, \"tile_height\": 16, "
        "\"tiles\": [" + tiles + "]}";
    const ply::TileSetLoadResult loaded = ply::loadTileSetFromString(doc);
    if (!loaded.ok()) {
        std::fprintf(stderr, "  FAIL: inline tileset does not load (%s)\n",
                     loaded.error.message.c_str());
        ++failures;
    }
    return loaded.tileset;
}

std::string frameJson(int x, int steps) {
    return "{\"region\": {\"x\": " + std::to_string(x) +
           ", \"y\": 0, \"w\": 16, \"h\": 16}, \"steps\": " + std::to_string(steps) + "}";
}

// --- frame index after n steps ----------------------------------------------

void testFrameIndex() {
    const ply::TileAnimator anim = ply::TileAnimator::build(village());

    pinAt(frameMatch, frameRan, anim.cycleOf(8) == 48, "tile 8 cycle 4x12 == 48");
    pinAt(phaseMatch, phaseRan, anim.phaseOf(8) == 3, "tile 8 configured phase 3");

    // Literal frame table at and across every boundary (clock = n + phase 3;
    // frames hold [0,12) [12,24) [24,36) [36,48), wrapping at 48).
    struct Case { long long n; int frame; };
    const Case cases[] = {
        {0, 0}, {8, 0},   // clock 3, 11 -> frame 0
        {9, 1}, {20, 1},  // clock 12, 23 -> frame 1
        {21, 2}, {32, 2}, // clock 24, 35 -> frame 2
        {33, 3}, {44, 3}, // clock 36, 47 -> frame 3
        {45, 0},          // clock 48 wraps -> frame 0
        {93, 0},          // clock 96 wraps twice -> frame 0
    };
    for (const Case& c : cases) {
        const int got = anim.frameIndexAt(8, c.n, 0);
        pinAt(frameMatch, frameRan, got == c.frame, "tile 8 frame after n steps");
    }

    // The frames' ART regions follow the index (strip order, pinned live
    // against the fixture metadata rather than literals).
    const ply::TileData* t8 = village().findTile(8);
    pinAt(frameMatch, frameRan,
          t8 != nullptr && anim.regionAt(8, 0, 0) == t8->animation[0].region &&
              anim.regionAt(8, 9, 0) == t8->animation[1].region &&
              anim.regionAt(8, 33, 0) == t8->animation[3].region,
          "regionAt returns the indexed frame's region");

    // Per-frame step widths are honoured — no equal division. Frames hold
    // 3, 5, 2 steps (cycle 10): boundaries at 0, 3, 8, 10.
    const ply::TileSet var = tilesetFrom(
        "{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"animation\": [" +
        frameJson(0, 3) + ", " + frameJson(16, 5) + ", " + frameJson(32, 2) + "]}");
    const ply::TileAnimator v = ply::TileAnimator::build(var);
    pinAt(frameMatch, frameRan, v.cycleOf(0) == 10, "variable-steps cycle 3+5+2 == 10");
    struct VCase { long long clock; int frame; };
    const VCase vcases[] = {
        {0, 0}, {2, 0}, {3, 1}, {7, 1}, {8, 2}, {9, 2}, {10, 0}, {23, 1},
    };
    for (const VCase& c : vcases) {
        pinAt(frameMatch, frameRan, v.frameIndexAt(0, c.clock, 0) == c.frame,
              "variable per-frame steps at the boundary");
    }

    // Static art: exactly one frame, always — the tile's own region.
    const ply::TileData* t0 = village().findTile(0);
    pinAt(frameMatch, frameRan,
          t0 != nullptr && t0->animation.empty() &&
              anim.frameIndexAt(0, 0, 0) == 0 && anim.frameIndexAt(0, 999999, 17) == 0,
          "static tile is frame 0 at any step and phase");
    pinAt(frameMatch, frameRan,
          t0 != nullptr && anim.regionAt(0, 123, 456) == t0->region,
          "static tile regionAt is the tile's own region");
}

// --- phases: per tile and per cell -------------------------------------------

void testPhases() {
    const ply::TileAnimator anim = ply::TileAnimator::build(village());

    // The per-cell phase offset is a TIME SHIFT: frame(t, n, p) ==
    // frame(t, n + p, 0) for any (n, p) — pinned as an identity over a
    // spread, so the offset cannot be a frame offset or a stride.
    bool identity = true;
    for (long long n = 0; n < 60; ++n) {
        for (long long p : {0LL, 1LL, 7LL, 12LL, 47LL, 48LL, 49LL, 100LL}) {
            identity = identity &&
                       anim.frameIndexAt(8, n, p) == anim.frameIndexAt(8, n + p, 0);
        }
    }
    pinAt(phaseMatch, phaseRan, identity, "cellPhase is a pure time shift");

    // THE LOCKSTEP CLAUSE: two cells of ONE tile with different phase
    // offsets show different frames at the same step — offset exactly as
    // configured (a 12-step offset on a 12-step frame is one frame).
    pinAt(phaseMatch, phaseRan,
          anim.frameIndexAt(8, 0, 0) == 0 && anim.frameIndexAt(8, 0, 12) == 1,
          "two cells, one tile: phases 0 vs 12 -> frames 0 vs 1 at step 0");
    pinAt(phaseMatch, phaseRan,
          anim.frameIndexAt(8, 5, 0) != anim.frameIndexAt(8, 5, 24),
          "same step, different phase offsets -> different frames");

    // Configured per TILE (ART_PIPELINE §7: "phase offsets per cell/tile
    // recorded in the tileset"): two water variants of one variant group
    // carrying phases 0 and 15 — the configured phases participate in the
    // clock automatically.
    const ply::TileSet water = tilesetFrom(
        "{\"id\": 0, \"region\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"variant\": \"water\", \"phase\": 0, \"animation\": [" +
        frameJson(0, 12) + ", " + frameJson(16, 12) + "]}, " +
        "{\"id\": 1, \"region\": {\"x\": 32, \"y\": 0, \"w\": 16, \"h\": 16}, "
        "\"variant\": \"water\", \"phase\": 15, \"animation\": [" +
        frameJson(0, 12) + ", " + frameJson(16, 12) + "]}");
    const ply::TileAnimator w = ply::TileAnimator::build(water);
    pinAt(phaseMatch, phaseRan,
          w.phaseOf(0) == 0 && w.phaseOf(1) == 15, "configured per-tile phases read back");
    pinAt(phaseMatch, phaseRan,
          w.frameIndexAt(0, 0, 0) == 0 && w.frameIndexAt(1, 0, 0) == 1,
          "two configured variants offset as configured (0 vs 15 -> f0 vs f1)");
}

// --- wrap, huge clocks, negative steps --------------------------------------

void testWrapAndWideClocks() {
    const ply::TileAnimator anim = ply::TileAnimator::build(village());

    // The F1 lesson: the clock must not wrap at a reachable value. These
    // steps are past 2^31 and 2^40; an int-narrowed clock fails them.
    pinAt(wrapMatch, wrapRan,
          anim.frameIndexAt(8, (1LL << 40) + 7, 0) == 2,
          "clock at 2^40+10 reduces exactly (frame 2)");
    pinAt(wrapMatch, wrapRan,
          anim.frameIndexAt(8, (1LL << 40) + 41, (1LL << 40) + 5) == 2,
          "huge step + huge phase reduce exactly (frame 2)");
    pinAt(wrapMatch, wrapRan,
          anim.frameIndexAt(8, (1LL << 31) + 9, 0) == 3,
          "clock just past 2^31 (frame 3; literal computed, not hand-waved)");

    // Negative steps reduce modulo the cycle into [0, cycle) — same frame a
    // full cycle earlier would show.
    pinAt(wrapMatch, wrapRan,
          anim.frameIndexAt(8, -6, 0) == 3 && anim.frameIndexAt(8, -51, 0) == 0,
          "negative steps reduce into [0, cycle)");
    pinAt(wrapMatch, wrapRan,
          anim.frameIndexAt(8, -6, 0) == anim.frameIndexAt(8, 42, 0),
          "negative step equals its positive counterpart one cycle over");
}

// --- loud unknowns and the hand-built defence --------------------------------

void testEdges() {
    const ply::TileAnimator anim = ply::TileAnimator::build(village());

    // An unknown tile is not animatable: loud, never a plausible frame.
    pinAt(edgeMatch, edgeRan,
          anim.frameIndexAt(999, 0, 0) == -1 && anim.frameIndexAt(-5, 100, 0) == -1,
          "unknown tile frame index is -1");
    pinAt(edgeMatch, edgeRan,
          anim.regionAt(999, 0, 0) == ply::UIRect{} && anim.cycleOf(999) == 0 &&
              anim.phaseOf(999) == 0,
          "unknown tile region/cycle/phase are empty, 0, 0");

    // The loader rejects steps <= 0 (NonPositiveSize); a hand-built TileSet
    // that bypasses the loader is defended by clamping such a frame to 1
    // step (frames (0, 2) -> effective (1, 2), cycle 3).
    ply::TileSet hand;
    hand.name = "hand";
    hand.tileWidth = 16;
    hand.tileHeight = 16;
    ply::TileData tile;
    tile.id = 0;
    tile.region = {0, 0, 16, 16};
    tile.animation.push_back({ply::UIRect{0, 0, 16, 16}, 0});
    tile.animation.push_back({ply::UIRect{16, 0, 16, 16}, 2});
    hand.tiles.push_back(tile);
    const ply::TileAnimator h = ply::TileAnimator::build(hand);
    pinAt(edgeMatch, edgeRan, h.cycleOf(0) == 3, "hand-built steps=0 clamps to 1 (cycle 3)");
    pinAt(edgeMatch, edgeRan,
          h.frameIndexAt(0, 0, 0) == 0 && h.frameIndexAt(0, 1, 0) == 1 &&
              h.frameIndexAt(0, 2, 0) == 1 && h.frameIndexAt(0, 3, 0) == 0,
          "clamped frame boundaries land as if steps were 1");
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const ply::TileAnimator anim = ply::TileAnimator::build(village());
    const ply::TileData* t8 = village().findTile(8);
    std::printf(
        "EVIDENCE tile_animator frames=%zu cycle=%lld phase=%d frame=%d/%d phase=%d/%d "
        "wrap=%d/%d edge=%d/%d wide_frame=%d\n",
        t8 != nullptr ? t8->animation.size() : 0, anim.cycleOf(8), anim.phaseOf(8),
        frameMatch, frameRan, phaseMatch, phaseRan, wrapMatch, wrapRan, edgeMatch, edgeRan,
        anim.frameIndexAt(8, (1LL << 40) + 7, 0));
}

}  // namespace

int main() {
    testFrameIndex();
    testPhases();
    testWrapAndWideClocks();
    testEdges();

    if (failures == 0) {
        printEvidence();
        std::printf("tile_animator tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "tile_animator tests: %d failure(s)\n", failures);
    return 1;
}
