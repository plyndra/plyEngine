#include "plyEngine/RenderTarget.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <cmath>

namespace ply {

int integerUpscale(int internalWidth, int internalHeight, int outputWidth, int outputHeight) {
    if (internalWidth <= 0 || internalHeight <= 0 || outputWidth <= 0 || outputHeight <= 0) return 1;
    const int byWidth = outputWidth / internalWidth;
    const int byHeight = outputHeight / internalHeight;
    const int factor = byWidth < byHeight ? byWidth : byHeight;
    return factor < 1 ? 1 : factor;
}

UIRect upscaleRect(const UIRect& internal, int factor) {
    const int k = factor < 1 ? 1 : factor;
    return UIRect{internal.x * k, internal.y * k, internal.w * k, internal.h * k};
}

bool pixelAligned(const UIRect& outputRect, int factor) {
    const int k = factor < 1 ? 1 : factor;
    return outputRect.x % k == 0 && outputRect.y % k == 0 && outputRect.w % k == 0 &&
           outputRect.h % k == 0;
}

UIRect internalLineQuad(int x, int y, int length, bool horizontal) {
    return horizontal ? UIRect{x, y, length, 1} : UIRect{x, y, 1, length};
}

RenderTargetPlan makeRenderTargetPlan(int internalWidth, int internalHeight, int outputWidth,
                                      int outputHeight) {
    RenderTargetPlan plan;
    plan.internalWidth = internalWidth;
    plan.internalHeight = internalHeight;
    plan.outputWidth = outputWidth;
    plan.outputHeight = outputHeight;
    plan.factor = integerUpscale(internalWidth, internalHeight, outputWidth, outputHeight);

    const int shownWidth = internalWidth * plan.factor;
    const int shownHeight = internalHeight * plan.factor;
    plan.destination = UIRect{(outputWidth - shownWidth) / 2, (outputHeight - shownHeight) / 2,
                              shownWidth, shownHeight};
    return plan;
}

int snapCameraAxis(double worldPosition) {
    // Floor, not round: ARCHITECTURE_TARGET.md §11 requires 10.4 and 10.6 to
    // render identically, and round-to-nearest would split them into 10 and 11.
    // std::floor is specified exactly, so the result does not depend on the
    // host's floating-point mode beyond the double itself.
    return static_cast<int>(std::floor(worldPosition));
}

UIRect worldToInternal(const UIRect& worldRect, int snappedCameraX, int snappedCameraY) {
    return UIRect{worldRect.x - snappedCameraX, worldRect.y - snappedCameraY, worldRect.w,
                  worldRect.h};
}

const char* toString(SceneLayer layer) {
    switch (layer) {
        case SceneLayer::GroundTiles: return "GroundTiles";
        case SceneLayer::GroundDecor: return "GroundDecor";
        case SceneLayer::Shadows: return "Shadows";
        case SceneLayer::World: return "World";
        case SceneLayer::Overhead: return "Overhead";
        case SceneLayer::Particles: return "Particles";
    }
    return "";  // unreachable; every enumerator is handled
}

std::vector<SceneLayerDraw> orderSceneLayers(const std::vector<SceneLayerDraw>& layers) {
    // One slot per enumerator, so the output order is the enumerator order by
    // construction rather than by a sort whose stability would matter.
    constexpr std::size_t kLayerCount = 6;
    std::vector<SceneLayerDraw> buckets(kLayerCount);
    const SceneLayer order[kLayerCount] = {SceneLayer::GroundTiles, SceneLayer::GroundDecor,
                                           SceneLayer::Shadows,     SceneLayer::World,
                                           SceneLayer::Overhead,    SceneLayer::Particles};
    for (std::size_t i = 0; i < kLayerCount; ++i) buckets[i].layer = order[i];

    for (const SceneLayerDraw& layer : layers) {
        const std::size_t index = static_cast<std::size_t>(layer.layer);
        if (index >= kLayerCount) continue;
        // Duplicates merge in the order given.
        buckets[index].quads.insert(buckets[index].quads.end(), layer.quads.begin(),
                                    layer.quads.end());
    }

    std::vector<SceneLayerDraw> out;
    out.reserve(kLayerCount);
    for (SceneLayerDraw& bucket : buckets) {
        if (!bucket.quads.empty()) out.push_back(std::move(bucket));  // empty layers are no-ops
    }
    return out;
}

std::vector<SceneLayerCount> sceneLayerCounts(const SceneSubmission& scene) {
    std::vector<SceneLayerCount> counts;
    counts.reserve(scene.layers.size());
    for (const SceneLayerDraw& layer : scene.layers) {
        counts.push_back(SceneLayerCount{layer.layer, layer.quads.size()});
    }
    return counts;
}

std::uint64_t structuralHash(const SceneSubmission& scene) {
    StructuralHasher hasher;
    hasher.i32(scene.upscaleFactor);
    hasher.u64(scene.layers.size());
    for (const SceneLayerDraw& layer : scene.layers) {
        hasher.i32(static_cast<std::int32_t>(layer.layer));
        hasher.u64(layer.quads.size());
        for (const UIRect& quad : layer.quads) {
            hasher.i32(quad.x);
            hasher.i32(quad.y);
            hasher.i32(quad.w);
            hasher.i32(quad.h);
        }
    }
    return hasher.value();
}

void SceneRenderer::submit(SceneLayer layer, const UIRect& quad) {
    for (SceneLayerDraw& existing : layers_) {
        if (existing.layer == layer) {
            existing.quads.push_back(quad);
            return;
        }
    }
    SceneLayerDraw draw;
    draw.layer = layer;
    draw.quads.push_back(quad);
    layers_.push_back(std::move(draw));
}

void SceneRenderer::submitLayer(SceneLayer layer, const std::vector<UIRect>& quads) {
    for (const UIRect& quad : quads) submit(layer, quad);
}

SceneSubmission SceneRenderer::submission(int upscaleFactor) const {
    SceneSubmission scene;
    scene.upscaleFactor = upscaleFactor;
    scene.layers = orderSceneLayers(layers_);
    return scene;
}

}  // namespace ply
