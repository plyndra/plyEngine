// Headless input-routing tests (M1a a10): E-a10.1 — a click inside a widget
// does not reach the world, and press-capture keeps the release after the
// cursor leaves the widget — plus the scripted synthetic input source the unit
// exists for. Pure data: no window, no device, no clock, no float.

#include "plyEngine/UIRouter.hpp"
#include "plyEngine/UILayout.hpp"
#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <cstdio>
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

using ply::PointerEvent;
using ply::PointerEventType;
using ply::RouteResult;
using ply::UIRect;
using ply::UIRouter;

// Two side-by-side widgets, in render order (index 1 draws over index 0).
const std::vector<UIRect> kWidgets = {
    {100, 100, 40, 20},  // 0: x 100..140, y 100..120
    {124, 100, 40, 20},  // 1: x 124..164, y 100..120 — overlaps 0 at 124..140
};

PointerEvent move(int x, int y) { return PointerEvent{PointerEventType::Move, x, y}; }
PointerEvent press(int x, int y) { return PointerEvent{PointerEventType::Press, x, y}; }
PointerEvent release(int x, int y) { return PointerEvent{PointerEventType::Release, x, y}; }

UIRouter makeRouter() {
    UIRouter router;
    router.setInteractive(kWidgets);
    return router;
}

// --- E-a10.1: UI consumes before world ---------------------------------------------

void testUiConsumesBeforeWorld() {
    UIRouter router = makeRouter();

    // A press inside widget 0: the UI takes it, the world never sees it.
    {
        const RouteResult r = router.route(press(110, 110));
        eq(r.widget, 0, "press inside widget 0 routes to widget 0");
        check(r.uiConsumed(), "the UI consumed the press");
        check(!r.worldConsumed, "and the world did not");
        check(r.globalConsumed, "the global handler still sees it");
        eq(r.capturedWidget, 0, "the press captured widget 0");
    }
    // The matching release is consumed by the capture, not the world.
    {
        const RouteResult r = router.route(release(110, 110));
        eq(r.widget, 0, "release over the widget returns to it");
        check(!r.worldConsumed, "the world still does not see it");
        eq(r.capturedWidget, -1, "and the capture is cleared");
    }
    // A press on empty space goes to the world and captures nothing.
    {
        const RouteResult r = router.route(press(10, 10));
        eq(r.widget, -1, "press on empty space routes to no widget");
        check(r.worldConsumed, "the world consumes it");
        check(!r.uiConsumed(), "the UI does not");
        eq(r.capturedWidget, -1, "and nothing is captured");
    }
    // A move over empty space is a world move (aim/interact), not a UI hover.
    {
        const RouteResult r = router.route(move(10, 10));
        check(r.worldConsumed, "a move over empty space belongs to the world");
        check(!r.uiConsumed(), "and not to the UI");
    }
    // A move over a widget is UI hover.
    {
        const RouteResult r = router.route(move(110, 110));
        eq(r.widget, 0, "a move over a widget is that widget's hover");
        check(!r.worldConsumed, "and the world does not see the hover");
    }
}

// --- E-a10.1: press-capture keeps the release ---------------------------------------

void testPressCaptureKeepsRelease() {
    UIRouter router = makeRouter();

    // Press on widget 0, drag far away, release over nothing.
    eq(router.route(press(110, 110)).capturedWidget, 0, "press captures widget 0");

    // The drag leaves the widget: still consumed by the capture, so dragging a
    // HUD button does not turn into a world move.
    {
        const RouteResult r = router.route(move(500, 500));
        eq(r.widget, 0, "a move far outside still goes to the capturing widget");
        check(!r.worldConsumed, "the world never sees the drag");
        eq(r.capturedWidget, 0, "and the capture is held");
    }
    // The release is the case the rule exists for.
    {
        const RouteResult r = router.route(release(500, 500));
        eq(r.widget, 0, "the release after leaving still goes to the widget");
        check(!r.worldConsumed, "the world never sees the release either");
        eq(r.capturedWidget, -1, "and the capture ends");
    }
    // After the release the router is free again: the next move at that point
    // is a world move, so the capture really did end.
    {
        const RouteResult r = router.route(move(500, 500));
        check(r.worldConsumed, "after release, the same point is a world move again");
        check(!r.uiConsumed(), "and no widget takes it");
    }
}

