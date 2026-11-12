// Benchmark (M1b b10, E-b10.1): "benchmark emits quads/frame, draw calls,
// step p95" — the headless harness over the REAL submission path.
//
// Pinned: the p95 definition itself on synthetic series (the nearest-rank
// convention, literal); the harness's aggregation re-derived independently
// over the same documented camera sweep (quadsAvg/quadsMax/drawCalls/
// stepP95 all recomputed in the grader); the ADR-0012 budget flag both
// ways; and determinism (two runs identical). The determinism contract is
// the header's: no wall-clock anywhere — the step p95 is the work proxy.
//
// House rules: pure data. No GL, no window, NO CLOCK, no float.

#include "plyEngine/Benchmark.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

int p95Match = 0, p95Ran = 0;
int metricsMatch = 0, metricsRan = 0;
int budgetMatch = 0, budgetRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

// --- the p95 definition, on synthetic series --------------------------------

void testP95Math() {
    std::vector<long long> ramp;
    for (long long i = 1; i <= 100; ++i) ramp.push_back(i);
    pinAt(p95Match, p95Ran, ply::workP95(ramp) == 95,
          "p95 of 1..100 is 95 (the nearest-rank convention)");
    pinAt(p95Match, p95Ran, ply::workP95({}) == 0, "p95 of an empty series is 0");
    pinAt(p95Match, p95Ran, ply::workP95({7}) == 7, "p95 of a single value is that value");
    // Ten 10s and one 100: the smallest v with >= 95% of 11 steps at most v
    // is 100 (10.45 steps must be covered -> all 11).
    pinAt(p95Match, p95Ran,
          ply::workP95({10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 100}) == 100,
          "p95 of a skewed series takes the tail (100)");
    // 20 values: the smallest v covering 19 of 20 (95%) is the 19th smallest.
    pinAt(p95Match, p95Ran,
          ply::workP95({5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 60}) == 5,
          "p95 ignores the single top outlier at n=20 (19/20 = 95%)");
}

// --- the fixture: a grass-only AutotileMap over the real path ---------------

ply::TileSet grassTileset() {
    ply::TileSet ts;
    ts.name = "grass";
    ts.image = "g.png";
    ts.tileWidth = 16;
    ts.tileHeight = 16;
    for (int i = 0; i < ply::kBlobMaskCount; ++i) {
        ply::TileData tile;
        tile.id = i;
        tile.region = {0, i * 16, 16, 16};
        tile.mask = ply::kBlobMasks[i];
        tile.variant = "grass";
        ts.tiles.push_back(tile);
    }
    return ts;
}

ply::MapData grassMap() {
    ply::MapData map;
    map.name = "bench";
    map.tileset = "grass";
    map.width = 20;
    map.height = 12;
    ply::MapLayer ground;
    ground.name = "ground";
    for (int i = 0; i < 20 * 12; ++i) ground.tiles.push_back(i % 7 == 0 ? -1 : (i % 3 == 0 ? 0 : 7));
    map.layers.push_back(ground);
    return map;
}

// The documented sweep, re-derived here: frame i's camera is
// {(i*16) % max(1, mapWidthPx), (i*8) % max(1, mapHeightPx), 640, 360};
// quads = below+above; drawCalls = distinct non-empty (pass, layer) groups;
// work = the visible cell count (cells whose world rect overlaps the camera).
ply::UIRect cameraAt(int frame, int mapWidthPx, int mapHeightPx) {
    const int x = mapWidthPx > 0 ? (frame * 16) % mapWidthPx : 0;
    const int y = mapHeightPx > 0 ? (frame * 8) % mapHeightPx : 0;
    return {x, y, 640, 360};
}

long long visibleCells(const ply::UIRect& cam, int cellsX, int cellsY) {
    long long count = 0;
    for (int cy = 0; cy < cellsY; ++cy) {
        for (int cx = 0; cx < cellsX; ++cx) {
            const ply::UIRect cell{cx * 16, (cellsY - 1 - cy) * 16, 16, 16};
            if (cell.x < cam.x + cam.w && cam.x < cell.x + cell.w &&
                cell.y < cam.y + cam.h && cam.y < cell.y + cell.h) {
                ++count;
            }
        }
    }
    return count;
}

