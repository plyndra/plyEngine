// Headless minimap tests (M1a a8): E-a8.1 — world (x, y) -> minimap pixel
// correct at the four corners and the centre — plus the marker transform at
// integer minimap scale and the render-target downscale (UI_FRAMEWORK.md §5,
// "render-target downscale + marker transforms").
//
// The world is y-up (OrthoCamera.hpp); minimap px are y-down screen pixels
// like the rest of the UI (UI_FRAMEWORK.md §1), so every mapping here mirrors
// the y axis. Everything is integer arithmetic (i64 intermediates): no GL, no
// window, no clock, and no float anywhere in the graded path. A caller
// holding float world positions floors them first (a7's snapCameraAxis) and
// the float stops at that snap.

#include "plyEngine/Minimap.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <cstdio>
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

void eqPoint(const ply::MinimapPoint& got, int wantX, int wantY, const char* what) {
    if (got.x != wantX || got.y != wantY) {
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d}, want {%d,%d})\n", what, got.x, got.y,
                     wantX, wantY);
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

void eqHash(std::uint64_t got, std::uint64_t want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got 0x%016llx, want 0x%016llx)\n", what,
                     static_cast<unsigned long long>(got),
                     static_cast<unsigned long long>(want));
        ++failures;
    }
}

using ply::MinimapMarker;
using ply::MinimapPoint;
using ply::UIRect;

// The reference fixture: the a7 internal-resolution render target
// (640x360 world px, 1 unit = 1 px, y-up) downscaled onto a 160x90 minimap —
// exactly 4 world px per map px, but nothing in the transform assumes that.
constexpr ply::MinimapSpec kA{640, 360, 160, 90};
// A fit that is NOT an integer downscale (640/100 = 6.4): the transform is an
// exact ratio, so the corners still land on the corners.
constexpr ply::MinimapSpec kB{100, 60, 24, 16};

// --- E-a8.1: world -> minimap px at the four corners and the centre ----------

void testCornersAndCentre() {
    // The world grid [0, 639] x [0, 359] (y-up) maps onto the minimap grid
    // [0, 159] x [0, 89] (y-down): the four corner px land on the four corner
    // px, mirrored on y. Pinned as literals, not re-derived here.
    eqPoint(ply::worldToMinimap(kA, 0, 0, 1), 0, 89, "bottom-left corner -> minimap bottom-left");
    eqPoint(ply::worldToMinimap(kA, 639, 0, 1), 159, 89,
            "bottom-right corner -> minimap bottom-right");
    eqPoint(ply::worldToMinimap(kA, 0, 359, 1), 0, 0, "top-left corner -> minimap top-left");
    eqPoint(ply::worldToMinimap(kA, 639, 359, 1), 159, 0, "top-right corner -> minimap top-right");
    eqPoint(ply::worldToMinimap(kA, 320, 180, 1), 80, 44, "world centre -> minimap centre px");

    // The world grid's exclusive far edge (worldWidth, worldHeight) is one px
    // outside the map — the transform is total and monotone, clamping is the
    // draw policy. This is also what makes an edge off-by-one a pin failure.
    eqPoint(ply::worldToMinimap(kA, 640, 360, 1), 160, -1,
            "the exclusive far edge maps one px outside the map");

    // At integer minimap scale 2 the map grid is 320x180 and the same
    // invariant holds: corners on corners.
    eqPoint(ply::worldToMinimap(kA, 0, 0, 2), 0, 179, "scale 2 bottom-left");
    eqPoint(ply::worldToMinimap(kA, 639, 0, 2), 319, 179, "scale 2 bottom-right");
    eqPoint(ply::worldToMinimap(kA, 0, 359, 2), 0, 0, "scale 2 top-left");
    eqPoint(ply::worldToMinimap(kA, 639, 359, 2), 319, 0, "scale 2 top-right");
    eqPoint(ply::worldToMinimap(kA, 320, 180, 2), 160, 89, "scale 2 centre");
    eqPoint(ply::worldToMinimap(kA, 640, 360, 2), 320, -1, "scale 2 far edge");
    eqPoint(ply::worldToMinimap(kA, 320, 180, 3), 240, 134, "scale 3 centre");

    // A non-integer fit (kB: 24/100, 16/60) still pins corners to corners —
    // the mapping is an exact integer ratio, not a rounded scale factor.
    eqPoint(ply::worldToMinimap(kB, 0, 0, 1), 0, 15, "kB bottom-left");
    eqPoint(ply::worldToMinimap(kB, 99, 0, 1), 23, 15, "kB bottom-right");
    eqPoint(ply::worldToMinimap(kB, 0, 59, 1), 0, 0, "kB top-left");
    eqPoint(ply::worldToMinimap(kB, 99, 59, 1), 23, 0, "kB top-right");
    eqPoint(ply::worldToMinimap(kB, 50, 30, 1), 12, 7, "kB centre");

    // The scale clamp (a4's named behaviour): 0 or negative behaves as 1.
    eqPoint(ply::worldToMinimap(kA, 639, 359, 0), 159, 0, "scale 0 clamps to 1");
    eqPoint(ply::worldToMinimap(kA, 639, 359, -5), 159, 0, "negative scale clamps to 1");

    // A non-positive extent has no grid to land on: that axis maps to 0,
    // per axis, and the other axis is unaffected (named behaviour).
    eqPoint(ply::worldToMinimap(ply::MinimapSpec{0, 360, 160, 90}, 7, 7, 1), 0, 88,
            "non-positive worldWidth maps x to 0");
    eqPoint(ply::worldToMinimap(ply::MinimapSpec{640, 0, 160, 90}, 7, 7, 1), 1, 0,
            "non-positive worldHeight maps y to 0");
    eqPoint(ply::worldToMinimap(ply::MinimapSpec{640, 360, 0, 90}, 7, 7, 1), 0, 88,
            "non-positive mapWidth maps x to 0");
    eqPoint(ply::worldToMinimap(ply::MinimapSpec{640, 360, 160, 0}, 7, 7, 1), 1, 0,
            "non-positive mapHeight maps y to 0");
}

