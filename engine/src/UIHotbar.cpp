#include "plyEngine/UIHotbar.hpp"

namespace ply {

UIWidgetSpec hotbarSpec(int slotCount, int slotArtSize, int gap, UIAnchor anchor, int offsetX,
                        int offsetY, int scale) {
    UIWidgetSpec spec;
    spec.anchor = anchor;
    spec.offsetX = offsetX;
    spec.offsetY = offsetY;
    spec.scale = scale;

    const int count = slotCount < 0 ? 0 : slotCount;
    const int slot = slotArtSize < 0 ? 0 : slotArtSize;
    const int gapPx = gap < 0 ? 0 : gap;
    spec.width = count * slot + (count > 0 ? count - 1 : 0) * gapPx;
    spec.height = slot;
    return spec;
}

std::vector<UIRect> hotbarSlotRects(const UIRect& bar, int slotCount, int gap) {
    std::vector<UIRect> out;
    if (slotCount <= 0) return out;

    // floor((bar.w - (n-1)*gap) / n): both terms non-negative for a sane bar,
    // so C++ division floors. Slots span the bar's full height.
    const int slotW = (bar.w - (slotCount - 1) * gap) / slotCount;
    if (slotW <= 0) return out;

    out.reserve(static_cast<std::size_t>(slotCount));
    for (int i = 0; i < slotCount; ++i) {
        out.push_back(UIRect{bar.x + i * (slotW + gap), bar.y, slotW, bar.h});
    }
    return out;
}

int hotbarSlotAt(const std::vector<UIRect>& slots, int x, int y) {
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const UIRect& slot = slots[i];
        if (x >= slot.x && x < slot.x + slot.w && y >= slot.y && y < slot.y + slot.h) {
            return static_cast<int>(i);
        }
    }
    return -1;  // gaps, edges and outside are all misses
}

int hotbarWrapSelection(int current, int delta, int slotCount) {
    if (slotCount <= 0) return -1;

    // Floor-mod normalises an out-of-range current into [0, slotCount).
    int index = current % slotCount;
    if (index < 0) index += slotCount;
    if (current < 0) {
        // Nothing selected: a right step selects slot 0, a left step the last.
        return delta < 0 ? slotCount - 1 : 0;
    }
    index = (index + delta) % slotCount;
    if (index < 0) index += slotCount;
    return index;
}

UISlotState hotbarSlotState(int index, int selectedIndex) {
    return index == selectedIndex ? UISlotState::Selected : UISlotState::Normal;
}

}  // namespace ply
