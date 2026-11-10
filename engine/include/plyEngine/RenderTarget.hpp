#pragma once

#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace ply {

/// Render-target and scene-order policy (M1a a7, ARCHITECTURE_TARGET.md §11).
///
/// This is the *headless half* of a7: what the internal resolution is, how the
/// integer upscale maps a pixel, how the camera snaps, and what order the
/// scene's layers are submitted in. All of it is pure data, which is what makes
/// the three a7 graders structural instead of framebuffer comparisons (ADR
/// 0014). The GL side — allocating the world framebuffer and binding it for the
/// present blit — is a thin wrapper around these numbers and lands with the
/// renderer wiring, where a GL context exists; no test here needs one, and the
/// M1a gate forbids a GPU dependency in these tests.

// --- integer upscale ---------------------------------------------------------

/// The largest integer k with internalWidth*k <= outputWidth and
/// internalHeight*k <= outputHeight, never below 1 (a window smaller than the
/// internal resolution still upscales 1:1 rather than shrinking). ADR 0012:
/// 640x360 internal gives 3 at 1080p and 4 at 1440p.
/// Non-positive inputs yield 1, the identity factor.
int integerUpscale(int internalWidth, int internalHeight, int outputWidth, int outputHeight);

/// Maps an internal-resolution rect to output pixels: every field times
/// `factor`. Exact — this is nearest, with no blending term anywhere.
UIRect upscaleRect(const UIRect& internal, int factor);

/// True when every field of `outputRect` is a multiple of `factor`, i.e. the
/// rect lands exactly on the upscaled pixel grid with no fractional coverage.
/// A partially-covered output pixel is what "blended" means here, and this is
/// the predicate E-a7.1 asserts for the whole submitted quad list.
bool pixelAligned(const UIRect& outputRect, int factor);

/// A 1 px-thick line at the internal resolution, in internal px. Horizontal
/// lines are `length` wide and 1 tall; vertical ones 1 wide and `length` tall.
/// This is the fixture E-a7.1 scales: it must come out `factor` px thick.
UIRect internalLineQuad(int x, int y, int length, bool horizontal);

// --- camera snap --------------------------------------------------------------

/// Snaps one world axis to an integer pixel by flooring, i.e. rounds toward
/// negative infinity.
///
/// Why floor and not round-to-nearest: ARCHITECTURE_TARGET.md §11 requires the
/// camera at x=10.4 and x=10.6 to produce an identical frame. Round-to-nearest
/// maps those to 10 and 11 — different frames — so it cannot satisfy the spec's
/// own example, while floor maps both to 10. Flooring is also monotone, so
/// panning never oscillates across a boundary, and it is exactly reproducible
/// across hosts, unlike anything built on floating-point comparison.
int snapCameraAxis(double worldPosition);

/// World rect -> internal-resolution rect for a camera already snapped by
/// snapCameraAxis(). Integer arithmetic only; the float stopped at the snap.
UIRect worldToInternal(const UIRect& worldRect, int snappedCameraX, int snappedCameraY);

// --- scene layer order ---------------------------------------------------------

/// The world-scene draw layers, in submission order (ARCHITECTURE_TARGET.md
/// §11). LightPass and PostPass are passes rather than quad layers and join
/// this list with b8a/b8b; UILayer is drawn after the upscale and belongs to
/// the UI layer, not here.
enum class SceneLayer { GroundTiles, GroundDecor, Shadows, World, Overhead, Particles };

/// Stable token for the layer, e.g. "GroundTiles". Never renamed.
const char* toString(SceneLayer layer);

/// One layer's quads, in internal-resolution pixels.
struct SceneLayerDraw {
    SceneLayer layer = SceneLayer::GroundTiles;
    std::vector<UIRect> quads;

    bool operator==(const SceneLayerDraw&) const = default;
};

/// A whole scene submission: the upscale factor and the drawable layers in
/// draw order.
struct SceneSubmission {
    int upscaleFactor = 1;
    std::vector<SceneLayerDraw> layers;

    bool operator==(const SceneSubmission&) const = default;
};

/// Puts the scene in draw order: layers sorted by their enumerator order, each
/// layer appearing exactly once (duplicates merged in the order given), and
/// empty layers dropped so a submission never carries a no-op draw.
std::vector<SceneLayerDraw> orderSceneLayers(const std::vector<SceneLayerDraw>& layers);

/// Per-layer quad counts in draw order — the shape E-a7.2 asserts.
struct SceneLayerCount {
    SceneLayer layer;
    std::size_t quads;

    bool operator==(const SceneLayerCount&) const = default;
};

std::vector<SceneLayerCount> sceneLayerCounts(const SceneSubmission& scene);

/// FNV-1a 64 over the submission: upscale factor, layer count, then per layer
/// its token index and quad count followed by each quad's i32 x, y, w, h.
/// Integers only — the camera's float position never reaches the hash, which is
/// what makes E-a7.3 a structural comparison rather than a float one.
std::uint64_t structuralHash(const SceneSubmission& scene);

}  // namespace ply
