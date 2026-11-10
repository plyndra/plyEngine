// Headless Panel + Tooltip tests (M1a a5.4): E-a5.4 — a tooltip never leaves
// the logical viewport for any cursor position — plus the panel title bar.
// The viewport sweep is exhaustive where the viewport is small enough to be,
// and a strided sweep otherwise, so "for any cursor position" is a loop over a
// declared set, not a universal claimed without one. No GL context, no window,
// no clock, no float.

#include "plyEngine/UIPanel.hpp"
#include "plyEngine/UILayout.hpp"
#include "plyEngine/NineSlice.hpp"
#include "plyEngine/StructuralHash.hpp"
#include "plyEngine/UIAtlas.hpp"

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

void eqRect(const ply::UIRect& got, const ply::UIRect& want, const char* what) {
    if (!(got == want)) {
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d,%d,%d}, want {%d,%d,%d,%d})\n", what, got.x,
                     got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        ++failures;
    }
}

using ply::UIRect;

// --- panel title bar -------------------------------------------------------------

void testPanelTitle() {
    const UIRect panel{40, 60, 200, 120};

    // A 16 px title sits flush with the panel's top edge and spans its width.
    eqRect(ply::panelTitleRect(panel, 16), UIRect{40, 60, 200, 16}, "title bar rect");

    // No title: zero height emits nothing (zero-size, so it contributes no
    // geometry to a renderer that skips empty quads).
    {
        const UIRect none = ply::panelTitleRect(panel, 0);
        eq(none.w, 0, "no title bar has zero width");
        eq(none.h, 0, "no title bar has zero height");
    }
    // A title taller than the panel is clamped, never overflowing it.
    {
        const UIRect tall = ply::panelTitleRect(panel, 500);
        eq(tall.h, 120, "an over-tall title bar clamps to the panel height");
        eq(tall.y, 60, "clamped title bar still starts at the panel top");
        check(tall.y + tall.h <= panel.y + panel.h, "title bar never overflows the panel");
    }
    // A panel with no height has no title bar.
    eq(ply::panelTitleRect(UIRect{0, 0, 200, 0}, 16).h, 0, "zero-height panel has no title bar");
    // A negative title height is treated as no title.
    eq(ply::panelTitleRect(panel, -8).h, 0, "negative title height emits nothing");
}

// --- composition: a panel from a4 layout + a2 nine-slice ---------------------------

void testPanelComposition() {
    // A 100x60 art-px panel, centre-anchored at scale 2 -> {220,120,200,120}.
    const ply::UIWidgetSpec spec{ply::UIAnchor::Centre, 0, 0, 100, 60, 2};
    const UIRect panel = ply::layoutWidgets({spec}, 640, 360)[0];
    eqRect(panel, UIRect{220, 120, 200, 120}, "laid-out panel rect");

    // The nine-slice background comes from a2 at the laid-out (scaled) size.
    ply::UIAtlasRegion region;
    region.name = "panel.wood";
    region.x = 0;
    region.y = 0;
    region.w = 48;
    region.h = 48;
    region.hasMargins = true;
    region.margins = ply::UIMargins{12, 12, 12, 12};
    region.edges = ply::StretchMode::Tile;
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, panel.w, panel.h);
    check(!quads.empty(), "panel background emits nine-slice quads");
    check(quads[1].topRight.x == panel.w, "background quads are panel-sized");
    check(ply::structuralHash(quads) != 0, "panel background hashes");

    // Title bar of 12 art px at scale 2 = 24 screen px, inside the panel.
    const UIRect title = ply::panelTitleRect(panel, 12 * 2);
    eqRect(title, UIRect{220, 120, 200, 24}, "title bar in the laid-out panel");
}

// --- E-a5.4: the tooltip never leaves the viewport -------------------------------

