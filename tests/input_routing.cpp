// Headless router-wiring tests (M1a a11): E-a11.1 — a host-mouse frame and a
// virtual-cursor frame with identical content produce identical routing
// transcripts (one path, not two), edges are detected across frames rather than
// re-emitted, and the keyboard half of UI_FRAMEWORK.md §6 is routed by the same
// ownership rule as the pointer.
//
// No GL, no window, no device and no clock: a frame is plain data, so the whole
// pipeline grades in ctest. The one step this cannot cover is the host shim that
// fills an InputFrame from ply::Input's polled state — that needs SDL — and the
// header says so rather than pretending otherwise.

#include "plyEngine/InputRouting.hpp"
#include "plyEngine/UIRouter.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
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

void eqStr(const std::string& got, const std::string& want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got '%s', want '%s')\n", what, got.c_str(),
                     want.c_str());
        ++failures;
    }
}

void eqHash(std::uint64_t got, std::uint64_t want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got 0x%016llx, want 0x%016llx)\n", what,
                     static_cast<unsigned long long>(got),
                     static_cast<unsigned long long>(want));
        ++failures;
    }
}

using ply::InputFrame;
using ply::Key;
using ply::KeyEdge;
using ply::PointerSource;
using ply::PumpState;
using ply::UIRect;
using ply::UIRouter;

// The HUD fixture: two interactive rects, the second drawn over the first.
// Widget 0 is the hotbar (bottom centre), widget 1 a minimap button that
// overlaps it on the right.
std::vector<UIRect> hudRects() {
    return {UIRect{266, 332, 108, 20}, UIRect{340, 328, 24, 28}};
}

UIRouter makeRouter() {
    UIRouter router;
    router.setInteractive(hudRects());
    return router;
}

// --- the frame pump: identity across sources -------------------------------

// The same gesture, described twice. Only the provenance tag differs.
std::vector<InputFrame> gesture(PointerSource source) {
    std::vector<InputFrame> frames;
    InputFrame f;
    f.source = source;
    f.cursorX = 300;
    f.cursorY = 340;
    frames.push_back(f);  // arrive over the hotbar
    f.primaryDown = true;
    f.keys.clear();
    f.keys.push_back(KeyEdge{Key::Space, true});
    frames.push_back(f);  // press, plus a key that the capture should own
    f.cursorX = 120;
    f.cursorY = 100;
    frames.push_back(f);  // drag far away, still captured
    f.primaryDown = false;
    f.keys.clear();
    f.keys.push_back(KeyEdge{Key::Space, false});
    frames.push_back(f);  // release away from the widget
    return frames;
}

void testOnePathAcrossSources() {
    UIRouter mouseRouter = makeRouter();
    UIRouter phoneRouter = makeRouter();
    PumpState mouseState;
    PumpState phoneState;

    std::vector<ply::FrameRoute> mouseRoutes;
    std::vector<ply::FrameRoute> phoneRoutes;
    for (const InputFrame& frame : gesture(PointerSource::HostMouse)) {
        mouseRoutes.push_back(ply::pumpFrame(mouseRouter, mouseState, frame));
    }
    for (const InputFrame& frame : gesture(PointerSource::VirtualCursor)) {
        phoneRoutes.push_back(ply::pumpFrame(phoneRouter, phoneState, frame));
    }

    check(mouseRoutes.size() == 4 && phoneRoutes.size() == 4, "both sources produced 4 frames");
    eq(mouseState == phoneState ? 1 : 0, 1, "both sources leave identical pump state");
    eq(mouseRouter.capturedWidget(), -1, "both gestures end with no capture");
    eq(phoneRouter.capturedWidget(), -1, "both gestures end with no capture (2)");

    // The decisive assertion: the source tag changed, nothing else did.
    bool identical = mouseRoutes.size() == phoneRoutes.size();
    for (std::size_t i = 0; identical && i < mouseRoutes.size(); ++i) {
        identical = mouseRoutes[i] == phoneRoutes[i];
    }
    check(identical, "a host-mouse frame and a virtual-cursor frame route identically");

    // And the hashes agree frame by frame, which is what makes the claim
    // checkable across processes too.
    for (std::size_t i = 0; i < mouseRoutes.size() && i < phoneRoutes.size(); ++i) {
        eqHash(ply::structuralHash(phoneRoutes[i]), ply::structuralHash(mouseRoutes[i]),
               "the transcript hash ignores the source tag");
    }

    // Provenance is still recorded, so a transcript can say where it came from.
    eqStr(ply::toString(PointerSource::HostMouse), "HostMouse", "source token");
    eqStr(ply::toString(PointerSource::VirtualCursor), "VirtualCursor", "source token 2");

    // The outcome of the gesture itself, pinned: the press captured widget 0,
    // the drag away kept it, the release ended it, and no world click escaped.
    if (mouseRoutes.size() == 4) {
        check(mouseRoutes[1].pointer.size() == 1, "the press frame routed one pointer event");
        if (mouseRoutes[1].pointer.size() == 1) {
            eq(mouseRoutes[1].pointer[0].widget, 0, "the press captured widget 0");
            eq(mouseRoutes[1].pointer[0].worldConsumed, 0, "the press did not reach the world");
            eq(mouseRoutes[1].keys.size(), 1, "the key in the press frame was routed");
            if (mouseRoutes[1].keys.size() == 1) {
                eq(mouseRoutes[1].keys[0].widget, 0, "the captured widget owns the key");
                eq(mouseRoutes[1].keys[0].worldConsumed, 0, "the key did not reach the world");
                eq(mouseRoutes[1].keys[0].globalConsumed, 1, "the global handler saw the key");
            }
        }
        eq(mouseRoutes[2].pointer.size(), 1, "the drag frame routed one move");
        if (!mouseRoutes[2].pointer.empty()) {
            eq(mouseRoutes[2].pointer[0].widget, 0, "the drag stayed with the capture");
            eq(mouseRoutes[2].pointer[0].worldConsumed, 0, "the drag did not reach the world");
        }
        eq(mouseRoutes[3].pointer.size(), 1, "the release frame routed one release");
        if (!mouseRoutes[3].pointer.empty()) {
            eq(mouseRoutes[3].pointer[0].widget, 0, "the release stayed with the capture");
            eq(mouseRoutes[3].pointer[0].capturedWidget, -1, "the release cleared the capture");
        }
    }
}

