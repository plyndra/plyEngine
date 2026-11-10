// Headless UI layout tests: E-a4.1 (9 anchors x 3 logical sizes x 3 scales ->
// expected rects, integer-exact), E-a4.2 (every emitted rect is integral after
// layout) and E-a4.3 (layout hash identical across two processes), plus the
// dirty-layout cache (UI_FRAMEWORK.md §2 rule 4). No GL context, no window, no
// clock, no float anywhere in the layout path — the hash is FNV-1a 64 over
// explicit little-endian integers (StructuralHash), never std::hash.
//
// E-a4.3 runs the hash in a literal second process: the suite re-executes
// itself with --layout-hash-child and compares the printed value against the
// in-process hash.

#include "plyEngine/UILayout.hpp"
#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
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

void eqHash(std::uint64_t got, std::uint64_t want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got 0x%016llx, want 0x%016llx)\n", what,
                     static_cast<unsigned long long>(got),
                     static_cast<unsigned long long>(want));
        ++failures;
    }
}

using ply::UIAnchor;
using A = UIAnchor;

// --- the anchor x size x scale matrix ---------------------------------------

/// One expected rect, hand-pinned as a literal (generated independently with
/// floor arithmetic, then frozen). The grader is the table: any change to the
/// anchor mapping, the offset handling, the scale application or the snapping
/// changes a coordinate and fails here.
struct MatrixCase {
    UIAnchor anchor;
    int logicalW, logicalH;
    int scale;
    int x, y, w, h;
};

// Widget: 33x13 source-art px (odd, so centre halving produces real halves),
// offset (8, 5) from the anchored edge.
constexpr int kW = 33, kH = 13, kOffX = 8, kOffY = 5;
constexpr std::size_t kMatrixRows = 81;  // 9 anchors x 3 sizes x 3 scales

