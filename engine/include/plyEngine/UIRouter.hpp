#pragma once

#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <vector>

namespace ply {

/// Input routing (M1a a10, UI_FRAMEWORK.md §6).
///
/// The router is the single place that decides whether a pointer event belongs
/// to the UI or to the world. It is pure data — rects in, decisions out — and it
/// is the *decision core* that both the real input path and the phone dev
/// client's virtual cursor are meant to feed. Neither source is wired yet: as of
/// a10 nothing in the tree constructs a UIRouter outside its tests, so what is
/// proven here is the routing rule, not a working device path. There is also no
/// key event in this API (see PointerEventType), so the keyboard half of
/// UI_FRAMEWORK.md §6 is not merely unwired — it is not representable yet, and
/// the "global handler runs while a panel has focus" rule is asserted only for
/// pointer events.

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
