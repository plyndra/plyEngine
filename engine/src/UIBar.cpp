#include "plyEngine/UIBar.hpp"

namespace ply {

namespace {

/// The inner track a frame's caps leave open. Degenerate when the caps cover
/// the whole frame (or the frame is non-positive); callers treat width <= 0 as
/// "nothing to fill".
int innerWidth(const UIRect& frame, const UIBarCaps& caps) {
    return frame.w - caps.left - caps.right;
}

int innerHeight(const UIRect& frame, const UIBarCaps& caps) {
    return frame.h - caps.top - caps.bottom;
}

}  // namespace

UIRect barFillRect(const UIRect& frame, const UIBarCaps& caps, int valuePermille) {
    // Clamp to 0..1000 first (named behaviour, tested): value 1500 must fill
    // the track, not overflow it.
    const int value = valuePermille < 0 ? 0 : (valuePermille > 1000 ? 1000 : valuePermille);

    const int trackW = innerWidth(frame, caps);
    const int trackH = innerHeight(frame, caps);
    if (trackW <= 0 || trackH <= 0) return UIRect{frame.x + caps.left, frame.y + caps.top, 0, 0};

    // floor(trackW * value / 1000): both factors non-negative, so C++ division
    // already floors. Integer-only — the structural hash never sees a float.
    const int fillW = trackW * value / 1000;
    return UIRect{frame.x + caps.left, frame.y + caps.top, fillW, trackH};
}

std::vector<UIRect> barTickRects(const UIRect& frame, const UIBarCaps& caps, int tickCount) {
    std::vector<UIRect> out;
    const int trackW = innerWidth(frame, caps);
    const int trackH = innerHeight(frame, caps);
    if (tickCount <= 1 || trackW <= 0 || trackH <= 0) return out;

    out.reserve(static_cast<std::size_t>(tickCount - 1));
    for (int k = 1; k < tickCount; ++k) {
        const int boundary = frame.x + caps.left + trackW * k / tickCount;
        out.push_back(UIRect{boundary, frame.y + caps.top, 1, trackH});
    }
    return out;
}

}  // namespace ply