void testHarnessAggregation() {
    const ply::TileSet ts = grassTileset();
    const ply::MapData map = grassMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);

    ply::BenchmarkConfig config;
    config.frames = 60;
    const ply::BenchmarkResult result = ply::runBenchmark(autotile, config);

    // Independent recomputation over the documented sweep.
    long long quadsSum = 0, quadsMax = 0, drawSum = 0;
    std::vector<long long> work;
    for (int frame = 0; frame < 60; ++frame) {
        const ply::UIRect cam = cameraAt(frame, autotile.mapWidthPx(), autotile.mapHeightPx());
        const ply::WorldMapSubmission sub = autotile.submitResolved(cam);
        const long long quads =
            static_cast<long long>(sub.below.size()) + static_cast<long long>(sub.above.size());
        quadsSum += quads;
        if (quads > quadsMax) quadsMax = quads;
        long long draws = 0;
        for (const ply::WorldMapQuad& quad : sub.below) {
            (void)quad;
            ++draws;
            break;
        }
        for (const ply::WorldMapQuad& quad : sub.above) {
            (void)quad;
            ++draws;
            break;
        }
        drawSum += draws;
        work.push_back(visibleCells(cam, autotile.widthCells(), autotile.heightCells()));
    }
    pinAt(metricsMatch, metricsRan,
          result.frames == 60 && result.quadsMax == quadsMax &&
              result.quadsAvg == quadsSum / 60 && result.drawCalls == drawSum / 60,
          "quads/frame and draw calls match the independent aggregation");
    pinAt(metricsMatch, metricsRan,
          result.stepP95 == ply::workP95(work),
          "step p95 matches the p95 of the independent work series");
    pinAt(metricsMatch, metricsRan,
          result.quadsMax > 0 && result.quadsAvg > 0, "the run actually drew something");

    // The draw-calls definition spans LAYERS, not just passes (injection
    // design showed the single-layer fixture can't tell "one per pass" from
    // "one per (pass, layer)"): a two-layer map draws two batches in the
    // below pass.
    ply::MapData twoLayers = grassMap();
    ply::MapLayer canopy;
    canopy.name = "canopy";
    for (int i = 0; i < 20 * 12; ++i) canopy.tiles.push_back(7);
    twoLayers.layers.push_back(canopy);
    ply::AutotileMap layered = ply::AutotileMap::build(twoLayers, ts);
    pinAt(metricsMatch, metricsRan,
          ply::runBenchmark(layered, config).drawCalls == 2,
          "draw calls count (pass, layer) groups: two below layers -> 2");
}

void testBudgetAndDeterminism() {
    const ply::TileSet ts = grassTileset();
    const ply::MapData map = grassMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);

    ply::BenchmarkConfig config;
    config.frames = 60;
    const ply::BenchmarkResult under = ply::runBenchmark(autotile, config);
    pinAt(budgetMatch, budgetRan, under.withinBudget,
          "the fixture run is within the ADR-0012 budget (<= 4000 quads)");

    ply::BenchmarkConfig tight = config;
    tight.quadsBudget = 5;
    pinAt(budgetMatch, budgetRan,
          !ply::runBenchmark(autotile, tight).withinBudget,
          "an exceeded budget flips the flag (the check is live)");

    // Determinism: two identical runs produce identical results (no clock,
    // no wall-clock anywhere — the header's contract).
    ply::AutotileMap again = ply::AutotileMap::build(map, ts);
    pinAt(budgetMatch, budgetRan,
          ply::runBenchmark(again, config) == under,
          "two runs are identical (the harness is deterministic)");
}

void printEvidence() {
    const ply::TileSet ts = grassTileset();
    const ply::MapData map = grassMap();
    ply::AutotileMap autotile = ply::AutotileMap::build(map, ts);
    ply::BenchmarkConfig config;
    config.frames = 120;
    const ply::BenchmarkResult result = ply::runBenchmark(autotile, config);
    std::printf(
        "EVIDENCE benchmark frames=%d quads_avg=%lld quads_max=%lld draw_calls=%lld "
        "step_p95=%lld budget=%s quads_budget=%d p95=%d/%d metrics=%d/%d budgetpins=%d/%d\n",
        result.frames, result.quadsAvg, result.quadsMax, result.drawCalls, result.stepP95,
        result.withinBudget ? "ok" : "OVER", config.quadsBudget, p95Match, p95Ran,
        metricsMatch, metricsRan, budgetMatch, budgetRan);
}

}  // namespace

int main() {
    testP95Math();
    testHarnessAggregation();
    testBudgetAndDeterminism();

    if (failures == 0) {
        printEvidence();
        std::printf("benchmark tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "benchmark tests: %d failure(s)\n", failures);
    return 1;
}
