#include "plyEngine/UIPanel.hpp"

namespace ply {

namespace {

/// One axis of the tooltip placement: fits below/right of the cursor when it
/// can, otherwise flips to the far side, otherwise clamps into the viewport.
int placeAxis(int cursor, int offset, int size, int limit) {
    const int near = cursor + offset;
    // The far side of the cursor: the tooltip sits before it, so its trailing
    // edge is `offset` before the cursor.
    const int far = cursor - offset - size;

    if (near >= 0 && near + size <= limit) return near;
    if (far >= 0 && far + size <= limit) return far;

    // Neither side fits (tooltip too large for this axis): the closest legal
    // position, clamped so the tooltip stays inside as far as it can.
    const int maxPos = limit - size;
    if (maxPos < 0) return 0;  // larger than the viewport: pin to the near edge
    if (near < 0) return 0;
    if (near > maxPos) return maxPos;
    return near;
}

}  // namespace

UIRect panelTitleRect(const UIRect& panel, int titleHeight) {
    if (titleHeight <= 0 || panel.w <= 0 || panel.h <= 0) return UIRect{panel.x, panel.y, 0, 0};
    const int height = titleHeight > panel.h ? panel.h : titleHeight;
    return UIRect{panel.x, panel.y, panel.w, height};
}

UIRect tooltipRect(int cursorX, int cursorY, int width, int height, int offsetX, int offsetY,
                   int logicalWidth, int logicalHeight) {
    const int x = placeAxis(cursorX, offsetX, width, logicalWidth);
    const int y = placeAxis(cursorY, offsetY, height, logicalHeight);
    return UIRect{x, y, width, height};
}

bool rectInsideViewport(const UIRect& rect, int logicalWidth, int logicalHeight) {
    if (rect.w < 0 || rect.h < 0) return false;
    if (rect.x < 0 || rect.y < 0) return false;
    return rect.x + rect.w <= logicalWidth && rect.y + rect.h <= logicalHeight;
}

}  // namespace ply
