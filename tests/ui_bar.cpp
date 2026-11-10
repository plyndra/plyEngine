// Headless Bar tests (M1a a5.1): E-a5.1 — fill fraction → pixel width, clamps
// at 0/1 — plus the segment-tick geometry and the a4 pipeline handoff (a
// laid-out frame rect feeds the bar). Pure integer math: value is per mille,
// so the fill width and the structural hash never touch floating point. No GL
// context, no window, no clock.

#include "plyEngine/UIBar.hpp"
#include "plyEngine/UILayout.hpp"
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
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d,%d,%d}, want {%d,%d,%d,%d})\n", what,
                     got.x, got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        ++failures;
    }
}

using ply::UIBarCaps;
using ply::UIRect;

// The reference frame: {10, 20, 100, 20} with caps {8, 4, 8, 4}.
// Inner track: x 18..102 (84 px wide), y 24..36 (12 px tall).
constexpr UIRect kFrame{10, 20, 100, 20};
constexpr UIBarCaps kCaps{8, 4, 8, 4};
constexpr int kInnerW = 84;
constexpr int kInnerH = 12;
constexpr int kInnerX = 18;
constexpr int kInnerY = 24;

// --- E-a5.1: fill fraction -> pixel width -----------------------------------

void testFillWidth() {
    // Hand-pinned: width = floor(84 * v / 1000). The 1 and 999 rows are the
    // interesting ones — a value of 1/1000 fills less than a pixel (width 0),
    // 999/1000 fills 83.916 px (width 83, not 84).
    struct FillCase {
        int value;
        int width;
    };
    const FillCase cases[] = {
        {0, 0}, {1, 0}, {500, 42}, {999, 83}, {1000, 84},
    };
    for (const FillCase& c : cases) {
        const UIRect fill = ply::barFillRect(kFrame, kCaps, c.value);
        eq(fill.w, c.width, "fill width = floor(innerW * value / 1000)");
        eq(fill.x, kInnerX, "fill starts after the left cap");
        eq(fill.y, kInnerY, "fill starts below the top cap");
        eq(fill.h, kInnerH, "fill spans the inner height");
    }
    // One full rect pinned exactly, so the fields cannot trade places.
    eqRect(ply::barFillRect(kFrame, kCaps, 500), UIRect{18, 24, 42, 12}, "fill rect at 500");

    // Different track, same rule: 640-wide frame, caps 36 (a 12 px margin at
    // scale 3), innerW 568; a quarter fill is 142 exactly.
    {
        const UIRect wide{0, 0, 640, 96};
        const UIBarCaps caps3{36, 36, 36, 36};
        const UIRect fill = ply::barFillRect(wide, caps3, 250);
        eq(fill.w, 142, "quarter fill of a 568 px track");
        eq(fill.x, 36, "wide frame fill starts after its caps");
    }
}

// --- E-a5.1: clamps at 0/1 ----------------------------------------------------

void testClamps() {
    // Below 0 and above 1000 clamp. 1500 is the case that catches a missing
    // clamp: 84 * 1500 / 1000 = 126, which would overflow the 84 px track.
    {
        const UIRect under = ply::barFillRect(kFrame, kCaps, -7);
        eqRect(under, ply::barFillRect(kFrame, kCaps, 0), "value below 0 clamps to 0");
        eq(under.w, 0, "clamped-low fill is empty");
    }
    {
        const UIRect over = ply::barFillRect(kFrame, kCaps, 1500);
        eqRect(over, ply::barFillRect(kFrame, kCaps, 1000), "value above 1 clamps to 1");
        eq(over.w, kInnerW, "clamped-high fill is the full track");
        check(over.w + over.x <= kFrame.x + kFrame.w - kCaps.right,
              "fill never exceeds the inner track");
    }
}

// --- segment ticks ------------------------------------------------------------

void testTicks() {
    // 4 segments over an 84 px track: boundaries at 21, 42, 63.
    const std::vector<UIRect> ticks = ply::barTickRects(kFrame, kCaps, 4);
    eq(static_cast<long long>(ticks.size()), 3, "4 segments produce 3 interior ticks");
    for (std::size_t i = 0; i < ticks.size(); ++i) {
        eq(ticks[i].x, kInnerX + 21 * static_cast<int>(i + 1), "tick boundary = floor(innerW*k/n)");
        eq(ticks[i].w, 1, "tick is 1 px wide");
        eq(ticks[i].y, kInnerY, "tick spans from the top cap");
        eq(ticks[i].h, kInnerH, "tick spans to the bottom cap");
        check(ticks[i].x < kInnerX + kInnerW, "last tick stays strictly inside the track");
    }

    check(ply::barTickRects(kFrame, kCaps, 1).empty(), "1 segment has no interior boundary");
    check(ply::barTickRects(kFrame, kCaps, 0).empty(), "0 segments emit nothing");
    check(ply::barTickRects(kFrame, kCaps, -3).empty(), "negative segments emit nothing");

    // Ticks are a function of the frame only — moving the fill must not move
    // them (segment boundaries stay put as the value changes).
    {
        const std::vector<UIRect> at500 = ply::barTickRects(kFrame, kCaps, 4);
        const UIRect fillHi = ply::barFillRect(kFrame, kCaps, 900);
        (void)fillHi;
        eq(static_cast<long long>(at500.size()), 3, "ticks unchanged at a different value");
        if (at500.size() == 3) eq(at500[2].x, 81, "third boundary still at 63 px into the track");
    }
}