constexpr MatrixCase kMatrix[] = {
    // clang-format off
    {A::TopLeft, 640, 360, 1, 8, 5, 33, 13},
    {A::TopCentre, 640, 360, 1, 311, 5, 33, 13},
    {A::TopRight, 640, 360, 1, 599, 5, 33, 13},
    {A::MidLeft, 640, 360, 1, 8, 178, 33, 13},
    {A::Centre, 640, 360, 1, 311, 178, 33, 13},
    {A::MidRight, 640, 360, 1, 599, 178, 33, 13},
    {A::BottomLeft, 640, 360, 1, 8, 342, 33, 13},
    {A::BottomCentre, 640, 360, 1, 311, 342, 33, 13},
    {A::BottomRight, 640, 360, 1, 599, 342, 33, 13},
    {A::TopLeft, 640, 360, 2, 8, 5, 66, 26},
    {A::TopCentre, 640, 360, 2, 295, 5, 66, 26},
    {A::TopRight, 640, 360, 2, 566, 5, 66, 26},
    {A::MidLeft, 640, 360, 2, 8, 172, 66, 26},
    {A::Centre, 640, 360, 2, 295, 172, 66, 26},
    {A::MidRight, 640, 360, 2, 566, 172, 66, 26},
    {A::BottomLeft, 640, 360, 2, 8, 329, 66, 26},
    {A::BottomCentre, 640, 360, 2, 295, 329, 66, 26},
    {A::BottomRight, 640, 360, 2, 566, 329, 66, 26},
    {A::TopLeft, 640, 360, 3, 8, 5, 99, 39},
    {A::TopCentre, 640, 360, 3, 278, 5, 99, 39},
    {A::TopRight, 640, 360, 3, 533, 5, 99, 39},
    {A::MidLeft, 640, 360, 3, 8, 165, 99, 39},
    {A::Centre, 640, 360, 3, 278, 165, 99, 39},
    {A::MidRight, 640, 360, 3, 533, 165, 99, 39},
    {A::BottomLeft, 640, 360, 3, 8, 316, 99, 39},
    {A::BottomCentre, 640, 360, 3, 278, 316, 99, 39},
    {A::BottomRight, 640, 360, 3, 533, 316, 99, 39},
    {A::TopLeft, 512, 288, 1, 8, 5, 33, 13},
    {A::TopCentre, 512, 288, 1, 247, 5, 33, 13},
    {A::TopRight, 512, 288, 1, 471, 5, 33, 13},
    {A::MidLeft, 512, 288, 1, 8, 142, 33, 13},
    {A::Centre, 512, 288, 1, 247, 142, 33, 13},
    {A::MidRight, 512, 288, 1, 471, 142, 33, 13},
    {A::BottomLeft, 512, 288, 1, 8, 270, 33, 13},
    {A::BottomCentre, 512, 288, 1, 247, 270, 33, 13},
    {A::BottomRight, 512, 288, 1, 471, 270, 33, 13},
    {A::TopLeft, 512, 288, 2, 8, 5, 66, 26},
    {A::TopCentre, 512, 288, 2, 231, 5, 66, 26},
    {A::TopRight, 512, 288, 2, 438, 5, 66, 26},
    {A::MidLeft, 512, 288, 2, 8, 136, 66, 26},
    {A::Centre, 512, 288, 2, 231, 136, 66, 26},
    {A::MidRight, 512, 288, 2, 438, 136, 66, 26},
    {A::BottomLeft, 512, 288, 2, 8, 257, 66, 26},
    {A::BottomCentre, 512, 288, 2, 231, 257, 66, 26},
    {A::BottomRight, 512, 288, 2, 438, 257, 66, 26},
    {A::TopLeft, 512, 288, 3, 8, 5, 99, 39},
    {A::TopCentre, 512, 288, 3, 214, 5, 99, 39},
    {A::TopRight, 512, 288, 3, 405, 5, 99, 39},
    {A::MidLeft, 512, 288, 3, 8, 129, 99, 39},
    {A::Centre, 512, 288, 3, 214, 129, 99, 39},
    {A::MidRight, 512, 288, 3, 405, 129, 99, 39},
    {A::BottomLeft, 512, 288, 3, 8, 244, 99, 39},
    {A::BottomCentre, 512, 288, 3, 214, 244, 99, 39},
    {A::BottomRight, 512, 288, 3, 405, 244, 99, 39},
    {A::TopLeft, 320, 180, 1, 8, 5, 33, 13},
    {A::TopCentre, 320, 180, 1, 151, 5, 33, 13},
    {A::TopRight, 320, 180, 1, 279, 5, 33, 13},
    {A::MidLeft, 320, 180, 1, 8, 88, 33, 13},
    {A::Centre, 320, 180, 1, 151, 88, 33, 13},
    {A::MidRight, 320, 180, 1, 279, 88, 33, 13},
    {A::BottomLeft, 320, 180, 1, 8, 162, 33, 13},
    {A::BottomCentre, 320, 180, 1, 151, 162, 33, 13},
    {A::BottomRight, 320, 180, 1, 279, 162, 33, 13},
    {A::TopLeft, 320, 180, 2, 8, 5, 66, 26},
    {A::TopCentre, 320, 180, 2, 135, 5, 66, 26},
    {A::TopRight, 320, 180, 2, 246, 5, 66, 26},
    {A::MidLeft, 320, 180, 2, 8, 82, 66, 26},
    {A::Centre, 320, 180, 2, 135, 82, 66, 26},
    {A::MidRight, 320, 180, 2, 246, 82, 66, 26},
    {A::BottomLeft, 320, 180, 2, 8, 149, 66, 26},
    {A::BottomCentre, 320, 180, 2, 135, 149, 66, 26},
    {A::BottomRight, 320, 180, 2, 246, 149, 66, 26},
    {A::TopLeft, 320, 180, 3, 8, 5, 99, 39},
    {A::TopCentre, 320, 180, 3, 118, 5, 99, 39},
    {A::TopRight, 320, 180, 3, 213, 5, 99, 39},
    {A::MidLeft, 320, 180, 3, 8, 75, 99, 39},
    {A::Centre, 320, 180, 3, 118, 75, 99, 39},
    {A::MidRight, 320, 180, 3, 213, 75, 99, 39},
    {A::BottomLeft, 320, 180, 3, 8, 136, 99, 39},
    {A::BottomCentre, 320, 180, 3, 118, 136, 99, 39},
    {A::BottomRight, 320, 180, 3, 213, 136, 99, 39},
    // clang-format on
};

/// The same 81 widgets, as layout inputs: the matrix spec list the hash runs
/// over (shared with the second-process child).
std::vector<ply::UIWidgetSpec> matrixWidgets() {
    std::vector<ply::UIWidgetSpec> widgets;
    widgets.reserve(kMatrixRows);
    for (const MatrixCase& row : kMatrix) {
        ply::UIWidgetSpec spec;
        spec.anchor = row.anchor;
        spec.offsetX = kOffX;
        spec.offsetY = kOffY;
        spec.width = kW;
        spec.height = kH;
        spec.scale = row.scale;
        widgets.push_back(spec);
    }
    return widgets;
}

