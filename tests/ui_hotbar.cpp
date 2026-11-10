// Headless Hotbar tests (M1a a5.3): E-a5.3 — click hits the right slot after
// layout rounding, and selection wraps. Composes a4's layout (the bar is
// placed by layoutWidgets, centre-line rounding included) with a5.2's state
// mapping. Pure integer math: no GL context, no window, no clock, no float.

#include "plyEngine/UIHotbar.hpp"
#include "plyEngine/UILayout.hpp"
#include "plyEngine/UISlot.hpp"

#include <cstdio>
#include <string_view>
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

using ply::UIAnchor;
using ply::UIRect;
using ply::UISlotState;

// The reference hotbar: 5 slots of 20 px art with 2 px gaps at scale 1 —
// 5*20 + 4*2 = 108 px wide — anchored BottomCentre with offset (0, 8).
constexpr int kSlotCount = 5, kSlotArt = 20, kGapArt = 2, kScale = 1;

ply::UIWidgetSpec refSpec() {
    return ply::hotbarSpec(kSlotCount, kSlotArt, kGapArt, UIAnchor::BottomCentre, 0, 8, kScale);
}

// --- the laid-out bar and its slots -----------------------------------------

void testBarLayout() {
    const ply::UIWidgetSpec spec = refSpec();
    eq(spec.width, 108, "bar art width = 5*20 + 4*2");
    eq(spec.height, 20, "bar art height = slot art height");

    const UIRect bar = ply::layoutWidgets({spec}, 640, 360)[0];
    // BottomCentre: x = (640-108)/2 = 266 exact; y = 360-8-20 = 332.
    eqRect(bar, UIRect{266, 332, 108, 20}, "laid-out bar rect");

    const std::vector<UIRect> slots = ply::hotbarSlotRects(bar, kSlotCount, kGapArt * kScale);
    eq(static_cast<long long>(slots.size()), 5, "5 slot rects");
    if (slots.size() == 5) {
        for (std::size_t i = 0; i < slots.size(); ++i) {
            eq(slots[i].x, 266 + 22 * static_cast<int>(i), "slot i x = bar.x + i*(20+2)");
            eq(slots[i].y, 332, "slot i y = bar y");
            eq(slots[i].w, 20, "slot width at scale 1");
            eq(slots[i].h, 20, "slot spans the bar height");
        }
        eq(slots[1].x - slots[0].x, 22, "stride = slot + gap");
        eqRect(slots[0], UIRect{266, 332, 20, 20}, "first slot rect");
        eqRect(slots[4], UIRect{354, 332, 20, 20}, "last slot rect");
    }
    check(ply::hotbarSlotRects(bar, 0, 2).empty(), "0 slots emit nothing");
    check(ply::hotbarSlotRects(bar, -2, 2).empty(), "negative slots emit nothing");
    check(ply::hotbarSlotRects(UIRect{0, 0, 3, 20}, 5, 2).empty(),
          "bar too narrow for the slots emits nothing");
}

// --- E-a5.3: the click hits the right slot -----------------------------------

void testHitTest() {
    const UIRect bar = ply::layoutWidgets({refSpec()}, 640, 360)[0];
    const std::vector<UIRect> slots = ply::hotbarSlotRects(bar, kSlotCount, kGapArt * kScale);

    // Inside a slot: middle of the first, last pixel of the last.
    eq(ply::hotbarSlotAt(slots, 266 + 5, 332 + 5), 0, "click in slot 0's middle hits slot 0");
    eq(ply::hotbarSlotAt(slots, 354 + 19, 340), 4, "click on slot 4's last pixel hits slot 4");

    // The misses: gaps, the bar's edges, outside. Three pinned misses.
    eq(ply::hotbarSlotAt(slots, 286, 333), -1, "click in the first gap hits nothing");
    eq(ply::hotbarSlotAt(slots, 265, 340), -1, "click one pixel left of the bar hits nothing");
    eq(ply::hotbarSlotAt(slots, 271, 331), -1, "click one pixel above the bar hits nothing");

    // After layout rounding: a centre-anchored bar with an odd art width
    // (5*21 + 4*2 = 113) gets a floored origin — (640-113)/2 = 263.5 -> 263 —
    // and the click must be judged by the rounded rects, not the fraction.
    {
        const ply::UIWidgetSpec odd = ply::hotbarSpec(5, 21, 2, UIAnchor::Centre, 0, 0, 1);
        const UIRect oddBar = ply::layoutWidgets({odd}, 640, 360)[0];
        eq(oddBar.x, 263, "odd bar's origin is floored by layout");
        const std::vector<UIRect> oddSlots = ply::hotbarSlotRects(oddBar, 5, 2);
        eq(static_cast<long long>(oddSlots.size()), 5, "odd bar still emits 5 slots");
        if (oddSlots.size() == 5) {
            eq(oddSlots[1].x, 286, "slot 1 starts at the rounded coordinate");
            eq(ply::hotbarSlotAt(oddSlots, 290, 175), 1, "click judged by the rounded rect");
            // The rounded boundary, pixel by pixel: slot 0 ends at 283, the
            // gap is 284..285, slot 1 starts at 286. No fractional edge.
            eq(ply::hotbarSlotAt(oddSlots, 283, 175), 0, "slot 0's last pixel is slot 0");
            eq(ply::hotbarSlotAt(oddSlots, 284, 175), -1, "the gap starts the pixel after");
            eq(ply::hotbarSlotAt(oddSlots, 285, 175), -1, "the gap ends the pixel before slot 1");
            eq(ply::hotbarSlotAt(oddSlots, 286, 175), 1, "slot 1's first pixel is slot 1");
        }
    }

    // Scale 2: same art, doubled — the hit test runs on laid-out pixels.
    {
        const ply::UIWidgetSpec big = ply::hotbarSpec(3, 20, 2, UIAnchor::Centre, 0, 0, 2);
        const UIRect bigBar = ply::layoutWidgets({big}, 640, 360)[0];
        eqRect(bigBar, UIRect{256, 160, 128, 40}, "scale-2 bar rect");
        const std::vector<UIRect> bigSlots = ply::hotbarSlotRects(bigBar, 3, 2 * 2);
        eq(static_cast<long long>(bigSlots.size()), 3, "scale-2 bar emits 3 slots");
        if (bigSlots.size() == 3) {
            eq(bigSlots[0].x, 256, "scale-2 slot 0");
            eq(bigSlots[1].x, 300, "scale-2 slot 1");
            eq(bigSlots[2].x, 344, "scale-2 slot 2");
            eq(ply::hotbarSlotAt(bigSlots, 344 + 39, 180), 2, "click in scale-2 slot 2");
            eq(ply::hotbarSlotAt(bigSlots, 296, 180), -1, "click in the scale-2 gap hits nothing");
        }
    }
}