void testCaptureEdges() {
    // A release with no press: nothing to capture, so it goes to the world.
    {
        UIRouter router = makeRouter();
        const RouteResult r = router.route(release(110, 110));
        eq(r.widget, -1, "a release with no press is not a widget gesture");
        check(r.worldConsumed, "and belongs to the world");
        eq(r.capturedWidget, -1, "nothing is captured");
    }
    // Press widget 0, release over widget 1: the capture owns the release, so
    // widget 1 does not steal a gesture it never started.
    {
        UIRouter router = makeRouter();
        eq(router.route(press(110, 110)).widget, 0, "press grabs widget 0");
        const RouteResult r = router.route(release(160, 110));
        eq(r.widget, 0, "the release stays with the capturing widget, not the one under the cursor");
        eq(r.capturedWidget, -1, "and ends the capture");
    }
    // A second press during a capture keeps the original capture, and the
    // resulting release still ends it (a gesture cannot leave the router stuck).
    {
        UIRouter router = makeRouter();
        eq(router.route(press(110, 110)).capturedWidget, 0, "first press captures 0");
        const RouteResult second = router.route(press(160, 110));
        eq(second.widget, 0, "a press during a capture belongs to the capture");
        eq(second.capturedWidget, 0, "and does not transfer it");
        const RouteResult rel = router.route(release(160, 110));
        eq(rel.capturedWidget, -1, "the release still ends the capture");
    }
    // cancelCapture drops it without a release.
    {
        UIRouter router = makeRouter();
        router.route(press(110, 110));
        router.cancelCapture();
        const RouteResult r = router.route(move(500, 500));
        check(r.worldConsumed, "after cancelCapture the pointer is the world's again");
        eq(r.capturedWidget, -1, "and nothing is captured");
    }
    // setInteractive clears the capture: old indices no longer mean anything.
    {
        UIRouter router = makeRouter();
        router.route(press(110, 110));
        router.setInteractive({UIRect{0, 0, 10, 10}});
        eq(router.capturedWidget(), -1, "replacing the rect list clears the capture");
        const RouteResult r = router.route(move(110, 110));
        check(r.worldConsumed, "and the old widget no longer takes events");
    }
}

// --- overlapping widgets -----------------------------------------------------------------

void testOverlapPicksTopmost() {
    UIRouter router = makeRouter();
    // (130,110) is inside both; index 1 draws over index 0, so it wins.
    eq(router.route(move(130, 110)).widget, 1, "the topmost widget wins an overlap");
    // (110,110) is only inside widget 0.
    eq(router.route(move(110, 110)).widget, 0, "a non-overlapping point picks its own widget");
    // Widget 0 spans x 100..140, widget 1 spans 124..164, so 100..123 is
    // widget 0 alone, 124..139 is the overlap (1 wins), 140..163 is 1 alone.
    // Edges are half-open: these are the exact boundary pixels on both sides.
    eq(router.route(move(123, 110)).widget, 0, "x=123 is widget 0's last pixel alone");
    eq(router.route(move(124, 110)).widget, 1, "x=124 starts the overlap, topmost wins");
    eq(router.route(move(139, 110)).widget, 1, "x=139 is still the overlap, still topmost");
    eq(router.route(move(140, 110)).widget, 1, "x=140 is outside widget 0 but inside widget 1");
    eq(router.route(move(164, 110)).widget, -1, "x=164 is the first pixel outside both");
    eq(router.route(move(163, 110)).widget, 1, "x=163 is widget 1's last pixel");
    eq(router.route(move(100, 120)).widget, -1, "y=120 is the first row below both");
    eq(router.route(move(100, 119)).widget, 0, "y=119 is their last row");
    eq(router.route(move(99, 110)).widget, -1, "x=99 is the first pixel left of both");
}

