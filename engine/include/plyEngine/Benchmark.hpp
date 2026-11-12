#pragma once

#include "plyEngine/AutotileMap.hpp"

namespace ply {

// Benchmark (M1b b10, ROADMAP: "Headless benchmark harness — emits
// quads/frame, draw calls, step p95 against the budget"; E-b10.1:
// "benchmark emits quads/frame, draw calls, step p95"). It drives the REAL
// submission path (AutotileMap::submitResolved along a scripted camera
// path) and emits the three metrics deterministically.
//
// DETERMINISM CONTRACT (stated once, ADR 0014's rule): a wall-clock
// benchmark cannot live in a byte-stable transcript or in CI — llvmpipe and
// neighbours make timings nondeterministic by construction. So "step p95"
// is the p95 of the per-step deterministic WORK counter — the number of
// cells the submission path visited that step — which correlates with cost,
// is reproducible on every host, and is pinnable. The timing harness is a
// local/M2 concern; this unit measures the WORK against the budget.
//
// The budget (ADR 0012): quads per internal frame <= 4000. The harness
// checks every frame against it and reports withinBudget.
//
// Definitions, exact:
// - the sweep: frame i's camera is { (i*16) % max(1, mapWidthPx),
//   (i*8) % max(1, mapHeightPx), 640, 360 }.
// - quads/frame: the quads one submitResolved call returns for the frame's
//   camera rect (below + above).
// - draw calls for a frame: the number of NON-EMPTY (pass, layer) groups in
//   the submission — one batch per pass per layer that has content.
// - step work: the VISIBLE CELL COUNT for the frame's camera — the cells
//   whose world rect overlaps it (a geometric, deterministic cost proxy for
//   the per-frame submission work).
// - step p95: the 95th percentile of the step-work series over the run,
//   defined as the SMALLEST value v such that at least 95% of the steps
//   have work <= v (a nearest-rank convention, pinned on synthetic series:
//   p95 of 1..100 == 95).

struct BenchmarkConfig {
    int frames = 120;          // frames to simulate
    int quadsBudget = 4000;    // ADR 0012's per-frame ceiling
};

struct BenchmarkResult {
    int frames = 0;
    long long quadsAvg = 0;      // mean quads per frame (integer mean)
    long long quadsMax = 0;      // the worst frame
    long long drawCalls = 0;     // mean draw calls per frame (integer mean)
    long long stepP95 = 0;       // the work p95 (see the contract above)
    bool withinBudget = false;   // every frame within quadsBudget

    bool operator==(const BenchmarkResult&) const = default;
};

/// The p95 of a work series (the nearest-rank convention above). Exposed so
/// the p95 definition itself is pin-able on a synthetic series.
long long workP95(std::vector<long long> series);

/// Runs the scripted camera path (a deterministic sweep across the map) and
/// emits the result. Headless and pure data: no GL, no window, no clock.
BenchmarkResult runBenchmark(AutotileMap& map, const BenchmarkConfig& config);

}  // namespace ply