// --- edge detection: a held button is not a press every frame --------------

void testEdgeDetection() {
    UIRouter router = makeRouter();
    PumpState state;

    InputFrame f;
    f.cursorX = 300;
    f.cursorY = 340;

    // Frame 1: the cursor arrives. One move, no button activity.
    ply::FrameRoute r = ply::pumpFrame(router, state, f);
    eq(r.pointer.size(), 1, "the first frame establishes the position with one move");
    if (!r.pointer.empty()) {
        // §6 lists hover in the widget queue, and a10's router implements that:
        // a move over an interactive rect belongs to the widget and only a move
        // outside reaches the world.
        eq(r.pointer[0].widget, 0, "a hover over a widget belongs to the widget");
        eq(r.pointer[0].worldConsumed, 0, "a hover over a widget does not reach the world");
    }

    // A first frame whose button is ALREADY down is not a press: the down edge
    // happened before we started observing, so the pump emits only the move and
    // starts no capture.
    {
        UIRouter fresh = makeRouter();
        PumpState freshState;
        InputFrame held;
        held.cursorX = 300;
        held.cursorY = 340;
        held.primaryDown = true;
        const ply::FrameRoute r0 = ply::pumpFrame(fresh, freshState, held);
        eq(r0.pointer.size(), 1, "a first frame with the button held emits only the move");
        eq(fresh.capturedWidget(), -1, "a button already down on the first frame captures nothing");
    }

    // Frame 2: press.
    f.primaryDown = true;
    r = ply::pumpFrame(router, state, f);
    eq(r.pointer.size(), 1, "the press frame routes exactly one event");
    eq(router.capturedWidget(), 0, "the press captured the widget");

    // Frames 3-5: the button stays down and the cursor stays still. Nothing is
    // emitted: a held button is not a press every frame, and a still cursor is
    // not a move every frame.
    long long silent = 0;
    for (int i = 0; i < 3; ++i) {
        r = ply::pumpFrame(router, state, f);
        if (r.pointer.empty() && r.keys.empty()) ++silent;
    }
    eq(silent, 3, "three idle frames routed nothing at all");

    // Frame 6: move while held -> a single move, still captured.
    f.cursorX = 350;
    f.cursorY = 340;
    r = ply::pumpFrame(router, state, f);
    eq(r.pointer.size(), 1, "a move while held routes one move");
    if (!r.pointer.empty()) eq(r.pointer[0].widget, 0, "the move stayed captured");

    // Frame 7: release.
    f.primaryDown = false;
    r = ply::pumpFrame(router, state, f);
    eq(r.pointer.size(), 1, "the release routes one event");
    eq(router.capturedWidget(), -1, "the capture is clear");

    // Frame 8: a second press over no widget reaches the world.
    f.cursorX = 10;
    f.cursorY = 10;
    f.primaryDown = true;
    r = ply::pumpFrame(router, state, f);
    // One move (the cursor travelled) then the press.
    eq(r.pointer.size(), 2, "a move and a press in one frame emit both, move first");
    if (r.pointer.size() == 2) {
        eq(r.pointer[0].worldConsumed, 1, "the move reached the world");
        eq(r.pointer[0].widget, -1, "the move hit no widget");
        eq(r.pointer[1].widget, -1, "the press hit no widget");
        eq(r.pointer[1].worldConsumed, 1, "the press reached the world");
    }
}

