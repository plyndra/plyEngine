// Headless render-target and scene-order tests (M1a a7): E-a7.1 (a 1 px line at
// internal res is exactly k px wide at output, with no partially-covered pixel
// anywhere in the submitted quad list), E-a7.2 (the layer sequence and
// per-layer quad counts match the fixture — draw order is data, not code) and
// E-a7.3 (camera x=10.4 and x=10.6 produce an identical structural hash).
//
// No GL context, no window, no wall clock. The camera's float position stops at
// the snap; everything hashed is an integer.

#include "plyEngine/RenderTarget.hpp"
#include "plyEngine/StructuralHash.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>
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

using ply::SceneLayer;
using ply::SceneSubmission;
using ply::UIRect;

// --- integer upscale ---------------------------------------------------------------

void testUpscaleFactor() {
    // ADR 0012: 640x360 internal at 1080p and 1440p.
    eq(ply::integerUpscale(640, 360, 1920, 1080), 3, "3x at 1080p");
    eq(ply::integerUpscale(640, 360, 2560, 1440), 4, "4x at 1440p");

    // The binding axis decides: a wide-but-short window takes the short axis.
    eq(ply::integerUpscale(640, 360, 2560, 800), 2, "the tighter axis decides the factor");
    // A window smaller than the internal res never shrinks below 1:1.
    eq(ply::integerUpscale(640, 360, 320, 180), 1, "smaller than internal still uses 1x");
    eq(ply::integerUpscale(640, 360, 0, 0), 1, "degenerate output is 1x, not 0");
    eq(ply::integerUpscale(640, 360, 1919, 1080), 2, "one pixel short of 3x is 2x");

    // The factor really is the largest that fits, both axes.
    for (int factor = 1; factor <= 4; ++factor) {
        const int outW = 640 * factor;
        const int outH = 360 * factor;
        eq(ply::integerUpscale(640, 360, outW, outH), factor, "exact multiple gives that factor");
        if (factor > 1) {
            eq(ply::integerUpscale(640, 360, outW - 1, outH), factor - 1,
               "one pixel under an exact multiple drops a factor");
        }
    }
}

// --- E-a7.1: 1 px line is exactly k px at output, nothing blended ------------------

long long testNearestUpscale() {
    long long alignedQuads = 0;
    long long checkedQuads = 0;

    for (int factor = 1; factor <= 4; ++factor) {
        // Horizontal 1 px line, 40 px long, at internal (7, 5).
        const UIRect hLine = ply::internalLineQuad(7, 5, 40, true);
        eq(hLine.h, 1, "horizontal line is 1 px thick at internal res");
        eq(hLine.w, 40, "horizontal line keeps its length");

        const UIRect hOut = ply::upscaleRect(hLine, factor);
        eq(hOut.h, factor, "a 1 px line is exactly k px thick at output");
        eq(hOut.w, 40 * factor, "line length scales by k");
        eq(hOut.x, 7 * factor, "line origin scales by k, no half pixel");
        eq(hOut.y, 5 * factor, "line y scales by k, no half pixel");

        // Vertical line, same discipline on the other axis.
        const UIRect vLine = ply::internalLineQuad(3, 9, 24, false);
        eq(vLine.w, 1, "vertical line is 1 px wide at internal res");
        const UIRect vOut = ply::upscaleRect(vLine, factor);
        eq(vOut.w, factor, "a 1 px vertical line is exactly k px wide at output");
        eq(vOut.h, 24 * factor, "vertical line length scales by k");
        eq(vOut.x, 3 * factor, "vertical line x scales by k");

        // Every field is on the upscaled grid: no partial coverage, so no
        // blended pixel can come out of this quad list.
        for (const UIRect& out : {hOut, vOut}) {
            ++checkedQuads;
            if (ply::pixelAligned(out, factor) && out.h > 0 && out.w > 0) ++alignedQuads;
            else check(false, "upscaled line is pixel-aligned and non-empty");
        }

        // A quad that is NOT on the grid is reported as such: this is the
        // predicate's teeth, otherwise "no blended pixels" would be free.
        check(!ply::pixelAligned(UIRect{1, 0, 4, 4}, 3), "an off-grid rect is not pixel-aligned");
        check(!ply::pixelAligned(UIRect{0, 0, 3, 4}, 2), "a non-multiple width is not aligned");
        check(ply::pixelAligned(UIRect{0, 0, 6, 4}, 2), "an on-grid rect is aligned");
    }

    // The area is exactly factor squared: nearest copies, never averages.
    {
        const UIRect one = ply::internalLineQuad(0, 0, 1, true);  // a single pixel
        const UIRect out = ply::upscaleRect(one, 3);
        eq(out.w * out.h, 9, "a 1x1 internal pixel becomes a 3x3 output block");
    }
    return checkedQuads == alignedQuads ? alignedQuads : -1;
}