// --- the scripted synthetic input source -------------------------------------------------

ply::InputScript gestureScript() {
    ply::InputScript script;
    script.events = {
        move(110, 110),    // hover widget 0
        press(110, 110),   // capture 0
        move(500, 500),    // drag away, still captured
        release(500, 500), // release off-widget, still captured, then freed
        move(10, 10),      // world move
        press(10, 10),     // world press
        release(10, 10),   // world release
        press(130, 110),   // capture the topmost widget
        release(130, 110),
    };
    return script;
}

long long testScriptedReplay() {
    UIRouter router = makeRouter();
    const std::vector<RouteResult> transcript = ply::replay(router, gestureScript());

    eq(static_cast<long long>(transcript.size()), 9, "one result per scripted event");
    if (transcript.size() != 9) return 0;

    // The expected transcript, pinned event by event.
    struct Row {
        int widget;
        bool world;
        int captured;
    };
    const Row expected[9] = {
        {0, false, -1},   // hover widget 0
        {0, false, 0},    // press captures 0
        {0, false, 0},    // drag away, still captured
        {0, false, -1},   // release off-widget ends it
        {-1, true, -1},   // world move
        {-1, true, -1},   // world press
        {-1, true, -1},   // world release
        {1, false, 1},    // topmost widget captures
        {1, false, -1},   // and releases
    };
    long long matched = 0;
    for (std::size_t i = 0; i < 9; ++i) {
        const RouteResult& r = transcript[i];
        if (r.widget == expected[i].widget && r.worldConsumed == expected[i].world &&
            r.capturedWidget == expected[i].captured)
            ++matched;
        else
            std::fprintf(stderr, "  FAIL: transcript event %zu (got widget %d world %d cap %d)\n", i,
                         r.widget, r.worldConsumed ? 1 : 0, r.capturedWidget);
    }
    eq(matched, 9, "the scripted transcript matches the pinned sequence");

    // Determinism: the same script through a fresh router gives the same
    // transcript hash, and the script itself hashes reproducibly.
    {
        UIRouter again = makeRouter();
        const std::vector<RouteResult> second = ply::replay(again, gestureScript());
        check(ply::structuralHash(second) == ply::structuralHash(transcript),
              "replaying the same script is deterministic");
    }
    // A different script gives a different transcript.
    {
        ply::InputScript other = gestureScript();
        other.events.push_back(move(999, 999));
        UIRouter third = makeRouter();
        check(ply::structuralHash(ply::replay(third, other)) !=
                  ply::structuralHash(transcript),
              "a different script has a different transcript hash");
    }
    // The transcript hash covers routing decisions, not just the length: the
    // same script against a router with different rects must differ.
    {
        UIRouter moved;
        moved.setInteractive({UIRect{400, 400, 40, 20}});
        check(ply::structuralHash(ply::replay(moved, gestureScript())) !=
                  ply::structuralHash(transcript),
              "the transcript hash covers where events went");
    }

    // The invariant the gate cares about: no event was ever delivered to both.
    long long both = 0;
    for (const RouteResult& r : transcript) {
        if (r.uiConsumed() && r.worldConsumed) ++both;
    }
    eq(both, 0, "no event reaches both the UI and the world");
    return 9;
}

// --- hashes ------------------------------------------------------------------------------

