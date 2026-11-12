// Benchmark (M1b b10): the headless harness over the real submission path.
// The contract — the sweep, the three metrics, the p95 convention, the
// budget check — is documented in Benchmark.hpp; this file is that
// contract. Deterministic by construction: no clock anywhere.
#include "plyEngine/Benchmark.hpp"

#include <algorithm>
#include <vector>

namespace ply {

long long workP95(std::vector<long long> series) {
    if (series.empty()) return 0;
    std::sort(series.begin(), series.end());
    const long long n = static_cast<long long>(series.size());
    // The smallest v with at least 95% of the steps <= v: the element at
    // rank ceil(95n/100), one-based (p95 of 1..100 is 95).
    long long rank = (95 * n + 99) / 100;
    if (rank < 1) rank = 1;
    if (rank > n) rank = n;
    return series[static_cast<std::size_t>(rank - 1)];
}

BenchmarkResult runBenchmark(AutotileMap& map, const BenchmarkConfig& config) {
    BenchmarkResult result;
    const int frames = config.frames > 0 ? config.frames : 0;
    result.frames = frames;
    long long quadsSum = 0;
    long long drawSum = 0;
    std::vector<long long> work;
    bool within = true;
    const int mapWidthPx = map.mapWidthPx();
    const int mapHeightPx = map.mapHeightPx();
    const int cellsX = map.widthCells();
    const int cellsY = map.heightCells();
    for (int i = 0; i < frames; ++i) {
        const UIRect camera{mapWidthPx > 0 ? (i * 16) % mapWidthPx : 0,
                            mapHeightPx > 0 ? (i * 8) % mapHeightPx : 0, 640, 360};
        const WorldMapSubmission submission = map.submitResolved(camera);
        const long long quads = static_cast<long long>(submission.below.size()) +
                                static_cast<long long>(submission.above.size());
        quadsSum += quads;
        if (quads > result.quadsMax) result.quadsMax = quads;
        if (quads > config.quadsBudget) within = false;
        // Draw calls: the non-empty (pass, layer) groups.
        long long draws = 0;
        std::vector<std::pair<int, std::size_t>> groups;
        for (const WorldMapQuad& quad : submission.below) groups.emplace_back(0, quad.layer);
        for (const WorldMapQuad& quad : submission.above) groups.emplace_back(1, quad.layer);
        std::sort(groups.begin(), groups.end());
        groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
        draws = static_cast<long long>(groups.size());
        drawSum += draws;
        // The work proxy: the visible cell count.
        long long visible = 0;
        for (int cy = 0; cy < cellsY; ++cy) {
            for (int cx = 0; cx < cellsX; ++cx) {
                const UIRect cell{cx * 16, (cellsY - 1 - cy) * 16, 16, 16};
                if (cell.x < camera.x + camera.w && camera.x < cell.x + cell.w &&
                    cell.y < camera.y + camera.h && camera.y < cell.y + cell.h) {
                    ++visible;
                }
            }
        }
        work.push_back(visible);
    }
    if (frames > 0) {
        result.quadsAvg = quadsSum / frames;
        result.drawCalls = drawSum / frames;
    }
    result.stepP95 = workP95(std::move(work));
    result.withinBudget = within;
    return result;
}

}  // namespace ply