// --- E-a7.2: layer order and per-layer quad counts ----------------------------------

/// The fixture scene: a few quads per layer, given deliberately out of order and
/// with an empty layer present, so the ordering and the drop-empty rule are both
/// exercised.
std::vector<ply::SceneLayerDraw> fixtureLayers() {
    return {
        {SceneLayer::World, {{100, 200, 16, 32}, {120, 210, 16, 32}}},
        {SceneLayer::GroundTiles, {{0, 0, 16, 16}, {16, 0, 16, 16}, {32, 0, 16, 16}}},
        {SceneLayer::Particles, {{200, 100, 2, 2}}},
        {SceneLayer::Shadows, {{104, 230, 8, 3}}},
        {SceneLayer::Overhead, {}},
        {SceneLayer::GroundDecor, {{0, 8, 4, 4}, {12, 8, 4, 4}, {24, 8, 4, 4}, {36, 8, 4, 4}}},
    };
}

void testLayerOrderAndCounts() {
    const std::vector<ply::SceneLayerDraw> ordered = ply::orderSceneLayers(fixtureLayers());

    // Draw order is the enumerator order, not the input order, and the empty
    // Overhead layer is dropped.
    eq(static_cast<long long>(ordered.size()), 5, "empty layers are dropped");
    if (ordered.size() == 5) {
        const SceneLayer expected[5] = {SceneLayer::GroundTiles, SceneLayer::GroundDecor,
                                        SceneLayer::Shadows, SceneLayer::World,
                                        SceneLayer::Particles};
        const long long expectedQuads[5] = {3, 4, 1, 2, 1};
        for (std::size_t i = 0; i < 5; ++i) {
            check(ordered[i].layer == expected[i],
                  "layers are submitted in the fixed enumerator order");
            eq(static_cast<long long>(ordered[i].quads.size()), expectedQuads[i],
               "per-layer quad count");
        }
        // Ground tiles first, particles last: the fixture is invertible.
        check(ordered.front().layer == SceneLayer::GroundTiles, "ground tiles draw first");
        check(ordered.back().layer == SceneLayer::Particles, "particles draw last");
    }

    // The tokens are stable and distinct.
    eq(static_cast<long long>(std::string_view(ply::toString(SceneLayer::GroundTiles)) ==
                              std::string_view("GroundTiles")
                                  ? 1
                                  : 0),
       1, "token GroundTiles");
    int distinct = 0;
    const SceneLayer all[6] = {SceneLayer::GroundTiles, SceneLayer::GroundDecor,
                               SceneLayer::Shadows,     SceneLayer::World,
                               SceneLayer::Overhead,    SceneLayer::Particles};
    for (std::size_t i = 0; i < 6; ++i) {
        int clashes = 0;
        for (std::size_t j = 0; j < 6; ++j) {
            if (i != j && std::string_view(ply::toString(all[i])) ==
                              std::string_view(ply::toString(all[j])))
                ++clashes;
        }
        if (clashes == 0 && std::string_view(ply::toString(all[i])).size() > 0) ++distinct;
    }
    eq(distinct, 6, "all 6 layer tokens are distinct and non-empty");

    // Duplicates merge, keeping the given order within the layer.
    {
        const std::vector<ply::SceneLayerDraw> two = {
            {SceneLayer::World, {{1, 1, 2, 2}}},
            {SceneLayer::World, {{9, 9, 2, 2}}},
        };
        const std::vector<ply::SceneLayerDraw> merged = ply::orderSceneLayers(two);
        eq(static_cast<long long>(merged.size()), 1, "duplicate layers merge into one");
        if (merged.size() == 1) {
            eq(static_cast<long long>(merged[0].quads.size()), 2, "merged layer keeps both quads");
            eq(merged[0].quads[0].x, 1, "merge preserves the given order");
            eq(merged[0].quads[1].x, 9, "merge preserves the given order");
        }
    }

    // The counts view that E-a7.2 asserts, as its own call.
    SceneSubmission scene;
    scene.upscaleFactor = 3;
    scene.layers = ordered;
    const std::vector<ply::SceneLayerCount> counts = ply::sceneLayerCounts(scene);
    eq(static_cast<long long>(counts.size()), 5, "one count row per submitted layer");
    if (counts.size() == 5) {
        eq(static_cast<long long>(counts[0].quads), 3, "ground tiles: 3 quads");
        eq(static_cast<long long>(counts[1].quads), 4, "ground decor: 4 quads");
        eq(static_cast<long long>(counts[2].quads), 1, "shadows: 1 quad");
        eq(static_cast<long long>(counts[3].quads), 2, "world: 2 quads");
        eq(static_cast<long long>(counts[4].quads), 1, "particles: 1 quad");
        long long total = 0;
        for (const ply::SceneLayerCount& c : counts) total += static_cast<long long>(c.quads);
        eq(total, 11, "the fixture scene totals 11 quads");
    }
}

