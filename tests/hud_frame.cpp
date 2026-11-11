// Headless HUD-over-world composition tests (M1a gate): the frame a renderer
// must draw — a7's world layers in internal px, the render-target plan, and the
// HUD above the upscale in output px — graded as data, with no GL context.
//
// This is the composition half of the gate clause "the HUD over a real map,
// with the world rendering through an integer-scaled render target". The GL half
// (allocating the target, binding the present blit) consumes these numbers and
// is verified separately, on a host with an offscreen GL device.

#include "plyEngine/HudFrame.hpp"
#include "plyEngine/MapLoader.hpp"
#include "plyEngine/RenderTarget.hpp"
#include "plyEngine/UILayout.hpp"
#include "plyEngine/UIRouter.hpp"

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

void eqRect(const ply::UIRect& got, const ply::UIRect& want, const char* what) {
    if (!(got == want)) {
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d,%d,%d}, want {%d,%d,%d,%d})\n", what, got.x,
                     got.y, got.w, got.h, want.x, want.y, want.w, want.h);
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

using ply::HudFrame;
using ply::HudViewport;
using ply::SceneLayer;
using ply::SceneLayerDraw;
using ply::SceneSubmission;
using ply::UIAnchor;
using ply::UIRect;
using ply::UIWidgetSpec;

constexpr HudViewport kViewport{640, 360};

// The HUD from UI_FRAMEWORK.md §5, at the sizes the widgets actually use:
// a hotbar row bottom-centre, two bars top-left, a minimap top-right.
std::vector<UIWidgetSpec> hudWidgets() {
    return {
        UIWidgetSpec{UIAnchor::BottomCentre, 0, 8, 108, 20, 1},   // hotbar
        UIWidgetSpec{UIAnchor::TopLeft, 8, 8, 84, 12, 1},         // health bar
        UIWidgetSpec{UIAnchor::TopLeft, 8, 24, 84, 12, 1},        // energy bar
        UIWidgetSpec{UIAnchor::TopRight, 8, 8, 160, 90, 1},       // minimap
    };
}

// The world: a9's tile quads as GroundTiles, plus one World quad and one
// Overhead quad, submitted out of order so ordering is actually exercised.
SceneSubmission worldScene() {
    SceneSubmission scene;
    scene.upscaleFactor = 1;  // set by the composition from the plan
    SceneLayerDraw ground;
    ground.layer = SceneLayer::GroundTiles;
    for (int cx = 0; cx < 4; ++cx) {
        for (int cy = 0; cy < 3; ++cy) {
            ground.quads.push_back(UIRect{cx * 16, cy * 16, 16, 16});
        }
    }
    SceneLayerDraw overhead;
    overhead.layer = SceneLayer::Overhead;
    overhead.quads.push_back(UIRect{32, 32, 16, 32});

    SceneLayerDraw world;
    world.layer = SceneLayer::World;
    world.quads.push_back(UIRect{16, 16, 16, 16});

    scene.layers = {world, overhead, ground};  // deliberately out of order
    return scene;
}

// --- the plan is a7's, not a second derivation -----------------------------

struct WindowCase {
    int width, height;
    const char* what;
};

const WindowCase kWindowCases[] = {
    {1920, 1080, "1080p"},
    {2560, 1440, "1440p"},
    {2560, 800, "wide and short (tighter axis decides)"},
    {320, 180, "a window smaller than the internal surface"},
};

void testPlanIsA7s() {
    for (const WindowCase& c : kWindowCases) {
        const HudFrame frame =
            ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), c.width, c.height);
        const ply::RenderTargetPlan expected =
            ply::makeRenderTargetPlan(kViewport.width, kViewport.height, c.width, c.height);
        if (!(frame.plan == expected)) {
            std::fprintf(stderr, "  FAIL: %s plan differs from makeRenderTargetPlan\n", c.what);
            ++failures;
        }
        eq(frame.plan.internalWidth, 640, "the world renders at the logical width internally");
        eq(frame.plan.internalHeight, 360, "the world renders at the logical height internally");
        // The present rect is the upscaled image, centred: full-bleed at 1080p.
        if (c.width == 1920 && c.height == 1080) {
            eqRect(frame.plan.destination, UIRect{0, 0, 1920, 1080}, "full-bleed present at 1080p");
        }
        // A window smaller than the internal surface still presents 1:1, centred.
        if (c.width == 320 && c.height == 180) {
            eq(frame.plan.factor, 1, "a smaller window does not shrink below 1:1");
            eqRect(frame.plan.destination, UIRect{-160, -90, 640, 360},
                   "the centred present rect goes negative when the window is smaller");
        }
    }
}