// --- the marker transform at integer minimap scale ---------------------------

void testMarkerTransform() {
    // The same marker (world 320,180; art 3x3 at scale 1) at integer scales
    // 1, 2 and 3. Position scales with the map grid; the art scales by the
    // same integer; the art is centred on the mapped px by a floor halving.
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 1), UIRect{79, 43, 3, 3},
           "marker at scale 1");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 2), UIRect{157, 86, 6, 6},
           "marker at scale 2");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 3), UIRect{236, 130, 9, 9},
           "marker at scale 3");

    // Scale before rounding, position: floor(3*160*2/640) = 1 while the
    // round-then-scale order (floor(3*160/640)*2) would give 0 and move the
    // rect off this pin. The scale multiplies before the division rounds.
    eqPoint(ply::worldToMinimap(kA, 3, 3, 2), 1, 178,
            "scale multiplies before the position rounds");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{3, 3, 1, 1}, 2), UIRect{0, 177, 2, 2},
           "the marker rect carries the same position rounding");

    // Scale before rounding, art (a4's rule 2): the centring offset is
    // floorDiv2(3*2) = 3, while floorDiv2(3)*2 would be 2. The two orders
    // genuinely differ here, so the scale-2 pin above cannot be satisfied by
    // both.
    check(ply::floorDiv2(6) != 2 * ply::floorDiv2(3),
          "rounding orders differ at art 3, scale 2 (offset 3 vs 2)");

    // Marker rounding floors toward negative infinity, not toward zero: a
    // marker left of / below the world origin maps left of / below the map.
    // Truncation would pin it to the map edge instead (-1 -> 0, 179 -> 178).
    eqPoint(ply::worldToMinimap(kA, -3, -1, 1), -1, 90, "floor, not trunc, at scale 1");
    eqPoint(ply::worldToMinimap(kA, -3, -1, 2), -2, 180, "floor, not trunc, at scale 2");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{-3, -1, 1, 1}, 1), UIRect{-1, 90, 1, 1},
           "marker below-left of the world at scale 1");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{-3, -1, 1, 1}, 2), UIRect{-3, 179, 2, 2},
           "marker below-left of the world at scale 2");

    // Corner markers land on the corner px with 1x1 art — the marker path and
    // the point path agree by construction.
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{0, 0, 1, 1}, 1), UIRect{0, 89, 1, 1},
           "bottom-left corner marker");
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{639, 359, 1, 1}, 1), UIRect{159, 0, 1, 1},
           "top-right corner marker");

    // Marker art below 1 clamps to 1 (named behaviour, like the scale clamp):
    // a marker always occupies at least one map px.
    eqRect(ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 0, -2}, 1), UIRect{80, 44, 1, 1},
           "non-positive marker art clamps to 1x1");
}