// --- E-a7.3: camera snap --------------------------------------------------------------

/// The scene as it is submitted for a camera at (worldX, worldY): world-space
/// quads offset by the snapped camera, then reused unchanged.
SceneSubmission sceneAtCamera(double worldX, double worldY) {
    const int camX = ply::snapCameraAxis(worldX);
    const int camY = ply::snapCameraAxis(worldY);

    std::vector<ply::SceneLayerDraw> layers;
    for (const ply::SceneLayerDraw& layer : fixtureLayers()) {
        ply::SceneLayerDraw shifted;
        shifted.layer = layer.layer;
        for (const UIRect& quad : layer.quads) {
            shifted.quads.push_back(ply::worldToInternal(quad, camX, camY));
        }
        layers.push_back(shifted);
    }

    SceneSubmission scene;
    scene.upscaleFactor = ply::integerUpscale(640, 360, 1920, 1080);
    scene.layers = ply::orderSceneLayers(layers);
    return scene;
}

void testCameraSnap() {
    // The spec's own example: 10.4 and 10.6 must be the same frame.
    eq(ply::snapCameraAxis(10.4), 10, "10.4 snaps to 10");
    eq(ply::snapCameraAxis(10.6), 10, "10.6 snaps to 10 too — that is the requirement");
    // Which is exactly what round-to-nearest would NOT do; pinned so the
    // distinction cannot be lost later.
    check(ply::snapCameraAxis(10.4) == ply::snapCameraAxis(10.6),
          "the spec's example cannot be satisfied by rounding to nearest");

    // Floor, not truncation: negatives go down, not toward zero.
    eq(ply::snapCameraAxis(-0.5), -1, "-0.5 floors to -1, not 0");
    eq(ply::snapCameraAxis(-10.4), -11, "-10.4 floors to -11, not -10");
    eq(ply::snapCameraAxis(3.0), 3, "an exact integer is unchanged");
    eq(ply::snapCameraAxis(0.0), 0, "zero is unchanged");

    // The hash is identical for 10.4 and 10.6 ...
    const SceneSubmission a = sceneAtCamera(10.4, 20.4);
    const SceneSubmission b = sceneAtCamera(10.6, 20.6);
    eqHash(ply::structuralHash(a), ply::structuralHash(b),
           "camera 10.4 vs 10.6 gives an identical structural hash");

    // ... and different for a genuinely different position.
    const SceneSubmission c = sceneAtCamera(11.4, 20.4);
    check(ply::structuralHash(a) != ply::structuralHash(c),
          "a one-pixel camera move does change the hash");
    const SceneSubmission d = sceneAtCamera(10.4, 21.4);
    check(ply::structuralHash(a) != ply::structuralHash(d),
          "the vertical axis is covered too");

    // Recomputed from scratch, not the same value compared to itself.
    eqHash(ply::structuralHash(sceneAtCamera(10.4, 20.4)), ply::structuralHash(a),
           "scene hash is reproducible across recomputation");

    // The hash covers the upscale factor: the same scene at a different factor
    // is a different submission.
    {
        SceneSubmission e = a;
        e.upscaleFactor = 4;
        check(ply::structuralHash(e) != ply::structuralHash(a),
              "the upscale factor is part of the hash");
    }
    // And the layer order: swapping two layers changes it.
    {
        SceneSubmission f = a;
        if (f.layers.size() >= 2) std::swap(f.layers[0], f.layers[1]);
        check(ply::structuralHash(f) != ply::structuralHash(a), "layer order is part of the hash");
    }

    // Translating by the snapped camera is exact: world (10.4-based) quads land
    // on the internal grid with no fractional offset.
    eqRect(ply::worldToInternal(UIRect{110, 220, 16, 32}, 10, 20), UIRect{100, 200, 16, 32},
           "world->internal subtracts the snapped camera");
}