// --- E-a4.1: the matrix ------------------------------------------------------

void testAnchorMatrix() {
    eq(static_cast<long long>(sizeof(kMatrix) / sizeof(kMatrix[0])),
       static_cast<long long>(kMatrixRows), "matrix table has all 81 rows");

    std::vector<ply::UIWidgetSpec> widgets;
    widgets.reserve(kMatrixRows);
    for (const MatrixCase& row : kMatrix) {
        ply::UIWidgetSpec spec;
        spec.anchor = row.anchor;
        spec.offsetX = kOffX;
        spec.offsetY = kOffY;
        spec.width = kW;
        spec.height = kH;
        spec.scale = row.scale;
        widgets.push_back(spec);
    }

    const std::vector<ply::UIRect> rects = ply::layoutWidgets(widgets, 640, 360);
    // Every row is laid out against its own logical size, so the matrix is
    // 81 independent one-widget layouts, not one batch.
    std::vector<ply::UIRect> all;
    all.reserve(kMatrixRows);
    for (std::size_t i = 0; i < kMatrixRows; ++i) {
        const MatrixCase& row = kMatrix[i];
        const std::vector<ply::UIWidgetSpec> one = {widgets[i]};
        const std::vector<ply::UIRect> out = ply::layoutWidgets(one, row.logicalW, row.logicalH);
        eq(static_cast<long long>(out.size()), 1, "one widget in, one rect out");
        if (out.size() != 1) return;
        eq(out[0].x, row.x, "rect x matches the pinned table");
        eq(out[0].y, row.y, "rect y matches the pinned table");
        eq(out[0].w, row.w, "rect w is the scaled art width");
        eq(out[0].h, row.h, "rect h is the scaled art height");
        all.push_back(out[0]);
    }
    eq(static_cast<long long>(all.size()), static_cast<long long>(kMatrixRows),
       "matrix produced 81 rects");

    // Five rows re-derived by hand here, so the table is anchored to arithmetic
    // a reader can redo on paper, not to the generator that wrote it.
    {
        // TopLeft at 640x360 s1: near edges, offset is the position.
        const ply::UIWidgetSpec spec{A::TopLeft, kOffX, kOffY, kW, kH, 1};
        const ply::UIRect r = ply::layoutWidgets({spec}, 640, 360)[0];
        eq(r.x, 8, "hand: TopLeft x = offset");
        eq(r.y, 5, "hand: TopLeft y = offset");
    }
    {
        // BottomRight at 640x360 s1: far edge minus offset minus size.
        const ply::UIWidgetSpec spec{A::BottomRight, kOffX, kOffY, kW, kH, 1};
        const ply::UIRect r = ply::layoutWidgets({spec}, 640, 360)[0];
        eq(r.x, 640 - 8 - 33, "hand: BottomRight x = W - offsetX - w");
        eq(r.y, 360 - 5 - 13, "hand: BottomRight y = H - offsetY - h");
    }
    {
        // Centre at 640x360 s1: (2*off + W - sw) / 2 = (16 + 607) / 2 = 311.5,
        // snapped down; (10 + 347) / 2 = 178.5, snapped down.
        const ply::UIWidgetSpec spec{A::Centre, kOffX, kOffY, kW, kH, 1};
        const ply::UIRect r = ply::layoutWidgets({spec}, 640, 360)[0];
        eq(r.x, 311, "hand: Centre x = floor((2*off + W - sw)/2)");
        eq(r.y, 178, "hand: Centre y = floor((2*off + H - sh)/2)");
    }
    {
        // Centre at scale 2: sw = 66, (16 + 574) / 2 = 295 exactly.
        const ply::UIWidgetSpec spec{A::Centre, kOffX, kOffY, kW, kH, 2};
        const ply::UIRect r = ply::layoutWidgets({spec}, 640, 360)[0];
        eq(r.x, 295, "hand: Centre s2 x is exact (no half)");
        eq(r.y, 172, "hand: Centre s2 y = floor((10+360-26)/2)");
    }
    {
        // Centre at scale 3: sw = 99, (16 + 541) / 2 = 278.5, snapped down.
        const ply::UIWidgetSpec spec{A::Centre, kOffX, kOffY, kW, kH, 3};
        const ply::UIRect r = ply::layoutWidgets({spec}, 640, 360)[0];
        eq(r.x, 278, "hand: Centre s3 x = floor((2*off + W - sw)/2)");
        eq(r.y, 165, "hand: Centre s3 y = floor((10+360-39)/2)");
    }
}