/// Sweeps every cursor position for a viewport and checks the invariant.
/// Returns the number of positions checked so the transcript can state it.
long long sweepAllCursors(int logicalWidth, int logicalHeight, int tipW, int tipH, int offsetX,
                          int offsetY) {
    long long checked = 0;
    int failuresBefore = failures;
    for (int cy = 0; cy <= logicalHeight; ++cy) {
        for (int cx = 0; cx <= logicalWidth; ++cx) {
            const UIRect tip =
                ply::tooltipRect(cx, cy, tipW, tipH, offsetX, offsetY, logicalWidth, logicalHeight);
            ++checked;
            if (!ply::rectInsideViewport(tip, logicalWidth, logicalHeight)) {
                if (failures == failuresBefore) {
                    std::fprintf(stderr,
                                 "  FAIL: tooltip left the viewport at cursor (%d,%d): "
                                 "{%d,%d,%d,%d} in %dx%d\n",
                                 cx, cy, tip.x, tip.y, tip.w, tip.h, logicalWidth, logicalHeight);
                }
                ++failures;
            }
        }
    }
    return checked;
}

long long testTooltipSweeps() {
    long long total = 0;
    // 640x360, the internal logical viewport: a strided sweep at 4 px keeps the
    // run fast while still crossing all four edges and both flip regions.
    {
        int failuresBefore = failures;
        long long checked = 0;
        for (int cy = 0; cy <= 360; cy += 4) {
            for (int cx = 0; cx <= 640; cx += 4) {
                const UIRect tip = ply::tooltipRect(cx, cy, 120, 48, 12, 16, 640, 360);
                ++checked;
                if (!ply::rectInsideViewport(tip, 640, 360)) {
                    if (failures == failuresBefore) {
                        std::fprintf(stderr, "  FAIL: 640x360 tooltip left the viewport at (%d,%d)\n",
                                     cx, cy);
                    }
                    ++failures;
                }
            }
        }
        eq(checked, 91 * 161, "640x360 strided sweep covers 91x161 cursor positions");
        total += checked;
    }
    // Small viewports: exhaustive, so the "any cursor position" claim is a real
    // loop over the whole set rather than a sampled one. Every tooltip here
    // fits the viewport — the scope the invariant is defined for.
    total += sweepAllCursors(40, 24, 18, 10, 2, 3);
    total += sweepAllCursors(40, 24, 30, 18, 2, 3);   // neither side fits: clamps
    total += sweepAllCursors(33, 21, 0, 0, 2, 3);     // zero-size tooltip
    total += sweepAllCursors(33, 21, 33, 21, 0, 0);   // tooltip exactly the viewport, no offset
    // The total is pinned too, so shortening any sweep cannot quietly shrink
    // the claim while the printed transcript still reads plausibly.
    eq(total, 91 * 161 + 41 * 25 + 41 * 25 + 34 * 22 + 34 * 22,
       "swept position total = strided 640x360 + four exhaustive small sweeps");
    return total;
}

// --- E-a5.4: the flip and clamp rules themselves ----------------------------------