/// The transcript hash must cover every field of a RouteResult. Tested on
/// hand-built results rather than through the router, because some fields are
/// currently derivable from others there — `worldConsumed` is always the
/// negation of "a widget took it" — so no routed transcript can isolate it.
/// (Found by fault injection: dropping the world flag from the hash survived
/// the whole suite, because every routed difference also changed `widget`.)
void testTranscriptHashFields() {
    const RouteResult base{-1, true, true, -1};
    const std::vector<RouteResult> reference = {base};
    const std::uint64_t baseHash = ply::structuralHash(reference);

    struct Perturbation {
        RouteResult changed;
        const char* what;
    };
    const Perturbation cases[] = {
        {RouteResult{0, true, true, -1}, "the widget index"},
        {RouteResult{-1, false, true, -1}, "the world flag"},
        {RouteResult{-1, true, false, -1}, "the global flag"},
        {RouteResult{-1, true, true, 2}, "the capture index"},
    };
    for (const Perturbation& c : cases) {
        const std::vector<RouteResult> changed = {c.changed};
        if (ply::structuralHash(changed) == baseHash) {
            std::fprintf(stderr, "  FAIL: transcript hash ignores %s\n", c.what);
            ++failures;
        }
    }
    // And the count is covered: one result vs two.
    const std::vector<RouteResult> two = {base, base};
    check(ply::structuralHash(two) != baseHash, "the transcript hash covers the count");
    // Reproducible from scratch.
    eq(ply::structuralHash(reference), ply::structuralHash({RouteResult{-1, true, true, -1}}),
       "the transcript hash is reproducible");
}

void testHashes() {
    const ply::InputScript script = gestureScript();
    eq(ply::structuralHash(script) == ply::structuralHash(gestureScript()) ? 0 : 1, 0,
       "the script hash is reproducible");

    // The script hash covers coordinates, not just the event count.
    {
        ply::InputScript nudged = gestureScript();
        if (!nudged.events.empty()) nudged.events[0].x += 1;
        check(ply::structuralHash(nudged) != ply::structuralHash(script),
              "the script hash covers coordinates");
    }
    // And the event types, not just positions.
    {
        ply::InputScript retyped = gestureScript();
        if (!retyped.events.empty()) retyped.events[0].type = PointerEventType::Press;
        check(ply::structuralHash(retyped) != ply::structuralHash(script),
              "the script hash covers event types");
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence(long long scriptedEvents) {
    UIRouter router = makeRouter();
    const std::vector<RouteResult> transcript = ply::replay(router, gestureScript());

    // The three decisive decisions, replayed on a fresh router so the numbers
    // in the line stand alone.
    UIRouter probe = makeRouter();
    const RouteResult inside = probe.route(press(110, 110));
    const RouteResult dragged = probe.route(move(500, 500));
    const RouteResult offRelease = probe.route(release(500, 500));
    const RouteResult worldPress = makeRouter().route(press(10, 10));

    long long both = 0;
    for (const RouteResult& r : transcript) {
        if (r.uiConsumed() && r.worldConsumed) ++both;
    }

    std::printf(
        "EVIDENCE ui_router scripted=%lld inside=%d/%d drag=%d/%d offrelease=%d/%d "
        "worldpress=%d/%d both=%lld overlap=%d transcript=0x%016llx script_hash=0x%016llx\n",
        scriptedEvents, inside.widget, inside.worldConsumed ? 1 : 0, dragged.widget,
        dragged.worldConsumed ? 1 : 0, offRelease.widget, offRelease.worldConsumed ? 1 : 0,
        worldPress.widget, worldPress.worldConsumed ? 1 : 0, both,
        makeRouter().route(move(130, 110)).widget,
        static_cast<unsigned long long>(ply::structuralHash(transcript)),
        static_cast<unsigned long long>(ply::structuralHash(gestureScript())));
}

}  // namespace

int main() {
    testUiConsumesBeforeWorld();
    testPressCaptureKeepsRelease();
    testCaptureEdges();
    testOverlapPicksTopmost();
    const long long scripted = testScriptedReplay();
    testTranscriptHashFields();
    testHashes();

    if (failures == 0) {
        printEvidence(scripted);
        std::printf("ui_router tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_router tests: %d failure(s)\n", failures);
    return 1;
}