// --- E-a4.2: integer snapping ------------------------------------------------

void testIntegerSnapping() {
    // The centre-line division is the one place a half-pixel can arise. These
    // cases make the numerator odd and negative, so floor (the contract),
    // C++ truncation and round-half-away all disagree.
    // At 100x100 with a 13x13 widget: numerator = 2*off + 100 - 13 = 2*off + 87.
    struct SnapCase {
        int offsetX;
        int numerator;
        int floored;
    };
    const SnapCase cases[] = {
        {0, 87, 43},    // odd positive: floor == trunc, still pinned
        {-50, -13, -7}, // trunc gives -6, round-half-away gives -6
        {-46, -5, -3},  // trunc gives -2
        {-44, -1, -1},  // trunc gives 0
    };
    for (const SnapCase& snap : cases) {
        const ply::UIWidgetSpec spec{A::Centre, snap.offsetX, 0, 13, 13, 1};
        const ply::UIRect r = ply::layoutWidgets({spec}, 100, 100)[0];
        eq(r.x, snap.floored, "centre x snaps by flooring (toward -inf)");
        check(r.w == 13 && r.h == 13, "snapped rect keeps its scaled size");
        eq(r.y, 43, "unaffected axis snaps the same way (floor(87/2))");
    }
    // Same on the vertical axis.
    {
        const ply::UIWidgetSpec spec{A::Centre, 0, -44, 13, 13, 1};
        const ply::UIRect r = ply::layoutWidgets({spec}, 100, 100)[0];
        eq(r.y, -1, "centre y snaps by flooring (toward -inf)");
        eq(r.x, 43, "unaffected axis uses the same rule");
    }

    // Scale multiplies the source art *before* layout rounding: 33 px at scale
    // 1 has a half-pixel (303.5), at scale 2 it is exact (287), at scale 3 it
    // has one again (270.5). Rounding a halved size instead would give
    // 303/287/270 from different arithmetic and still pass — the point is the
    // exact pinned triple below.
    {
        const ply::UIWidgetSpec s1{A::Centre, 0, 0, 33, 13, 1};
        const ply::UIWidgetSpec s2{A::Centre, 0, 0, 33, 13, 2};
        const ply::UIWidgetSpec s3{A::Centre, 0, 0, 33, 13, 3};
        const ply::UIRect r1 = ply::layoutWidgets({s1}, 640, 360)[0];
        const ply::UIRect r2 = ply::layoutWidgets({s2}, 640, 360)[0];
        const ply::UIRect r3 = ply::layoutWidgets({s3}, 640, 360)[0];
        eq(r1.x, 303, "s1 centre x = floor((640-33)/2)");
        eq(r2.x, 287, "s2 centre x = (640-66)/2 exactly");
        eq(r3.x, 270, "s3 centre x = floor((640-99)/2)");
    }
}

/// E-a4.2's own loop: every emitted rect has integral x, y, w, h (compared
/// field-by-field against the pinned integer table) and a positive size.
/// Returns how many rects were checked, for the transcript line.
std::size_t testIntegralRects() {
    std::size_t integral = 0;
    std::vector<ply::UIWidgetSpec> widgets;
    widgets.reserve(kMatrixRows);
    for (const MatrixCase& row : kMatrix) {
        const ply::UIWidgetSpec spec{row.anchor, kOffX, kOffY, kW, kH, row.scale};
        widgets.push_back(spec);
    }
    std::size_t i = 0;
    for (const MatrixCase& row : kMatrix) {
        const ply::UIRect r = ply::layoutWidgets({widgets[i]}, row.logicalW, row.logicalH)[0];
        if (r.x == row.x && r.y == row.y && r.w == row.w && r.h == row.h && r.w > 0 && r.h > 0)
            ++integral;
        else
            check(false, "rect is integral and positive at every matrix row");
        ++i;
    }
    eq(static_cast<long long>(integral), static_cast<long long>(kMatrixRows),
       "all 81 matrix rects are integral and positive");
    return integral;
}

// --- hash field coverage ------------------------------------------------------

