#pragma once

#include "plyEngine/Input.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <vector>

namespace ply {

/// Input routing (M1a a10, UI_FRAMEWORK.md §6).
///
/// The router is the single place that decides whether a pointer event belongs
/// to the UI or to the world. It is pure data — rects in, decisions out — and it
/// is the *decision core* that both the real input path and the phone dev
/// client's virtual cursor feed. a10 built this decision core and recorded that
/// nothing outside its tests fed it; a11 (InputRouting.hpp) is the wiring, and it
/// uses this class through `pumpFrame` — the router itself is unchanged by the
/// wiring. The keyboard half of §6 is `KeyEvent`/`routeKey` below: a10 could not
/// represent keys at all, so the "global handler runs while a panel has focus"
/// rule was pointer-only until a11 added them.

/// What a pointer can do. A button has one press and one release per gesture;
/// `Move` is the cursor travelling without a button changing state.
enum class PointerEventType { Move, Press, Release };
/// One pointer event at a point in logical (or output, see UIRouter) pixels.
struct PointerEvent {
    PointerEventType type = PointerEventType::Move;
    int x = 0, y = 0;

    bool operator==(const PointerEvent&) const = default;
};

/// Where one event went. Both queues can be empty: an event consumed by a
/// widget never also reaches the world, and a release kept by a capture is
/// consumed even when the cursor is nowhere near the widget.
struct RouteResult {
    /// Index of the interactive widget that took the event, or -1.
    int widget = -1;
    /// True when the world queue receives the event.
    bool worldConsumed = false;
    /// True when the global handler (pause, toggle debug) sees it. Constant
    /// true today: every routed event reaches the global queue. It is a field
    /// rather than an assumption so a future "the UI swallowed this one"
    /// suppression has somewhere to live, and the hash covers it.
    bool globalConsumed = true;
    /// Capture state after this event.
    int capturedWidget = -1;

    bool uiConsumed() const { return widget >= 0; }

    bool operator==(const RouteResult&) const = default;
};

/// One key edge, routed by the same ownership rule as the pointer.
struct KeyEvent {
    Key key = Key::Unknown;
    /// True for a down edge, false for an up edge.
    bool pressed = false;

    bool operator==(const KeyEvent&) const = default;
};

/// Where one key went: the captured widget, or the world.
///
/// The rule is deliberately narrow, because the router has no focus concept:
/// while a widget holds a pointer capture it owns the keys pressed during that
/// gesture (a slider keeps the arrow keys it grabbed), and with no capture the
/// key goes to the world. The global handler sees every key regardless, so a
/// pause or debug toggle cannot be swallowed by a widget.
struct KeyRouteResult {
    /// Index of the widget that took the key, or -1.
    int widget = -1;
    /// True when the world queue receives the key.
    bool worldConsumed = false;
    /// True when the global handler sees it. Constant true, like RouteResult's.
    bool globalConsumed = true;

    bool uiConsumed() const { return widget >= 0; }

    bool operator==(const KeyRouteResult&) const = default;
};

/// The router. Interactive rects are supplied in the same order the caller
/// renders them, because the index a route returns is the slot the caller needs
/// to act on; nothing else about them is assumed.
///
/// Rules (UI_FRAMEWORK.md §6):
///  - UI consumes before world: an event over an interactive widget never
///    reaches the world queue.
///  - Press on a widget captures it; the capture keeps every later event until
///    the matching release, even if the cursor has left the widget. That is what
///    stops a drag off a button from becoming a world click.
///  - Release always ends the capture, whether or not it is still over the
///    widget, so a gesture cannot leave the router stuck.
///  - With no capture, a press or move over no widget goes to the world.
class UIRouter {
public:
    /// Replaces the interactive rect list. Clears any capture: the rect indices
    /// a capture referred to no longer mean the same thing.
    void setInteractive(const std::vector<UIRect>& rects);

    /// Routes one event and advances the capture state.
    RouteResult route(const PointerEvent& event);

    /// Routes one key edge. Does not change the capture: keys are owned by an
    /// existing gesture, they do not start or end one.
    KeyRouteResult routeKey(const KeyEvent& event);

    /// The widget currently holding the pointer, or -1.
    int capturedWidget() const { return captured_; }

    /// Drops the capture without routing a release (a lost focus, a device
    /// disconnect). The next event routes as if no press had happened.
    void cancelCapture() { captured_ = -1; }

private:
    /// Index of the topmost interactive rect containing the point, or -1.
    /// Topmost wins because later rects draw over earlier ones.
    int widgetAt(int x, int y) const;

    std::vector<UIRect> rects_;
    int captured_ = -1;
};

/// A scripted gesture sequence: the deterministic input source a10 needs, so
/// routing can be asserted without a window, a device or a clock.
struct InputScript {
    std::vector<PointerEvent> events;

    bool operator==(const InputScript&) const = default;
};

/// Replays `script` through `router` and returns one result per event. The
/// router is advanced as a real session would be, so capture state carries
/// between events — that is the whole point of replaying a script rather than
/// asserting each event in isolation.
std::vector<RouteResult> replay(UIRouter& router, const InputScript& script);

/// FNV-1a 64 over a routing transcript: count, then per result the widget
/// index, the world and global flags and the capture index. Integers only, so
/// two replays of the same script can be compared across processes.
std::uint64_t structuralHash(const std::vector<RouteResult>& transcript);

/// FNV-1a 64 over the script itself, so a fixture's input can be pinned the
/// same way its output is.
std::uint64_t structuralHash(const InputScript& script);

}  // namespace ply