void testTooltipPlacement() {
    // Fits below-right: no flip.
    eqRect(ply::tooltipRect(100, 100, 60, 20, 12, 16, 640, 360), UIRect{112, 116, 60, 20},
           "tooltip sits at cursor + offset when it fits");

    // Would pass the right edge: flip to the left of the cursor.
    // cursor 620 + 12 + 60 = 692 > 640 -> 620 - 12 - 60 = 548.
    eqRect(ply::tooltipRect(620, 100, 60, 20, 12, 16, 640, 360), UIRect{548, 116, 60, 20},
           "tooltip flips to the left when it would pass the right edge");

    // Would pass the bottom edge: flip above.
    eqRect(ply::tooltipRect(100, 350, 60, 20, 12, 16, 640, 360), UIRect{112, 314, 60, 20},
           "tooltip flips above when it would pass the bottom edge");

    // Both edges: flips on both axes independently.
    eqRect(ply::tooltipRect(620, 350, 60, 20, 12, 16, 640, 360), UIRect{548, 314, 60, 20},
           "tooltip flips on both axes at the bottom-right corner");

    // Neither side fits, but the tooltip still fits the viewport: clamped in.
    {
        const UIRect tip = ply::tooltipRect(30, 12, 30, 18, 2, 3, 40, 24);
        check(ply::rectInsideViewport(tip, 40, 24), "a tooltip that fits is clamped inside");
        eq(tip.x, 10, "clamped to viewport width - tooltip width on x");
        eq(tip.y, 6, "clamped to viewport height - tooltip height on y");
    }
    // Larger than the viewport on both axes: pinned to the near edge, which is
    // the documented (and only possible) behaviour. It is not "inside" — a
    // tooltip bigger than the screen cannot be — and the art lint owns that.
    {
        const UIRect tip = ply::tooltipRect(30, 12, 60, 40, 2, 3, 40, 24);
        eq(tip.x, 0, "an over-wide tooltip pins to the near edge on x");
        eq(tip.y, 0, "an over-tall tooltip pins to the near edge on y");
        check(!ply::rectInsideViewport(tip, 40, 24),
              "an oversized tooltip is honestly reported as not inside, not silently claimed");
    }

    // The exact fit on the far side: cursor where cursor+offset+size is exactly
    // the viewport edge must NOT flip (the edge is reachable, not past it).
    // 640 - 12 - 60 = 568; at cursor 568 the tooltip ends exactly at 640.
    eqRect(ply::tooltipRect(568, 100, 60, 20, 12, 16, 640, 360), UIRect{580, 116, 60, 20},
           "a tooltip ending exactly at the far edge does not flip");
    check(580 + 60 == 640, "and it really is flush with the edge");

    // One pixel further, and only then does it flip.
    eqRect(ply::tooltipRect(569, 100, 60, 20, 12, 16, 640, 360), UIRect{497, 116, 60, 20},
           "one pixel past the edge flips");
}

void testRectInsideViewport() {
    check(ply::rectInsideViewport(UIRect{0, 0, 640, 360}, 640, 360),
          "a viewport-sized rect is inside");
    check(ply::rectInsideViewport(UIRect{0, 0, 0, 0}, 640, 360), "a zero-size rect at the origin is inside");
    check(!ply::rectInsideViewport(UIRect{642, 0, 0, 0}, 640, 360),
          "a zero-size rect past the edge is outside");
    check(ply::rectInsideViewport(UIRect{639, 359, 1, 1}, 640, 360), "the last pixel is inside");
    check(!ply::rectInsideViewport(UIRect{640, 359, 1, 1}, 640, 360), "x past the edge is outside");
    check(!ply::rectInsideViewport(UIRect{639, 360, 1, 1}, 640, 360), "y past the edge is outside");
    check(!ply::rectInsideViewport(UIRect{-1, 0, 10, 10}, 640, 360), "a negative origin is outside");
    check(!ply::rectInsideViewport(UIRect{0, 0, 641, 360}, 640, 360), "a too-wide rect is outside");
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence(long long sweepCount) {
    const UIRect panel{40, 60, 200, 120};
    const UIRect title = ply::panelTitleRect(panel, 16);

    const UIRect flipR = ply::tooltipRect(620, 100, 60, 20, 12, 16, 640, 360);
    const UIRect flipB = ply::tooltipRect(100, 350, 60, 20, 12, 16, 640, 360);
    const UIRect both = ply::tooltipRect(620, 350, 60, 20, 12, 16, 640, 360);
    const UIRect clamped = ply::tooltipRect(30, 12, 60, 40, 2, 3, 40, 24);
    const UIRect exact = ply::tooltipRect(568, 100, 60, 20, 12, 16, 640, 360);

    std::printf(
        "EVIDENCE ui_panel title=%d,%d,%d,%d tooltip_sweep=%lld flip_right=%d flip_bottom=%d "
        "both=%d,%d oversized_clamp=%d,%d exact_fit=%d\n",
        title.x, title.y, title.w, title.h, sweepCount, flipR.x, flipB.y, both.x, both.y,
        clamped.x, clamped.y, exact.x);
}

}  // namespace

int main() {
    testPanelTitle();
    testPanelComposition();
    testTooltipPlacement();
    testRectInsideViewport();
    const long long sweepCount = testTooltipSweeps();

    if (failures == 0) {
        printEvidence(sweepCount);
        std::printf("ui_panel tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_panel tests: %d failure(s)\n", failures);
    return 1;
}