// --- the render-target downscale --------------------------------------------

void testRenderTargetDownscale() {
    // The a7 render target's world rect downscales exactly onto the minimap:
    // this is the "render-target downscale" of UI_FRAMEWORK.md §5.
    eqRect(ply::minimapRect(kA, UIRect{0, 0, 640, 360}, 1), UIRect{0, 0, 160, 90},
           "the whole render target fills the minimap at scale 1");
    eqRect(ply::minimapRect(kA, UIRect{0, 0, 640, 360}, 2), UIRect{0, 0, 320, 180},
           "the whole render target fills the minimap at scale 2");

    // A camera view rect (120x90 world px at 40,24) as it lands on the
    // minimap: the covering rect of every world px it holds.
    eqRect(ply::minimapRect(kA, UIRect{40, 24, 120, 90}, 1), UIRect{10, 61, 30, 23},
           "view rect on the minimap");

    // One mapping, not two: a 1x1 world rect is exactly the px the point
    // transform returns, for every pinned corner/centre at scales 1 and 2.
    const int xs[5] = {0, 639, 0, 639, 320};
    const int ys[5] = {0, 0, 359, 359, 180};
    for (int scale = 1; scale <= 2; ++scale) {
        for (int i = 0; i < 5; ++i) {
            const UIRect r = ply::minimapRect(kA, UIRect{xs[i], ys[i], 1, 1}, scale);
            const MinimapPoint p = ply::worldToMinimap(kA, xs[i], ys[i], scale);
            eqRect(r, UIRect{p.x, p.y, 1, 1}, "a 1x1 world rect is exactly the point's px");
        }
    }

    // A zero extent has no px to cover: extent 0 on that axis, at the
    // transformed origin; the other axis still maps (named behaviour).
    eqRect(ply::minimapRect(kA, UIRect{20, 30, 0, 5}, 1), UIRect{5, 81, 0, 2},
           "zero-width rect keeps its y span");
    eqRect(ply::minimapRect(kA, UIRect{20, 30, 5, 0}, 1), UIRect{5, 82, 2, 0},
           "zero-height rect keeps its x span");
}

// --- structural hash over the marker/rect list -------------------------------

const std::vector<MinimapMarker>& fixtureMarkers() {
    static const std::vector<MinimapMarker> markers = {
        MinimapMarker{320, 180, 3, 3},
        MinimapMarker{0, 0, 1, 1},
        MinimapMarker{639, 359, 2, 4},
    };
    return markers;
}

std::vector<UIRect> placeMarkers(int scale) {
    std::vector<UIRect> placed;
    for (const MinimapMarker& marker : fixtureMarkers()) {
        placed.push_back(ply::minimapMarkerRect(kA, marker, scale));
    }
    return placed;
}

void testStructuralHash() {
    // Pinned by literal: the marker list hashes (FNV-1a 64 over count + each
    // marker's i32 x, y, artW, artH) and the placed rect list hashes through
    // a4's rect hash. Both are reproducible on recompute.
    const std::uint64_t markerHash = ply::structuralHash(fixtureMarkers());
    eqHash(markerHash, 0x8cef02ace23713feull, "marker list hash is pinned");
    eqHash(ply::structuralHash(fixtureMarkers()), markerHash, "marker hash reproduces");
    eqHash(ply::structuralHash(placeMarkers(1)), 0x76ce779a9f2dcd36ull, "placed rect hash is pinned");
    eqHash(ply::structuralHash(placeMarkers(1)), ply::structuralHash(placeMarkers(1)),
           "placed rect hash reproduces across fresh placements");

    // Per-field perturbation probes: a hash that silently dropped any one of
    // x, y, artW or artH would pass every equality above. Each probe moves
    // exactly one field and requires the hash to move.
    const MinimapMarker base{320, 180, 3, 3};
    const std::uint64_t baseHash = ply::structuralHash({base});
    check(ply::structuralHash({MinimapMarker{321, 180, 3, 3}}) != baseHash,
          "hash moves when x moves");
    check(ply::structuralHash({MinimapMarker{320, 181, 3, 3}}) != baseHash,
          "hash moves when y moves");
    check(ply::structuralHash({MinimapMarker{320, 180, 4, 3}}) != baseHash,
          "hash moves when artW moves");
    check(ply::structuralHash({MinimapMarker{320, 180, 3, 4}}) != baseHash,
          "hash moves when artH moves");

    // The hash covers list order: the same markers in another order is a
    // different submission.
    check(ply::structuralHash({MinimapMarker{0, 0, 1, 1}, base}) !=
              ply::structuralHash({base, MinimapMarker{0, 0, 1, 1}}),
          "hash moves when the marker order moves");
}