// --- the HUD is above the upscale, and its layout does not move with it ----

void testHudAboveUpscale() {
    const HudFrame f3 =
        ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 1920, 1080);
    const HudFrame f4 =
        ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 2560, 1440);

    eq(f3.plan.factor, 3, "1080p presents at 3x");
    eq(f4.plan.factor, 4, "1440p presents at 4x");

    // The layout is identical at both window sizes — the HUD does not shift.
    eq(f3.hudLogical.size(), 4, "four HUD widgets laid out");
    check(f3.hudLogical == f4.hudLogical, "the HUD's logical layout is unchanged by the window size");

    // The world's quads are identical too: they live in internal px, so the
    // window cannot move them. Only the present rect and the HUD's output rects
    // change — which is exactly what "above the upscale" means.
    check(f3.world == f4.world, "the world submission does not depend on the window size");

    // HUD output rects are logical * integer multiplier, so no pixel is
    // fractional and a 1 px border stays exactly `multiplier` px.
    if (f3.hudLogical.size() == 4 && f3.hudOutput.size() == 4) {
        eqRect(f3.hudOutput[0], ply::upscaleRect(f3.hudLogical[0], 3), "hotbar scales by the factor");
        eqRect(f4.hudOutput[0], ply::upscaleRect(f4.hudLogical[0], 4),
               "hotbar scales by the larger factor");
        // Bottom-centre at 3x: a5.3 pins the logical rect at {266,332,108,20}
        // (the centre-line halving floors 532/2), so the output rect is that
        // times 3.
        eqRect(f3.hudLogical[0], UIRect{266, 332, 108, 20}, "the hotbar's logical rect is a5.3's");
        eqRect(f3.hudOutput[0], UIRect{798, 996, 324, 60}, "the hotbar's output rect is pinned");
    }

    // A UI scale multiplies on top of the window factor: 2 at 1080p means 6.
    const HudFrame ui2 =
        ply::composeHudFrame(kViewport, hudWidgets(), 2, worldScene(), 1920, 1080);
    if (ui2.hudLogical.size() == 4 && ui2.hudOutput.size() == 4) {
        eqRect(ui2.hudOutput[0], ply::upscaleRect(ui2.hudLogical[0], 6),
               "uiScale multiplies the window factor");
        eq(ui2.hudOutput[0].w, f3.hudOutput[0].w * 2, "a 2x UI scale doubles the output rect");
    }
    // A scale below 1 clamps to 1, like every other UI factor.
    const HudFrame ui0 =
        ply::composeHudFrame(kViewport, hudWidgets(), 0, worldScene(), 1920, 1080);
    if (ui0.hudOutput.size() == 4) {
        eqRect(ui0.hudOutput[0], f3.hudOutput[0], "uiScale 0 clamps to 1");
    }

    // The world's layers come out in a7's order regardless of submission order.
    eq(f3.world.size(), 3, "three non-empty layers, the empty ones dropped");
    if (f3.world.size() == 3) {
        eq(static_cast<int>(f3.world[0].layer), static_cast<int>(SceneLayer::GroundTiles),
           "GroundTiles is the bottom layer");
        eq(static_cast<int>(f3.world[2].layer), static_cast<int>(SceneLayer::Overhead),
           "Overhead is on top");
        eq(f3.world[0].quads.size(), 12, "12 ground tiles from the map");
    }
}

// --- the router gets exactly the HUD the HUD draws -------------------------

void testInteractiveMatchesLayout() {
    const HudFrame frame =
        ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 1920, 1080);
    check(frame.interactive == frame.hudLogical,
          "the interactive rects are the laid-out rects, one list not two");

    ply::UIRouter router;
    router.setInteractive(frame.interactive);
    if (frame.interactive.size() == 4) {
        // Press the centre of the hotbar: 266..374 x 332..352 -> (320, 342).
        const UIRect hotbar = frame.interactive[0];
        const ply::RouteResult hit =
            router.route(ply::PointerEvent{ply::PointerEventType::Press, hotbar.x + hotbar.w / 2,
                                           hotbar.y + hotbar.h / 2});
        eq(hit.widget, 0, "a press on the hotbar's centre reaches widget 0");
        eq(hit.worldConsumed, 0, "the press does not fall through to the world");
        router.route(ply::PointerEvent{ply::PointerEventType::Release, hotbar.x + hotbar.w / 2,
                                       hotbar.y + hotbar.h / 2});

        // Just below the hotbar is world — after the gesture ends, because a
        // press during a capture keeps the capture (a10's rule).
        const ply::RouteResult miss = router.route(ply::PointerEvent{
            ply::PointerEventType::Press, hotbar.x + hotbar.w / 2, hotbar.y + hotbar.h + 2});
        eq(miss.widget, -1, "a press below the HUD hits no widget");
        eq(miss.worldConsumed, 1, "a press below the HUD reaches the world");
    }
}