// --- pipeline: a4 layout feeds the bar ----------------------------------------

void testLayoutPipeline() {
    // A 100x20 art-px frame, centre-anchored at scale 2: layout gives
    // {220, 160, 200, 40}; the atlas region's 4 px margins become 8 px caps.
    const ply::UIWidgetSpec spec{ply::UIAnchor::Centre, 0, 0, 100, 20, 2};
    const UIRect frame = ply::layoutWidgets({spec}, 640, 360)[0];
    eqRect(frame, UIRect{220, 160, 200, 40}, "laid-out frame rect");

    ply::UIAtlasRegion region;
    region.name = "bar.frame";
    region.w = 64;
    region.h = 16;
    region.hasMargins = true;
    region.margins = ply::UIMargins{4, 4, 4, 4};
    const UIBarCaps caps{region.margins.left * 2, region.margins.top * 2,
                         region.margins.right * 2, region.margins.bottom * 2};

    const UIRect fill = ply::barFillRect(frame, caps, 300);
    eq(fill.w, 55, "30% of the 184 px inner track = 55 px");
    eqRect(fill, UIRect{228, 168, 55, 24}, "fill rect in the laid-out frame");
}

// --- hash ----------------------------------------------------------------------

void testHash() {
    std::vector<UIRect> state;
    state.push_back(ply::barFillRect(kFrame, kCaps, 500));
    for (const UIRect& tick : ply::barTickRects(kFrame, kCaps, 4)) state.push_back(tick);
    const std::uint64_t base = ply::structuralHash(state);

    // Reproduced from scratch.
    {
        std::vector<UIRect> again;
        again.push_back(ply::barFillRect(kFrame, kCaps, 500));
        for (const UIRect& tick : ply::barTickRects(kFrame, kCaps, 4)) again.push_back(tick);
        eq(ply::structuralHash(again) == base ? 0 : 1, 0, "bar state hash is reproducible");
    }
    // A different value moves the fill rect -> the hash moves.
    {
        std::vector<UIRect> changed;
        changed.push_back(ply::barFillRect(kFrame, kCaps, 750));
        for (const UIRect& tick : ply::barTickRects(kFrame, kCaps, 4)) changed.push_back(tick);
        check(ply::structuralHash(changed) != base, "hash covers the fill fraction");
    }
    // A different tick count moves the ticks -> the hash moves.
    {
        std::vector<UIRect> changed;
        changed.push_back(ply::barFillRect(kFrame, kCaps, 500));
        for (const UIRect& tick : ply::barTickRects(kFrame, kCaps, 5)) changed.push_back(tick);
        check(ply::structuralHash(changed) != base, "hash covers the tick layout");
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every number here is also asserted above; if the
// geometry changes, this line changes and the transcript stops matching.
void printEvidence() {
    const int w0 = ply::barFillRect(kFrame, kCaps, 0).w;
    const int w1 = ply::barFillRect(kFrame, kCaps, 1).w;
    const int w500 = ply::barFillRect(kFrame, kCaps, 500).w;
    const int w999 = ply::barFillRect(kFrame, kCaps, 999).w;
    const int w1000 = ply::barFillRect(kFrame, kCaps, 1000).w;
    const int clamped = ply::barFillRect(kFrame, kCaps, 1500).w;
    const std::vector<UIRect> ticks = ply::barTickRects(kFrame, kCaps, 4);

    const ply::UIWidgetSpec spec{ply::UIAnchor::Centre, 0, 0, 100, 20, 2};
    const UIRect frame = ply::layoutWidgets({spec}, 640, 360)[0];
    const UIBarCaps caps{8, 8, 8, 8};
    const int pipelineW = ply::barFillRect(frame, caps, 300).w;

    std::vector<UIRect> state;
    state.push_back(ply::barFillRect(kFrame, kCaps, 500));
    for (const UIRect& tick : ticks) state.push_back(tick);

    std::printf(
        "EVIDENCE ui_bar fill=%d/%d/%d/%d/%d clamp1500=%d ticks4=%d/%d/%d inner=%dx%d pipeline300=%d "
        "hash=0x%016llx\n",
        w0, w1, w500, w999, w1000, clamped, ticks[0].x, ticks[1].x, ticks[2].x, kInnerW, kInnerH,
        pipelineW, static_cast<unsigned long long>(ply::structuralHash(state)));
}

}  // namespace

int main() {
    testFillWidth();
    testClamps();
    testTicks();
    testLayoutPipeline();
    testHash();

    if (failures == 0) {
        printEvidence();
        std::printf("ui_bar tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_bar tests: %d failure(s)\n", failures);
    return 1;
}