// One line of measured values, printed so the ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence() {
    const MinimapPoint c1 = ply::worldToMinimap(kA, 0, 0, 1);
    const MinimapPoint c2 = ply::worldToMinimap(kA, 639, 0, 1);
    const MinimapPoint c3 = ply::worldToMinimap(kA, 0, 359, 1);
    const MinimapPoint c4 = ply::worldToMinimap(kA, 639, 359, 1);
    const MinimapPoint ce = ply::worldToMinimap(kA, 320, 180, 1);
    const MinimapPoint ox = ply::worldToMinimap(kA, 640, 360, 1);
    const MinimapPoint d1 = ply::worldToMinimap(kA, 0, 0, 2);
    const MinimapPoint d2 = ply::worldToMinimap(kA, 639, 0, 2);
    const MinimapPoint d3 = ply::worldToMinimap(kA, 0, 359, 2);
    const MinimapPoint d4 = ply::worldToMinimap(kA, 639, 359, 2);
    const MinimapPoint de = ply::worldToMinimap(kA, 320, 180, 2);
    const MinimapPoint f1 = ply::worldToMinimap(kB, 99, 0, 1);
    const MinimapPoint f2 = ply::worldToMinimap(kB, 0, 59, 1);
    const MinimapPoint fe = ply::worldToMinimap(kB, 50, 30, 1);
    const UIRect down = ply::minimapRect(kA, UIRect{0, 0, 640, 360}, 1);
    const UIRect view = ply::minimapRect(kA, UIRect{40, 24, 120, 90}, 1);
    const UIRect m1 = ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 1);
    const UIRect m2 = ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 2);
    const UIRect m3 = ply::minimapMarkerRect(kA, MinimapMarker{320, 180, 3, 3}, 3);
    const MinimapPoint n1 = ply::worldToMinimap(kA, -3, -1, 1);
    const MinimapPoint n2 = ply::worldToMinimap(kA, -3, -1, 2);
    const MinimapPoint p3 = ply::worldToMinimap(kA, 3, 3, 2);

    std::printf(
        "EVIDENCE minimap corners=%d,%d/%d,%d/%d,%d/%d,%d centre=%d,%d outside=%d,%d "
        "corners2=%d,%d/%d,%d/%d,%d/%d,%d centre2=%d,%d fit=%d,%d/%d,%d/%d,%d "
        "downscale=%d,%d,%d,%d view=%d,%d,%d,%d "
        "markers=%d,%d,%dx%d/%d,%d,%dx%d/%d,%d,%dx%d neg=%d,%d/%d,%d pos3=%d,%d "
        "marker_hash=0x%016llx rects_hash=0x%016llx\n",
        c1.x, c1.y, c2.x, c2.y, c3.x, c3.y, c4.x, c4.y, ce.x, ce.y, ox.x, ox.y, d1.x, d1.y, d2.x,
        d2.y, d3.x, d3.y, d4.x, d4.y, de.x, de.y, f1.x, f1.y, f2.x, f2.y, fe.x, fe.y, down.x,
        down.y, down.w, down.h, view.x, view.y, view.w, view.h, m1.x, m1.y, m1.w, m1.h, m2.x,
        m2.y, m2.w, m2.h, m3.x, m3.y, m3.w, m3.h, n1.x, n1.y, n2.x, n2.y, p3.x, p3.y,
        static_cast<unsigned long long>(ply::structuralHash(fixtureMarkers())),
        static_cast<unsigned long long>(ply::structuralHash(placeMarkers(1))));
}

}  // namespace

int main() {
    testCornersAndCentre();
    testMarkerTransform();
    testRenderTargetDownscale();
    testStructuralHash();

    if (failures == 0) {
        if (fixtureMarkers().size() == 3 && placeMarkers(1).size() == 3) {
            printEvidence();
        } else {
            std::fprintf(stderr, "  FAIL: evidence preconditions\n");
            return 1;
        }
        std::printf("minimap tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "minimap tests: %d failure(s)\n", failures);
    return 1;
}