// --- keyboard half of §6 ---------------------------------------------------

void testKeyboard() {
    UIRouter router = makeRouter();

    // No capture: the key belongs to the world, and the global handler sees it.
    const ply::KeyRouteResult none = router.routeKey(ply::KeyEvent{Key::Escape, true});
    eq(none.widget, -1, "a key with no capture hits no widget");
    eq(none.worldConsumed, 1, "a key with no capture reaches the world");
    eq(none.globalConsumed, 1, "the global handler always sees a key");
    eq(none.uiConsumed(), 0, "uiConsumed() is false with no widget");

    // Capture widget 0, then a key: the widget owns it, the world does not.
    ply::RouteResult press;
    press = router.route(ply::PointerEvent{ply::PointerEventType::Press, 300, 340});
    eq(press.widget, 0, "the press captured widget 0 for the key test");
    const ply::KeyRouteResult owned = router.routeKey(ply::KeyEvent{Key::Left, true});
    eq(owned.widget, 0, "a key during a gesture belongs to the captured widget");
    eq(owned.worldConsumed, 0, "the owned key did not reach the world");
    eq(owned.globalConsumed, 1, "the global handler still sees the owned key");

    // Keys do not change the capture: a key up edge is not a pointer release.
    router.routeKey(ply::KeyEvent{Key::Left, false});
    eq(router.capturedWidget(), 0, "a key edge does not disturb the pointer capture");

    // Release, then the same key goes to the world again.
    router.route(ply::PointerEvent{ply::PointerEventType::Release, 300, 340});
    const ply::KeyRouteResult after = router.routeKey(ply::KeyEvent{Key::Left, true});
    eq(after.widget, -1, "after release the key belongs to the world again");
    eq(after.worldConsumed, 1, "after release the world consumes the key");

    // A key edge routed through the pump reaches the same places, and the
    // pump's key list preserves the caller's order.
    UIRouter pumped = makeRouter();
    PumpState state;
    InputFrame f;
    f.cursorX = 300;
    f.cursorY = 340;
    f.keys.push_back(KeyEdge{Key::P, true});
    f.keys.push_back(KeyEdge{Key::P, false});
    const ply::FrameRoute route = ply::pumpFrame(pumped, state, f);
    eq(route.keys.size(), 2, "both key edges were routed");
    if (route.keys.size() == 2) {
        eq(route.keys[0].worldConsumed, 1, "the down edge reached the world");
        eq(route.keys[1].worldConsumed, 1, "the up edge reached the world");
    }
}

// --- the host conversion (the one graded part of the shim) -----------------

void testLogicalConversion() {
    // Window px -> logical px at the integer present factor. 1920x1080 presents
    // 640x360 at 3x (a7), so a window cursor at 900 lands on logical 300.
    eq(ply::logicalAxis(900, 3), 300, "a window coordinate divides by the present factor");
    eq(ply::logicalAxis(0, 3), 0, "the origin maps to the origin");
    eq(ply::logicalAxis(1919, 3), 639, "the last window px lands on the last logical px");
    eq(ply::logicalAxis(1920, 3), 640, "one past the edge maps one past the logical edge");
    eq(ply::logicalAxis(5, 1), 5, "factor 1 is the identity");
    eq(ply::logicalAxis(5, 0), 5, "factor 0 clamps to 1");
    eq(ply::logicalAxis(5, -3), 5, "a negative factor clamps to 1");

    // Floor, not truncation: a cursor past the top-left edge keeps moving out
    // instead of sticking at 0 (-1 px at factor 3 is -1, not 0).
    eq(ply::logicalAxis(-1, 3), -1, "a cursor one px past the left edge is logical -1");
    eq(ply::logicalAxis(-3, 3), -1, "exactly one logical step past the edge");
    eq(ply::logicalAxis(-4, 3), -2, "two logical steps past the edge");
    eq(ply::logicalAxis(-1, 1), -1, "negative is unchanged at factor 1");

    const ply::CursorPoint c = ply::logicalCursor(900, 540, 3);
    eq(c.x, 300, "cursor x converts");
    eq(c.y, 180, "cursor y converts");
    eq(ply::logicalCursor(-1, -1, 3).x, -1, "both axes floor the same way");
}

// --- the transcript hash ---------------------------------------------------