// --- render target plan -----------------------------------------------------------

void testRenderTargetPlan() {
    // Full-bleed: 640x360 into 1920x1080 at factor 3 covers the window exactly.
    {
        const ply::RenderTargetPlan plan = ply::makeRenderTargetPlan(640, 360, 1920, 1080);
        eq(plan.factor, 3, "plan factor at 1080p");
        eqRect(plan.destination, UIRect{0, 0, 1920, 1080}, "full-bleed destination");
        eq(plan.internalWidth, 640, "plan keeps the internal width");
        eq(plan.outputWidth, 1920, "plan keeps the output width");
    }
    // Odd window: 1919x1080 at factor 2 shows 1280x720, centred.
    {
        const ply::RenderTargetPlan plan = ply::makeRenderTargetPlan(640, 360, 1919, 1080);
        eq(plan.factor, 2, "odd window takes the floor factor");
        eqRect(plan.destination, UIRect{(1919 - 1280) / 2, (1080 - 720) / 2, 1280, 720},
               "odd window destination is centred");
        eq(plan.destination.x, 319, "the odd remainder splits: 639/2 floors to 319");
        // Centred means the leftover pixels on both sides are equal or differ by
        // one, never all piled on one side.
        const int left = plan.destination.x;
        const int right = 1919 - (plan.destination.x + plan.destination.w);
        check(right - left == 0 || right - left == 1, "leftover pixels split evenly");
    }
    // The destination is always exactly internal*factor.
    for (int factor = 1; factor <= 4; ++factor) {
        const ply::RenderTargetPlan plan =
            ply::makeRenderTargetPlan(640, 360, 640 * factor, 360 * factor);
        eq(plan.destination.w, 640 * factor, "destination width is internal * factor");
        eq(plan.destination.h, 360 * factor, "destination height is internal * factor");
        check(ply::pixelAligned(plan.destination, plan.factor),
              "the destination is on the output pixel grid");
    }
}

// --- the headless SceneRenderer ----------------------------------------------------