void testHashFields() {
    const std::vector<ply::UIWidgetSpec> widgets = matrixWidgets();
    const std::vector<ply::UIRect> rects = ply::layoutWidgets(widgets, 640, 360);
    const std::uint64_t base = ply::structuralHash(rects);

    // Reproduced from scratch, not compared to itself.
    eqHash(ply::structuralHash(ply::layoutWidgets(widgets, 640, 360)), base,
           "layout hash is reproducible across recomputation");

    // One changed offset changes one rect -> the hash moves.
    {
        std::vector<ply::UIWidgetSpec> nudged = widgets;
        nudged[0].offsetX += 1;
        check(ply::structuralHash(ply::layoutWidgets(nudged, 640, 360)) != base,
              "hash covers widget offsets");
    }
    // A different logical size moves far-anchored rects -> the hash moves.
    check(ply::structuralHash(ply::layoutWidgets(widgets, 641, 360)) != base,
          "hash covers the logical size");
    // Order is part of the contract.
    {
        std::vector<ply::UIWidgetSpec> swapped = widgets;
        std::swap(swapped[0], swapped[1]);
        check(ply::structuralHash(ply::layoutWidgets(swapped, 640, 360)) != base,
              "hash covers rect order");
    }
    // An empty layout hashes the count only, and differs from any one-rect hash.
    check(ply::structuralHash({}) != ply::structuralHash(rects), "empty layout hashes differently");
}

// --- E-a4.3: the hash in a second process -------------------------------------

std::uint64_t matrixLayoutHash() {
    return ply::structuralHash(ply::layoutWidgets(matrixWidgets(), 640, 360));
}

/// Runs this executable with --layout-hash-child and reads the hash it prints.
/// Returns false when a second process is not available (non-Linux) or the
/// child failed.
bool secondProcessHash(std::uint64_t& outHash) {
#if PLY_HAVE_SELF_EXE
    char path[4096];
    const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) return false;
    path[len] = '\0';

    std::string command = std::string("'") + path + "' --layout-hash-child";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return false;

    unsigned long long value = 0;
    const int read = std::fscanf(pipe, "LAYOUT_HASH %llx", &value);
    const int status = pclose(pipe);
    if (read != 1 || status != 0) return false;
    outHash = value;
    return true;
#else
    (void)outHash;
    return false;
#endif
}

void testTwoProcessHash() {
    const std::uint64_t here = matrixLayoutHash();

    std::uint64_t there = 0;
    const bool ran = secondProcessHash(there);
    if (PLY_HAVE_SELF_EXE) {
        check(ran, "second process ran");
        if (ran) eqHash(there, here, "layout hash identical across two processes");
        // And in this process, from scratch, not the same vector.
        eqHash(matrixLayoutHash(), here, "layout hash reproducible in-process");
    } else {
        // No /proc/self/exe: the best available demonstration is recomputation
        // in-process; the transcript then does not claim cross-process.
        eqHash(matrixLayoutHash(), here, "layout hash reproducible (in-process fallback)");
    }
}

// --- dirty-layout cache --------------------------------------------------------

int testLayoutCache() {
    const std::vector<ply::UIWidgetSpec> widgets = matrixWidgets();

    ply::UILayoutCache cache;
    const std::vector<ply::UIRect>& first = cache.rects(widgets, 640, 360);
    eq(static_cast<long long>(first.size()), static_cast<long long>(kMatrixRows),
       "cache populates on the first call");
    check(first == ply::layoutWidgets(widgets, 640, 360), "cached rects match a direct layout");
    eq(cache.recomputes(), 1, "first call computed once");
    const std::uint64_t firstHash = ply::structuralHash(first);

    // Same inputs: no recompute — reuse, not recalculation.
    check(cache.rects(widgets, 640, 360) == first, "repeat call returns the cached rect list");
    eq(cache.recomputes(), 1, "repeat call with unchanged inputs is a hit");

    // Logical size changed by one pixel: the cache must recompute. A stale
    // cache fails this — far-anchored and centre-anchored rects all move.
    const std::vector<ply::UIRect>& taller = cache.rects(widgets, 640, 361);
    check(taller == ply::layoutWidgets(widgets, 640, 361),
          "size change recomputes to match a direct layout");
    check(ply::structuralHash(taller) != firstHash,
          "one pixel of height actually changes the layout");
    eq(cache.recomputes(), 2, "size change recomputed");

    // Widget list changed: same discipline.
    std::vector<ply::UIWidgetSpec> moved = widgets;
    moved[40].offsetX += 7;
    const std::vector<ply::UIRect>& remade = cache.rects(moved, 640, 361);
    check(remade == ply::layoutWidgets(moved, 640, 361),
          "tree change recomputes to match a direct layout");
    eq(cache.recomputes(), 3, "tree change recomputed");

    // Same list again at the same size: hit.
    check(cache.rects(moved, 640, 361) == remade, "repeat after recompute is a hit");
    eq(cache.recomputes(), 3, "no recompute for repeated inputs");

    // clear() forces a recompute even with unchanged inputs.
    cache.clear();
    check(cache.rects(moved, 640, 361) == ply::layoutWidgets(moved, 640, 361),
          "cleared cache recomputes correctly");
    eq(cache.recomputes(), 4, "clear() forced a recompute");
    return 4;
}