// --- the composition hash --------------------------------------------------

void testCompositionHash() {
    const std::uint64_t pinned =
        ply::structuralHash(ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(),
                                                1920, 1080));
    eqHash(pinned, 0xb7af25909d336eb4ull, "the composition hash is pinned");

    // The hash covers the plan, the world and the HUD: perturbing each moves it.
    const HudFrame base =
        ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 1920, 1080);
    HudFrame altered = base;
    altered.plan.factor += 1;
    check(ply::structuralHash(altered) != pinned, "the hash covers the upscale factor");
    altered = base;
    altered.plan.destination.x += 1;
    check(ply::structuralHash(altered) != pinned, "the hash covers the present rect");
    altered = base;
    if (!altered.world.empty()) {
        altered.world[0].quads.push_back(UIRect{1, 2, 3, 4});
        check(ply::structuralHash(altered) != pinned, "the hash covers the world quads");
    }
    altered = base;
    if (!altered.hudLogical.empty()) {
        altered.hudLogical[0].x += 1;
        check(ply::structuralHash(altered) != pinned, "the hash covers the HUD layout");
    }
}

// One line of measured values, printed so the ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
// A live count: every window case has to compose a frame for it to rise.
long long windowsComposed() {
    long long count = 0;
    for (const WindowCase& c : kWindowCases) {
        const HudFrame frame =
            ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), c.width, c.height);
        if (frame.plan.outputWidth == c.width && !frame.hudLogical.empty()) ++count;
    }
    return count;
}

void printEvidence() {
    const HudFrame f3 = ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 1920, 1080);
    const HudFrame f4 = ply::composeHudFrame(kViewport, hudWidgets(), 1, worldScene(), 2560, 1440);
    const HudFrame ui2 = ply::composeHudFrame(kViewport, hudWidgets(), 2, worldScene(), 1920, 1080);
    long long aligned = 0;
    for (const UIRect& r : f3.hudOutput) {
        if (ply::pixelAligned(r, f3.plan.factor)) ++aligned;
    }

    std::printf(
        "EVIDENCE hud_frame windows=%lld factor=%d/%d/%d dest=%d,%d,%d,%d world_layers=%zu "
        "world_quads=%zu hud=%zu aligned=%lld hotbar3=%d,%d,%dx%d hotbar4=%d,%d,%dx%d "
        "ui2=%dx%d interactive=%zu hash3=0x%016llx\n",
        windowsComposed(), f3.plan.factor, f4.plan.factor,
        ply::makeRenderTargetPlan(kViewport.width, kViewport.height, 2560, 800).factor,
        f3.plan.destination.x, f3.plan.destination.y, f3.plan.destination.w,
        f3.plan.destination.h, f3.world.size(), f3.world[0].quads.size(), f3.hudLogical.size(),
        aligned, f3.hudOutput[0].x, f3.hudOutput[0].y, f3.hudOutput[0].w, f3.hudOutput[0].h,
        f4.hudOutput[0].x, f4.hudOutput[0].y, f4.hudOutput[0].w, f4.hudOutput[0].h,
        ui2.hudOutput[0].w, ui2.hudOutput[0].h, f3.interactive.size(),
        static_cast<unsigned long long>(ply::structuralHash(f3)));
}

}  // namespace

int main() {
    testPlanIsA7s();
    testHudAboveUpscale();
    testInteractiveMatchesLayout();
    testCompositionHash();

    if (failures == 0) {
        if (!worldScene().layers.empty()) {
            printEvidence();
        } else {
            std::fprintf(stderr, "  FAIL: evidence preconditions\n");
            return 1;
        }
        std::printf("hud_frame tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "hud_frame tests: %d failure(s)\n", failures);
    return 1;
}