void testSceneRenderer() {
    // Draw order is data, not call order: submit the layers back to front and
    // the submission still comes out in the fixed order.
    ply::SceneRenderer renderer;
    renderer.submit(SceneLayer::Particles, UIRect{200, 100, 2, 2});
    renderer.submit(SceneLayer::World, UIRect{100, 200, 16, 32});
    renderer.submit(SceneLayer::GroundTiles, UIRect{0, 0, 16, 16});
    renderer.submitLayer(SceneLayer::GroundTiles, {{16, 0, 16, 16}, {32, 0, 16, 16}});
    renderer.submit(SceneLayer::World, UIRect{120, 210, 16, 32});

    const SceneSubmission scene = renderer.submission(3);
    eq(scene.upscaleFactor, 3, "submission carries the factor");
    eq(static_cast<long long>(scene.layers.size()), 3, "only submitted layers appear");
    if (scene.layers.size() == 3) {
        check(scene.layers[0].layer == SceneLayer::GroundTiles, "tiles first despite last submits");
        check(scene.layers[1].layer == SceneLayer::World, "world in the middle");
        check(scene.layers[2].layer == SceneLayer::Particles, "particles last");
        eq(static_cast<long long>(scene.layers[0].quads.size()), 3, "three tile quads accumulated");
        eq(static_cast<long long>(scene.layers[1].quads.size()), 2, "two world quads accumulated");
    }

    // Accumulating in submission order keeps each layer's own order stable.
    if (scene.layers.size() == 3) {
        eq(scene.layers[1].quads[0].x, 100, "world quads keep submission order");
        eq(scene.layers[1].quads[1].x, 120, "world quads keep submission order");
    }

    // reset() clears everything.
    ply::SceneRenderer reused;
    reused.submit(SceneLayer::Shadows, UIRect{1, 2, 3, 4});
    reused.reset();
    reused.submit(SceneLayer::Overhead, UIRect{5, 6, 7, 8});
    const SceneSubmission afterReset = reused.submission(1);
    eq(static_cast<long long>(afterReset.layers.size()), 1, "reset drops prior layers");
    if (afterReset.layers.size() == 1) {
        check(afterReset.layers[0].layer == SceneLayer::Overhead, "only the post-reset layer remains");
    }

    // The renderer and the direct ordering path produce the identical
    // submission for the same quads: one ordering implementation, not two.
    {
        ply::SceneRenderer fromRenderer;
        for (const ply::SceneLayerDraw& layer : fixtureLayers()) {
            fromRenderer.submitLayer(layer.layer, layer.quads);
        }
        const SceneSubmission viaRenderer = fromRenderer.submission(3);

        SceneSubmission viaFunction;
        viaFunction.upscaleFactor = 3;
        viaFunction.layers = ply::orderSceneLayers(fixtureLayers());

        eqHash(ply::structuralHash(viaRenderer), ply::structuralHash(viaFunction),
               "SceneRenderer emits the same submission as orderSceneLayers");
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence(long long alignedQuads) {
    const UIRect hLine = ply::internalLineQuad(7, 5, 40, true);
    const UIRect k3 = ply::upscaleRect(hLine, 3);

    const std::vector<ply::SceneLayerDraw> ordered = ply::orderSceneLayers(fixtureLayers());
    SceneSubmission scene;
    scene.upscaleFactor = ply::integerUpscale(640, 360, 1920, 1080);
    scene.layers = ordered;
    const std::vector<ply::SceneLayerCount> counts = ply::sceneLayerCounts(scene);

    const std::uint64_t hashA = ply::structuralHash(sceneAtCamera(10.4, 20.4));
    const std::uint64_t hashB = ply::structuralHash(sceneAtCamera(10.6, 20.6));
    const std::uint64_t hashC = ply::structuralHash(sceneAtCamera(11.4, 20.4));

    std::printf(
        "EVIDENCE render_target factor=%d/%d/%d line1px_1x=%dx%d line1px_3x=%dx%d "
        "aligned=%lld dest=%d,%d,%d,%d layers=%zu counts=%zu/%zu/%zu/%zu/%zu total=%zu "
        "snap=%.1f->%d/%.1f->%d hash_same=%s hash_moved=%s\n",
        ply::integerUpscale(640, 360, 1920, 1080), ply::integerUpscale(640, 360, 2560, 1440),
        ply::integerUpscale(640, 360, 2560, 800), hLine.w, hLine.h, k3.w, k3.h, alignedQuads,
        ply::makeRenderTargetPlan(640, 360, 1919, 1080).destination.x,
        ply::makeRenderTargetPlan(640, 360, 1919, 1080).destination.y,
        ply::makeRenderTargetPlan(640, 360, 1919, 1080).destination.w,
        ply::makeRenderTargetPlan(640, 360, 1919, 1080).destination.h, ordered.size(),
        counts[0].quads, counts[1].quads, counts[2].quads, counts[3].quads, counts[4].quads,
        counts[0].quads + counts[1].quads + counts[2].quads + counts[3].quads + counts[4].quads,
        10.4, ply::snapCameraAxis(10.4), 10.6, ply::snapCameraAxis(10.6),
        hashA == hashB ? "match" : "differ", hashA != hashC ? "differ" : "same");
}

}  // namespace

int main() {
    testUpscaleFactor();
    const long long alignedQuads = testNearestUpscale();
    testRenderTargetPlan();
    testSceneRenderer();
    testLayerOrderAndCounts();
    testCameraSnap();

    if (failures == 0) {
        if (alignedQuads > 0 && ply::orderSceneLayers(fixtureLayers()).size() >= 5) {
            printEvidence(alignedQuads);
        } else {
            std::fprintf(stderr, "  FAIL: evidence preconditions\n");
            return 1;
        }
        std::printf("render_target tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "render_target tests: %d failure(s)\n", failures);
    return 1;
}