// --- scale clamp ---------------------------------------------------------------

void testScaleClamp() {
    const ply::UIWidgetSpec one{A::Centre, kOffX, kOffY, kW, kH, 1};
    const ply::UIRect expected = ply::layoutWidgets({one}, 640, 360)[0];

    const ply::UIWidgetSpec zero{A::Centre, kOffX, kOffY, kW, kH, 0};
    const ply::UIRect r0 = ply::layoutWidgets({zero}, 640, 360)[0];
    check(r0 == expected, "scale 0 is treated as 1");

    const ply::UIWidgetSpec negative{A::Centre, kOffX, kOffY, kW, kH, -2};
    const ply::UIRect rn = ply::layoutWidgets({negative}, 640, 360)[0];
    check(rn == expected, "negative scale is treated as 1");
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every number here is also asserted above; if the
// layout changes, this line changes and the transcript stops matching.
void printEvidence(std::size_t integral, int cacheRecomputes) {
    const std::uint64_t hash = matrixLayoutHash();

    const ply::UIWidgetSpec s1{A::Centre, 0, 0, 33, 13, 1};
    const ply::UIWidgetSpec s2{A::Centre, 0, 0, 33, 13, 2};
    const ply::UIWidgetSpec s3{A::Centre, 0, 0, 33, 13, 3};
    const int snap1 = ply::layoutWidgets({s1}, 640, 360)[0].x;
    const int snap2 = ply::layoutWidgets({s2}, 640, 360)[0].x;
    const int snap3 = ply::layoutWidgets({s3}, 640, 360)[0].x;

    std::uint64_t there = 0;
    const bool crossProcess = secondProcessHash(there) && there == hash;

    const ply::UIWidgetSpec zero{A::Centre, kOffX, kOffY, kW, kH, 0};
    const ply::UIWidgetSpec negative{A::Centre, kOffX, kOffY, kW, kH, -2};
    const int clamped0 = ply::layoutWidgets({zero}, 640, 360)[0].w;
    const int clampedNeg = ply::layoutWidgets({negative}, 640, 360)[0].w;

    std::printf(
        "EVIDENCE ui_layout rows=%zu rects=%zu integral=%zu matrix_hash=0x%016llx "
        "cross_process=%s centre_snap=%d/%d/%d floor=43/-7/-3/-1 scale_clamp=%d/%d "
        "cache_recomputes=%d\n",
        kMatrixRows, kMatrixRows, integral, static_cast<unsigned long long>(hash),
        crossProcess ? "match" : "recomputed", snap1, snap2, snap3, clamped0, clampedNeg,
        cacheRecomputes);
}

}  // namespace

int main(int argc, char** argv) {
    // Second-process mode for E-a4.3: print the matrix layout hash and exit.
    // The parent compares this against its own in-process hash.
    if (argc > 1 && std::strcmp(argv[1], "--layout-hash-child") == 0) {
        std::printf("LAYOUT_HASH %016llx\n",
                    static_cast<unsigned long long>(matrixLayoutHash()));
        return 0;
    }

    testAnchorMatrix();
    testIntegerSnapping();
    const std::size_t integral = testIntegralRects();
    testHashFields();
    testTwoProcessHash();
    const int cacheRecomputes = testLayoutCache();
    testScaleClamp();

    if (failures == 0) {
        printEvidence(integral, cacheRecomputes);
        std::printf("ui_layout tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_layout tests: %d failure(s)\n", failures);
    return 1;
}
