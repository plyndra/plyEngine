#include "plyEngine/InputRouting.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <cstdint>

namespace ply {

const char* toString(PointerSource source) {
    switch (source) {
        case PointerSource::HostMouse: return "HostMouse";
        case PointerSource::VirtualCursor: return "VirtualCursor";
    }
    return "";  // unreachable; every enumerator is handled
}

int logicalAxis(int windowPx, int factor) {
    const int k = factor < 1 ? 1 : factor;
    // Floor, not truncation, so negatives keep going outwards past the origin.
    std::int64_t q = static_cast<std::int64_t>(windowPx) / k;
    if (windowPx % k != 0 && windowPx < 0) --q;
    return static_cast<int>(q);
}

CursorPoint logicalCursor(int windowX, int windowY, int presentFactor) {
    return CursorPoint{logicalAxis(windowX, presentFactor), logicalAxis(windowY, presentFactor)};
}

InputFrame inputFrameFrom(const Input& input, PointerSource source, int presentFactor,
                          const std::vector<Key>& trackedKeys) {
    InputFrame frame;
    frame.source = source;
    const glm::vec2 position = input.mousePosition();
    // The truncation of the device's fractional position happens here, once, at
    // the boundary; everything downstream is integer.
    const CursorPoint cursor = logicalCursor(static_cast<int>(position.x),
                                            static_cast<int>(position.y), presentFactor);
    frame.cursorX = cursor.x;
    frame.cursorY = cursor.y;
    frame.primaryDown = input.isMouseDown(MouseButton::Left);
    for (const Key& key : trackedKeys) {
        if (input.isKeyPressed(key)) frame.keys.push_back(KeyEdge{key, true});
    }
    return frame;
}

FrameRoute pumpFrame(UIRouter& router, PumpState& state, const InputFrame& frame) {
    FrameRoute route;

    // A move is emitted only when the cursor actually travelled, except on the
    // very first frame, where the frame exists to establish a position. A
    // still cursor emits nothing, so an idle HUD produces an empty transcript
    // instead of a move per frame.
    const bool first = !state.hasPrevious;
    const bool moved = first || frame.cursorX != state.lastX || frame.cursorY != state.lastY;
    if (moved) {
        route.pointer.push_back(
            router.route(PointerEvent{PointerEventType::Move, frame.cursorX, frame.cursorY}));
    }

    // Edges come from comparing with the previous frame, so a held button is
    // one press, not one per frame. On the first frame a button that is already
    // down is NOT a press: its down edge happened before the frames we observe,
    // and inventing one would start a capture nobody asked for.
    if (frame.primaryDown && !first && !state.lastDown) {
        route.pointer.push_back(
            router.route(PointerEvent{PointerEventType::Press, frame.cursorX, frame.cursorY}));
    }
    if (!frame.primaryDown && !first && state.lastDown) {
        route.pointer.push_back(
            router.route(PointerEvent{PointerEventType::Release, frame.cursorX, frame.cursorY}));
    }

    // Keys after the pointer events, so a key pressed in the same frame as a
    // widget press sees the capture that press established.
    for (const KeyEdge& key : frame.keys) {
        route.keys.push_back(router.routeKey(KeyEvent{key.key, key.pressed}));
    }

    state.hasPrevious = true;
    state.lastX = frame.cursorX;
    state.lastY = frame.cursorY;
    state.lastDown = frame.primaryDown;
    return route;
}

std::uint64_t structuralHash(const FrameRoute& route) {
    StructuralHasher hasher;
    hasher.u64(route.pointer.size());
    for (const RouteResult& result : route.pointer) {
        hasher.i32(result.widget);
        hasher.i32(result.worldConsumed ? 1 : 0);
        hasher.i32(result.globalConsumed ? 1 : 0);
        hasher.i32(result.capturedWidget);
    }
    hasher.u64(route.keys.size());
    for (const KeyRouteResult& result : route.keys) {
        hasher.i32(result.widget);
        hasher.i32(result.worldConsumed ? 1 : 0);
        hasher.i32(result.globalConsumed ? 1 : 0);
    }
    return hasher.value();
}

}  // namespace ply
