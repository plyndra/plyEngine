#pragma once

#include "plyEngine/RenderTarget.hpp"

#include <cstdint>

namespace ply {

/// The GL half of a7's render target (M1a gate, ARCHITECTURE_TARGET.md §2).
///
/// The headless half — the plan, the pixel mapping, the camera snap and the
/// scene layer order — is `RenderTarget.hpp`, pure data. This class is the
/// consumer the header promised: it owns the world's framebuffer object and
/// colour texture at the plan's internal resolution, brackets the scene draw
/// between `begin()`/`end()` (the caller submits a7's layers in order inside
/// that bracket), and performs the present blit to the default framebuffer at
/// `plan.destination`.
///
/// The blit is `GL_NEAREST` with an exact integer factor, so every output pixel
/// is a copy of exactly one internal pixel — k×k blocks, never a blended pixel
/// (E-a7.4). That is the property the structural readback asserts: the back
/// buffer is compared against the internal surface it must reproduce, computed
/// from the same data, not against a committed pixel golden (ADR 0014 — pixel
/// goldens are container-pinned, opt-in, never a CI default).
///
/// GL state contract, kept deliberately narrow:
///  - `begin()` binds the FBO, sets the internal viewport and clears it. Blend
///    and depth state are the caller's; opaque scene quads do not need either.
///  - `present()` binds the default framebuffer, letterbox-clears it, and blits.
///    It sets the window viewport (so HUD draws after the present target the
///    window) and nothing else — a caller that enabled blending keeps it.
///  - Every call needs a current GL context (App owns one).
class PresentPass {
public:
    /// The clear/letterbox colour, in the engine's sRGB-as-bytes convention
    /// (Texture.hpp's note): what is written is what is compared.
    struct ClearColor {
        float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
    };

    /// Allocates the internal surface for `plan`'s internal size. A GL context
    /// must be current; on allocation failure the framebuffer stays 0 and the
    /// first `begin()` reports the GL error to stderr.
    explicit PresentPass(const RenderTargetPlan& plan);
    ~PresentPass();

    PresentPass(const PresentPass&) = delete;
    PresentPass& operator=(const PresentPass&) = delete;

    /// (Re)allocates the internal surface when `plan`'s internal size differs
    /// from the current one. The present factor and destination live in the
    /// plan passed to `present()`; only the internal size is state here.
    void setPlan(const RenderTargetPlan& plan);

    /// Binds the internal framebuffer, sets the internal viewport and clears
    /// it. Draw the scene's layers in a7's order between this and `end()`.
    void begin(ClearColor clear = {0.0f, 0.0f, 0.0f, 1.0f});

    /// Returns to the default framebuffer (viewport untouched — `present()`
    /// sets the window viewport, and App re-syncs it around the loop anyway).
    void end();

    /// Letterbox-clears the default framebuffer at the plan's output size and
    /// blits the internal texture to `plan.destination` with `GL_NEAREST`.
    ///
    /// `plan.destination` is in top-left output pixels (the UIRect convention).
    /// GL's rectangles are bottom-up, so the destination is converted on the
    /// way in; the texture's bottom row lands at the destination's bottom edge.
    /// With either renderer convention — the engine's y-up cameras, or the
    /// y-down internal-px ortho the structural tests use — the presented image
    /// is upright.
    ///
    /// A destination larger than the window cannot happen above factor 1
    /// (`makeRenderTargetPlan` only centres a shown image that fits), but at
    /// factor 1 a window smaller than the internal resolution centres a
    /// negative destination. The blit is then clipped, and the clip is snapped
    /// outward to the factor grid so the visible region stays exact k×k blocks:
    /// the clipped edge shows letterbox colour, never a partially covered
    /// pixel. A destination entirely outside the window blits nothing.
    void present(const RenderTargetPlan& plan, ClearColor letterbox = {0.0f, 0.0f, 0.0f, 1.0f});

    /// The GL names, for tests and callers that need to read the surface back.
    /// 0 before the first successful allocation.
    unsigned int framebuffer() const { return fbo_; }
    unsigned int texture() const { return texture_; }
    int internalWidth() const { return width_; }
    int internalHeight() const { return height_; }

private:
    void allocate(int width, int height);

    unsigned int fbo_ = 0;
    unsigned int texture_ = 0;
    bool complete_ = false;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace ply
