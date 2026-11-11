#include "plyEngine/HudFrame.hpp"

#include "plyEngine/StructuralHash.hpp"

namespace ply {

HudFrame composeHudFrame(const HudViewport& viewport, const std::vector<UIWidgetSpec>& hud,
                         int uiScale, const SceneSubmission& scene, int outputWidth,
                         int outputHeight) {
    HudFrame frame;

    // The world's upscale policy is a7's, called rather than copied: one
    // implementation of "largest integer factor, centred present rect".
    frame.plan = makeRenderTargetPlan(viewport.width, viewport.height, outputWidth, outputHeight);

    // Draw order is data (a7): the caller's submission order does not matter.
    frame.world = orderSceneLayers(scene.layers);

    // The HUD lays out against the logical viewport, never the window, which is
    // why a resize cannot move it.
    frame.hudLogical = layoutWidgets(hud, viewport.width, viewport.height);

    // Above the upscale: the output multiplier is the window's integer factor
    // times the user's explicit UI factor (clamped to 1, like every UI scale).
    const int ui = uiScale < 1 ? 1 : uiScale;
    const int multiplier = frame.plan.factor * ui;
    frame.hudOutput.reserve(frame.hudLogical.size());
    for (const UIRect& rect : frame.hudLogical) {
        frame.hudOutput.push_back(upscaleRect(rect, multiplier));
    }

    // What the router hit-tests is what the HUD draws — the same list, so a
    // layout change cannot leave a stale hit box behind.
    frame.interactive = frame.hudLogical;
    return frame;
}

std::uint64_t structuralHash(const HudFrame& frame) {
    StructuralHasher hasher;
    hasher.i32(frame.plan.internalWidth);
    hasher.i32(frame.plan.internalHeight);
    hasher.i32(frame.plan.outputWidth);
    hasher.i32(frame.plan.outputHeight);
    hasher.i32(frame.plan.factor);
    hasher.i32(frame.plan.destination.x);
    hasher.i32(frame.plan.destination.y);
    hasher.i32(frame.plan.destination.w);
    hasher.i32(frame.plan.destination.h);

    // The world, in draw order: layer token, quad count, then each quad.
    hasher.u64(frame.world.size());
    for (const SceneLayerDraw& layer : frame.world) {
        hasher.i32(static_cast<std::int32_t>(layer.layer));
        hasher.u64(layer.quads.size());
        for (const UIRect& quad : layer.quads) {
            hasher.i32(quad.x);
            hasher.i32(quad.y);
            hasher.i32(quad.w);
            hasher.i32(quad.h);
        }
    }

    // The HUD above the upscale, in logical px: the layout is the state that
    // matters (the output rects are a pure function of it and the multiplier).
    hasher.u64(frame.hudLogical.size());
    for (const UIRect& rect : frame.hudLogical) {
        hasher.i32(rect.x);
        hasher.i32(rect.y);
        hasher.i32(rect.w);
        hasher.i32(rect.h);
    }
    return hasher.value();
}

}  // namespace ply
