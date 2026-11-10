#pragma once

#include "plyEngine/UILayout.hpp"
#include "plyEngine/UISlot.hpp"

#include <vector>

namespace ply {

/// Hotbar geometry (M1a a5.3): a row of slots with a selection index.
/// Pure data — a composition of a4's placement and a5.2's state mapping, with
/// no GL, no window, no clock, no float.

/// The widget spec whose (width, height) is a hotbar row's total art size:
/// `slotCount` square slots of `slotArtSize` px separated by `gap` px, i.e.
/// width = slotCount*slotArtSize + (slotCount-1)*gap, height = slotArtSize.
/// The row is placed exactly like any widget (anchor + offset + integer
/// scale), so the bar's position comes from a4's layout — including its
/// centre-line rounding. `slotCount` below 0 is treated as 0 and `gap` below
/// 0 as 0 (named behaviour, tested).
UIWidgetSpec hotbarSpec(int slotCount, int slotArtSize, int gap, UIAnchor anchor, int offsetX,
                        int offsetY, int scale);

/// Per-slot rects inside a laid-out bar rect, left to right. `gap` is in
/// laid-out (screen) px — the art gap times the scale when the bar came from
/// `hotbarSpec` + `layoutWidgets`. Slot width is
/// floor((bar.w - (slotCount-1)*gap) / slotCount); slot i starts at
/// bar.x + i*(slotWidth + gap). Slots span the bar's full height. A bar that
/// fits no slot (slotCount <= 0 or non-positive slot width) yields nothing.
std::vector<UIRect> hotbarSlotRects(const UIRect& bar, int slotCount, int gap);

/// Index of the slot whose laid-out rect contains (x, y), or -1 when the
/// point is on a gap, the bar's edge or outside the bar entirely. The test is
/// against the laid-out rects as given — the coordinates a4 rounded — so a
/// centre-anchored bar's floored origin is what clicks are judged by.
int hotbarSlotAt(const std::vector<UIRect>& slots, int x, int y);

/// Selection index after a step of `delta` (-1 left, +1 right, 0 none) with
/// wrap-around: slotCount-1 wraps to 0, 0 wraps back to slotCount-1. The
/// current index is normalised by floor-mod into [0, slotCount) first, so an
/// out-of-range value keeps behaving (7 of 5 slots resumes at 2). A current
/// index of -1 (nothing selected) selects slot 0 on a right step and the last
/// slot on a left step. A hotbar with no slots has no selection: -1.
int hotbarWrapSelection(int current, int delta, int slotCount);

/// The visual state of the slot at `index` given the bar's selection:
/// Selected on the selected slot, Normal elsewhere (a Disabled slot is
/// per-slot data, not the bar's concern).
UISlotState hotbarSlotState(int index, int selectedIndex);

}  // namespace ply