// --- E-a5.3: selection wraps ---------------------------------------------------

void testWrapSelection() {
    // Wrap at both ends, no-op step, normalisation of an out-of-range current.
    eq(ply::hotbarWrapSelection(0, -1, kSlotCount), 4, "left from slot 0 wraps to the last");
    eq(ply::hotbarWrapSelection(4, 1, kSlotCount), 0, "right from the last wraps to slot 0");
    eq(ply::hotbarWrapSelection(2, 0, kSlotCount), 2, "a zero step keeps the selection");
    eq(ply::hotbarWrapSelection(2, 5, kSlotCount), 2, "a full lap returns to the same slot");
    eq(ply::hotbarWrapSelection(7, 0, kSlotCount), 2, "out-of-range current resumes in range");
    eq(ply::hotbarWrapSelection(-1, 1, kSlotCount), 0, "nothing selected + right selects slot 0");
    eq(ply::hotbarWrapSelection(-1, -1, kSlotCount), 4, "nothing selected + left selects the last");
    eq(ply::hotbarWrapSelection(0, 1, 0), -1, "an empty hotbar has no selection");
    eq(ply::hotbarWrapSelection(0, 1, -3), -1, "a nonsensical slot count has no selection");
}

// --- composition with a5.2 -------------------------------------------------------

void testSlotState() {
    check(ply::hotbarSlotState(3, 3) == UISlotState::Selected, "the selected slot's state is Selected");
    check(ply::hotbarSlotState(3, 1) == UISlotState::Normal, "an unselected slot's state is Normal");
    check(ply::hotbarSlotState(0, -1) == UISlotState::Normal,
          "with nothing selected every slot is Normal");

    // Through the a5.2 mapping: the selected slot's frame region is slot.selected.
    const std::string_view selected = ply::slotFrameRegionName(ply::hotbarSlotState(2, 2));
    if (selected != "slot.selected") {
        std::fprintf(stderr, "  FAIL: selected slot maps to slot.selected (got \"%.*s\")\n",
                     static_cast<int>(selected.size()), selected.data());
        ++failures;
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence() {
    const UIRect bar = ply::layoutWidgets({refSpec()}, 640, 360)[0];
    const std::vector<UIRect> slots = ply::hotbarSlotRects(bar, kSlotCount, kGapArt * kScale);
    if (slots.size() < 2) return;  // unreachable when the suite passes

    const ply::UIWidgetSpec odd = ply::hotbarSpec(5, 21, 2, UIAnchor::Centre, 0, 0, 1);
    const UIRect oddBar = ply::layoutWidgets({odd}, 640, 360)[0];
    const std::vector<UIRect> oddSlots = ply::hotbarSlotRects(oddBar, 5, 2);
    const int roundedHit = ply::hotbarSlotAt(oddSlots, 290, 175);

    const ply::UIWidgetSpec big = ply::hotbarSpec(3, 20, 2, UIAnchor::Centre, 0, 0, 2);
    const std::vector<UIRect> bigSlots =
        ply::hotbarSlotRects(ply::layoutWidgets({big}, 640, 360)[0], 3, 2 * 2);
    if (bigSlots.size() < 3) return;  // unreachable when the suite passes

    std::printf(
        "EVIDENCE ui_hotbar slots=%d bar=%dx%d first=%d,%d stride=%d hits=%d/%d misses=3 "
        "rounded_hit=%d scale2=%d/%d/%d wrap=%d/%d/%d/%d\n",
        kSlotCount, bar.w, bar.h, slots[0].x, slots[0].y, slots[1].x - slots[0].x,
        ply::hotbarSlotAt(slots, 271, 337), ply::hotbarSlotAt(slots, 373, 340), roundedHit,
        bigSlots[0].x, bigSlots[1].x, bigSlots[2].x, ply::hotbarWrapSelection(0, -1, kSlotCount),
        ply::hotbarWrapSelection(4, 1, kSlotCount), ply::hotbarWrapSelection(-1, 1, kSlotCount),
        ply::hotbarWrapSelection(7, 0, kSlotCount));
}

}  // namespace

int main() {
    testBarLayout();
    testHitTest();
    testWrapSelection();
    testSlotState();

    if (failures == 0) {
        printEvidence();
        std::printf("ui_hotbar tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_hotbar tests: %d failure(s)\n", failures);
    return 1;
}
