#pragma once

#include "plyEngine/RenderTarget.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <vector>

namespace ply {

// The HUD-over-world composition (M1a gate): what a frame must draw, in order,
// as data. This is the composition half of the gate clause "the HUD from
// UI_FRAMEWORK.md over a real map, with the world rendering through an
// integer-scaled render target" — every number the GL side needs, produced
// without a GL context so the claim is gradable in ctest.
//
// The two stages are deliberately separate, because that separation *is* the
// clause (UI_FRAMEWORK.md §1):
//
//   1. world  — a7's layers, in INTERNAL pixels (640x360). Rendered into the
//      render target, then upscaled by the integer factor.
//   2. HUD    — laid out against the same 640x360 LOGICAL viewport, then drawn
//      ABOVE the upscale at `factor * uiScale`, so a resize moves the world's
//      present rect but never the HUD's layout, and a 1 px border stays 1
//      logical px however large the window is.
//
// Nothing here allocates a texture or touches GL. a7's header explains why that
// half was deferred; with the offscreen video driver it is now verifiable on a
// headless host (see the a11 receipt), and it consumes exactly these numbers.

/// The logical viewport HUD widgets lay out against. ADR 0012 fixes it at
/// 640x360 — the same surface the world renders at internally, which is why the
/// HUD does not shift when the window changes shape.
struct HudViewport {
    int width = 640;
    int height = 360;

    bool operator==(const HudViewport&) const = default;
};

/// One composed frame.
struct HudFrame {
    /// a7's plan: internal size, integer factor and the centred present rect in
    /// output pixels. Carried rather than re-derived, so there is one
    /// implementation of the upscale policy.
    RenderTargetPlan plan;
    /// World layers in internal px, in a7's draw order (empty layers dropped).
    std::vector<SceneLayerDraw> world;
    /// HUD rects in logical px — the layout, which does not depend on the window.
    std::vector<UIRect> hudLogical;
    /// The same rects in output px: `logical * plan.factor * uiScale`.
    std::vector<UIRect> hudOutput;
    /// The HUD's clickable rects in logical px, exactly `hudLogical`, ready for
    /// UIRouter::setInteractive. One list, not a second derivation: what the
    /// router hit-tests is what the HUD draws.
    std::vector<UIRect> interactive;

    bool operator==(const HudFrame&) const = default;
};

/// Composes a frame for an output window of `outputWidth` x `outputHeight`.
///
///  - `plan` comes from makeRenderTargetPlan(viewport.width, viewport.height,
///    outputWidth, outputHeight) — the world's integer upscale (a7).
///  - `world` is the scene in a7's draw order, in internal px.
///  - HUD widgets are laid out by layoutWidgets (a4) against the logical
///    viewport; `uiScale` is the explicit UI factor (1..3), independent of the
///    window. The integer `factor * uiScale` is the HUD's output multiplier, so
///    every output rect is an exact multiple of it and no pixel is fractional.
HudFrame composeHudFrame(const HudViewport& viewport, const std::vector<UIWidgetSpec>& hud,
                         int uiScale, const SceneSubmission& scene, int outputWidth,
                         int outputHeight);

/// FNV-1a 64 over the composition: the plan (internal size, factor, destination),
/// the world layers with their quads, then the HUD's logical rects, then the
/// stage boundary is implicit in that order. Integers only (ADR 0014).
std::uint64_t structuralHash(const HudFrame& frame);

}  // namespace ply
