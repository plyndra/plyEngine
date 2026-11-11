#include "plyEngine/UIRouter.hpp"

#include "plyEngine/StructuralHash.hpp"

namespace ply {

void UIRouter::setInteractive(const std::vector<UIRect>& rects) {
    rects_ = rects;
    // Indices into the old list mean nothing in the new one, so a capture
    // cannot survive the swap.
    captured_ = -1;
}

int UIRouter::widgetAt(int x, int y) const {
    // Topmost wins: later rects draw over earlier ones, so scan backwards and
    // take the first hit. Edges are half-open, which is what makes adjacent
    // widgets share a boundary without either claiming it twice.
    for (std::size_t i = rects_.size(); i-- > 0;) {
        const UIRect& rect = rects_[i];
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

KeyRouteResult UIRouter::routeKey(const KeyEvent& event) {
    // Same ownership rule as the pointer, and deliberately no more: the router
    // has no focus concept, so a key belongs to the gesture that is already
    // running (if any) or to the world. The key itself changes no state — a key
    // up edge is not a pointer release.
    KeyRouteResult result;
    if (captured_ >= 0) {
        result.widget = captured_;
        return result;
    }
    result.worldConsumed = true;
    return result;
}

RouteResult UIRouter::route(const PointerEvent& event) {
    RouteResult result;

    if (captured_ >= 0) {
        // A capture owns everything until its release, wherever the pointer is.
        // That is what stops a drag off a HUD button becoming a world click.
        result.widget = captured_;
        if (event.type == PointerEventType::Release) captured_ = -1;
        result.capturedWidget = captured_;
        return result;
    }

    switch (event.type) {
        case PointerEventType::Move: {
            const int hit = widgetAt(event.x, event.y);
            result.widget = hit;
            result.worldConsumed = hit < 0;  // aim/interact only outside the UI
            break;
        }
        case PointerEventType::Press: {
            const int hit = widgetAt(event.x, event.y);
            result.widget = hit;
            if (hit >= 0) {
                captured_ = hit;  // capture on press, before any release
            } else {
                result.worldConsumed = true;
            }
            break;
        }
        case PointerEventType::Release: {
            // With no capture there is no widget gesture to end, so the world
            // gets it — a release without a press is the world's event.
            result.widget = -1;
            result.worldConsumed = true;
            break;
        }
    }

    result.capturedWidget = captured_;
    return result;
}

std::vector<RouteResult> replay(UIRouter& router, const InputScript& script) {
    std::vector<RouteResult> transcript;
    transcript.reserve(script.events.size());
    for (const PointerEvent& event : script.events) {
        transcript.push_back(router.route(event));
    }
    return transcript;
}

std::uint64_t structuralHash(const std::vector<RouteResult>& transcript) {
    StructuralHasher hasher;
    hasher.u64(transcript.size());
    for (const RouteResult& result : transcript) {
        hasher.i32(result.widget);
        hasher.byte(result.worldConsumed ? 1 : 0);
        hasher.byte(result.globalConsumed ? 1 : 0);
        hasher.i32(result.capturedWidget);
    }
    return hasher.value();
}

std::uint64_t structuralHash(const InputScript& script) {
    StructuralHasher hasher;
    hasher.u64(script.events.size());
    for (const PointerEvent& event : script.events) {
        hasher.byte(static_cast<std::uint8_t>(event.type));
        hasher.i32(event.x);
        hasher.i32(event.y);
    }
    return hasher.value();
}

}  // namespace ply