std::uint64_t gestureHash() {
    UIRouter router = makeRouter();
    PumpState state;
    std::uint64_t hash = 0;
    for (int frame = 0; frame < 4; ++frame) {
        InputFrame f;
        f.cursorX = 300 + frame * 10;
        f.cursorY = 340;
        f.primaryDown = frame >= 1 && frame <= 2;
        f.keys.push_back(KeyEdge{Key::Space, frame == 1});
        const ply::FrameRoute route = ply::pumpFrame(router, state, f);
        const std::uint64_t h = ply::structuralHash(route);
        hash = (hash ^ h) * 0x100000001b3ull;  // chain the per-frame hashes
    }
    return hash;
}

void testTranscriptHash() {
    eqHash(gestureHash(), 0x71b3dfb8a4fb64caull, "the frame transcript hash is pinned");
    eqHash(gestureHash(), gestureHash(), "the transcript hash reproduces");

    // Each field is covered: a hash that dropped one would pass an equality
    // check but not these.
    UIRouter router = makeRouter();
    PumpState state;
    InputFrame f;
    f.cursorX = 300;
    f.cursorY = 340;
    f.primaryDown = true;
    f.keys.push_back(KeyEdge{Key::Space, true});
    const ply::FrameRoute base = ply::pumpFrame(router, state, f);

    ply::FrameRoute altered = base;
    check(!altered.pointer.empty(), "the base transcript has a pointer event");
    if (!altered.pointer.empty()) {
        altered.pointer[0].widget += 1;
        check(ply::structuralHash(altered) != ply::structuralHash(base),
              "the hash covers the widget index");
        altered = base;
        altered.pointer[0].worldConsumed = !altered.pointer[0].worldConsumed;
        check(ply::structuralHash(altered) != ply::structuralHash(base),
              "the hash covers the world flag");
        altered = base;
        altered.pointer[0].capturedWidget += 1;
        check(ply::structuralHash(altered) != ply::structuralHash(base),
              "the hash covers the capture index");
    }
    altered = base;
    check(!altered.keys.empty(), "the base transcript has a key");
    if (!altered.keys.empty()) {
        altered.keys[0].globalConsumed = !altered.keys[0].globalConsumed;
        check(ply::structuralHash(altered) != ply::structuralHash(base),
              "the hash covers the global flag");
    }
}

// One line of measured values, printed so the ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence() {
    UIRouter mouseRouter = makeRouter();
    UIRouter phoneRouter = makeRouter();
    PumpState mouseState;
    PumpState phoneState;
    long long identical = 0;
    long long frames = 0;
    for (int i = 0; i < 4; ++i) {
        InputFrame m;
        m.source = PointerSource::HostMouse;
        m.cursorX = 300 + i * 10;
        m.cursorY = 340;
        m.primaryDown = i >= 1 && i <= 2;
        m.keys.push_back(KeyEdge{Key::Space, i == 1});
        InputFrame p = m;
        p.source = PointerSource::VirtualCursor;
        const ply::FrameRoute a = ply::pumpFrame(mouseRouter, mouseState, m);
        const ply::FrameRoute b = ply::pumpFrame(phoneRouter, phoneState, p);
        ++frames;
        if (a == b && ply::structuralHash(a) == ply::structuralHash(b)) ++identical;
    }

    UIRouter edgeRouter = makeRouter();
    PumpState edgeState;
    InputFrame held;
    held.cursorX = 300;
    held.cursorY = 340;
    held.primaryDown = true;
    ply::pumpFrame(edgeRouter, edgeState, held);
    long long idle = 0;
    for (int i = 0; i < 3; ++i) {
        if (ply::pumpFrame(edgeRouter, edgeState, held).pointer.empty()) ++idle;
    }

    std::printf(
        "EVIDENCE input_routing sources=2 frames=%lld identical=%lld idle_silent=%lld "
        "key_nocap=%d key_captured_widget=%d transcript=0x%016llx\n",
        frames, identical, idle,
        makeRouter().routeKey(ply::KeyEvent{Key::Escape, true}).worldConsumed,
        [&]() {
            UIRouter r = makeRouter();
            r.route(ply::PointerEvent{ply::PointerEventType::Press, 300, 340});
            return r.routeKey(ply::KeyEvent{Key::Left, true}).widget;
        }(),
        static_cast<unsigned long long>(gestureHash()));
}

}  // namespace

int main() {
    testOnePathAcrossSources();
    testEdgeDetection();
    testKeyboard();
    testTranscriptHash();
    testLogicalConversion();

    if (failures == 0) {
        printEvidence();
        std::printf("input_routing tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "input_routing tests: %d failure(s)\n", failures);
    return 1;
}
