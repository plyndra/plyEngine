// GL present half of a7 (M1a gate): FBO + texture at the plan's internal
// resolution, scene layers drawn in a7's order, and the present blit at
// plan.destination — verified through a structural readback under SDL's
// offscreen video driver, not a pixel golden in CI (evals/2.0.md anti-pattern
// list; ADR 0014 keeps committed pixel goldens container-pinned and opt-in).
//
// The readback is structural in the same sense as the a7 line-width grader:
// the expected image is *computed from the pattern data* at run time — every
// output pixel must equal its block's internal texel exactly (nearest upscale
// produces exact k×k blocks), the presented rect must equal
// makeRenderTargetPlan's destination, the layer overlaps must resolve in a7's
// order, and the set of colours inside the destination must be exactly the
// pattern's set — a linear or fractional upscale produces mixtures and fails.
// Nothing committed here depends on llvmpipe's bytes; a conformant GL passes.

#include "plyEngine/App.hpp"
#include "plyEngine/PresentPass.hpp"
#include "plyEngine/RenderTarget.hpp"
#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

#include <cstdio>
#include <cstdlib>
#include <set>
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
        std::fprintf(stderr, "  FAIL: %s (got {%d,%d,%d,%d}, want {%d,%d,%d,%d})\n", what,
                     got.x, got.y, got.w, got.h, want.x, want.y, want.w, want.h);
        ++failures;
    }
}

// --- the pattern -------------------------------------------------------------

// Internal resolution for every case (the plan policy is generic; 64x48 keeps
// the readback small while exercising all three factor regimes).
constexpr int kIW = 64;
constexpr int kIH = 48;

// The layer pattern, in internal px, in a7's draw order: GroundTiles lays a
// 1 px two-colour checker over the whole surface, World lays one solid quad
// over it, Overhead lays a smaller solid quad over that. The overlap is how
// "layers through a7's order" becomes observable: later layers must win.
constexpr ply::UIRect kWorldQuad{12, 10, 36, 28};
constexpr ply::UIRect kOverheadQuad{26, 18, 12, 10};

struct Color {
    int r, g, b;
    bool operator<(const Color& o) const {
        return r != o.r ? r < o.r : g != o.g ? g < o.g : b < o.b;
    }
    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b; }
};

const Color kChecker0{200, 60, 60};
const Color kChecker1{60, 200, 120};
const Color kWorld{70, 110, 230};
const Color kOverhead{235, 185, 70};
const Color kLetterbox{16, 18, 28};
// What a linear filter would put between the two checker colours — a colour
// the pattern never contains, so its absence is the "no blended pixel" proof.
const Color kBlendWouldBe{(kChecker0.r + kChecker1.r) / 2, (kChecker0.g + kChecker1.g) / 2,
                          (kChecker0.b + kChecker1.b) / 2};

/// The internal image the layers must produce, top-left origin.
Color expectedInternal(int x, int y) {
    if (x >= kOverheadQuad.x && x < kOverheadQuad.x + kOverheadQuad.w && y >= kOverheadQuad.y &&
        y < kOverheadQuad.y + kOverheadQuad.h) {
        return kOverhead;
    }
    if (x >= kWorldQuad.x && x < kWorldQuad.x + kWorldQuad.w && y >= kWorldQuad.y &&
        y < kWorldQuad.y + kWorldQuad.h) {
        return kWorld;
    }
    return (x + y) % 2 ? kChecker1 : kChecker0;
}

}  // namespace

int main() {
    // The offscreen driver needs no display: this is what makes the GL half
    // verifiable on a headless host (measured 2026-11-11, see evals/2.0.log).
#ifdef _WIN32
    _putenv_s("SDL_VIDEODRIVER", "offscreen");
#else
    ::setenv("SDL_VIDEODRIVER", "offscreen", 1);
#endif

    long long compared = 0;
    long long blended = 0;
    int casesRun = 0;
    bool fboComplete = false;
    std::size_t colorsSeen = 0;

    const struct {
        int outW, outH;
        const char* what;
    } cases[3] = {
        {256, 192, "factor 4, full-bleed destination"},
        {208, 156, "factor 3, centred letterbox destination"},
        {48, 36, "factor 1, window smaller than the internal surface"},
    };

    for (const auto& c : cases) {
        ply::App::Config config;
        config.window.title = "present_gl";
        config.window.width = c.outW;
        config.window.height = c.outH;
        config.window.resizable = false;
        ply::App app(config);
        if (!app.valid()) {
            // No GL context here (e.g. a runner without EGL): skip loudly
            // rather than pass vacuously. CI runs this suite explicitly on
            // Linux with software GL; elsewhere the skip is the honest state.
            std::printf("present_gl: SKIP — no GL context under SDL_VIDEODRIVER=offscreen "
                        "(%s)\n",
                        app.error());
            return 77;
        }

        const ply::RenderTargetPlan plan =
            ply::makeRenderTargetPlan(kIW, kIH, c.outW, c.outH);
        eqRect(plan.destination, ply::makeRenderTargetPlan(kIW, kIH, c.outW, c.outH).destination,
               "the plan is a7's, called not copied");

        // Solid 8x8 cells: checker0, checker1, world, overhead.
        std::vector<unsigned char> pixels(32 * 8 * 4, 0);
        const Color cells[4] = {kChecker0, kChecker1, kWorld, kOverhead};
        for (int i = 0; i < 4; ++i) {
            for (int y = 0; y < 8; ++y) {
                for (int x = 0; x < 8; ++x) {
                    const std::size_t idx =
                        (static_cast<std::size_t>(y) * 32 + i * 8 + x) * 4;
                    pixels[idx + 0] = static_cast<unsigned char>(cells[i].r);
                    pixels[idx + 1] = static_cast<unsigned char>(cells[i].g);
                    pixels[idx + 2] = static_cast<unsigned char>(cells[i].b);
                    pixels[idx + 3] = 255;
                }
            }
        }
        ply::SpriteAtlas atlas(32, 8, pixels.data(), ply::TextureFilter::Nearest);
        atlas.addRegion("checker0", 0, 0, 8, 8);
        atlas.addRegion("checker1", 8, 0, 8, 8);
        atlas.addRegion("world", 16, 0, 8, 8);
        atlas.addRegion("overhead", 24, 0, 8, 8);

        ply::PresentPass pass(plan);
        fboComplete = pass.framebuffer() != 0;
        check(fboComplete, "the FBO allocated");
        if (fboComplete) {
            glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffer());
            fboComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        eq(pass.internalWidth(), kIW, "internal width");
        eq(pass.internalHeight(), kIH, "internal height");

        // setPlan with the same internal size must not churn the surface.
        const unsigned fboBefore = pass.framebuffer();
        const unsigned texBefore = pass.texture();
        pass.setPlan(plan);
        eq(pass.framebuffer() == fboBefore && pass.texture() == texBefore, 1,
           "setPlan with an unchanged internal size keeps the surface");

        // The texture is the plan's internal size, exactly.
        glBindTexture(GL_TEXTURE_2D, pass.texture());
        int texW = 0, texH = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
        glBindTexture(GL_TEXTURE_2D, 0);
        eq(texW, kIW, "texture width == internal width");
        eq(texH, kIH, "texture height == internal height");

        // Draw the layers in a7's order into the FBO...
        pass.begin({0.0f, 0.0f, 0.0f, 1.0f});
        {
            ply::SpriteBatch batch(8192);
            const glm::mat4 ortho =
                glm::ortho(0.0f, static_cast<float>(kIW), static_cast<float>(kIH), 0.0f);
            const ply::TextureRegion r0 = atlas.region("checker0");
            const ply::TextureRegion r1 = atlas.region("checker1");
            const ply::TextureRegion r2 = atlas.region("world");
            const ply::TextureRegion r3 = atlas.region("overhead");
            batch.begin(atlas.texture(), ortho);
            for (int y = 0; y < kIH; ++y) {
                for (int x = 0; x < kIW; ++x) {
                    batch.draw(glm::vec2{x + 0.5f, y + 0.5f}, glm::vec2{1.0f, 1.0f},
                               (x + y) % 2 ? r1 : r0);
                }
            }
            batch.end();
            batch.begin(atlas.texture(), ortho);
            batch.draw(glm::vec2{kWorldQuad.x + kWorldQuad.w / 2.0f,
                                 kWorldQuad.y + kWorldQuad.h / 2.0f},
                       glm::vec2{kWorldQuad.w, kWorldQuad.h}, r2);
            batch.end();
            batch.begin(atlas.texture(), ortho);
            batch.draw(glm::vec2{kOverheadQuad.x + kOverheadQuad.w / 2.0f,
                                 kOverheadQuad.y + kOverheadQuad.h / 2.0f},
                       glm::vec2{kOverheadQuad.w, kOverheadQuad.h}, r3);
            batch.end();
        }

        // ...read the internal surface back (FBO still bound; GL rows run
        // bottom-up, the pattern is top-left)...
        std::vector<unsigned char> internalRB(static_cast<std::size_t>(kIW) * kIH * 4);
        glReadPixels(0, 0, kIW, kIH, GL_RGBA, GL_UNSIGNED_BYTE, internalRB.data());
        long long internalMismatch = 0;
        for (int y = 0; y < kIH; ++y) {
            for (int x = 0; x < kIW; ++x) {
                const std::size_t idx =
                    (static_cast<std::size_t>(kIH - 1 - y) * kIW + x) * 4;
                const Color got{internalRB[idx], internalRB[idx + 1], internalRB[idx + 2]};
                if (!(got == expectedInternal(x, y))) {
                    ++internalMismatch;
                }
            }
        }
        eq(internalMismatch, 0,
           "internal surface matches the pattern; overlap resolves in a7's order");
        pass.end();

        // ...and present, then read the window back and require the exact
        // structural reproduction.
        pass.present(plan, {kLetterbox.r / 255.0f, kLetterbox.g / 255.0f,
                            kLetterbox.b / 255.0f, 1.0f});
        std::vector<unsigned char> windowRB(static_cast<std::size_t>(c.outW) * c.outH * 4);
        glReadPixels(0, 0, c.outW, c.outH, GL_RGBA, GL_UNSIGNED_BYTE, windowRB.data());

        std::set<Color> inside;
        std::set<Color> outside;
        long long caseMismatch = 0;
        for (int y = 0; y < c.outH; ++y) {
            for (int x = 0; x < c.outW; ++x) {
                const std::size_t idx =
                    (static_cast<std::size_t>(c.outH - 1 - y) * c.outW + x) * 4;
                const Color got{windowRB[idx], windowRB[idx + 1], windowRB[idx + 2]};
                // The destination is top-left origin; GL rows run bottom-up.
                const bool inDest = x >= plan.destination.x &&
                                    x < plan.destination.x + plan.destination.w &&
                                    y >= plan.destination.y &&
                                    y < plan.destination.y + plan.destination.h;
                if (!inDest) {
                    outside.insert(got);
                    if (!(got == kLetterbox)) {
                        ++caseMismatch;
                    }
                    continue;
                }
                inside.insert(got);
                // Exact k x k blocks: this output pixel must be a copy of the
                // one internal texel its block covers.
                const int sx = (x - plan.destination.x) / plan.factor;
                const int sy = (y - plan.destination.y) / plan.factor;
                if (!(got == expectedInternal(sx, sy))) {
                    ++caseMismatch;
                }
            }
        }
        compared += static_cast<long long>(c.outW) * c.outH;
        blended += caseMismatch;
        eq(caseMismatch, 0, c.what);
        eq(static_cast<long long>(inside.size()), 4,
           "exactly the pattern's four colours inside the destination (a linear "
           "upscale would add the checker mixtures)");
        check(inside.find(kBlendWouldBe) == inside.end(),
              "the linear-filter mixture colour never appears (no blended pixel)");
        if (c.outW == 208) {
            eq(static_cast<long long>(outside.size()), 1,
               "outside a letterboxed destination there is only letterbox colour");
        }

        ++casesRun;
        if (casesRun == 1) {
            colorsSeen = inside.size();
        }
    }

    // setPlan must reallocate when the internal size actually changes.
    {
        ply::App::Config config;
        config.window.title = "present_gl";
        config.window.width = 128;
        config.window.height = 96;
        config.window.resizable = false;
        ply::App app(config);
        if (app.valid()) {
            ply::PresentPass pass(ply::makeRenderTargetPlan(kIW, kIH, 128, 96));
            const unsigned oldTex = pass.texture();
            pass.setPlan(ply::makeRenderTargetPlan(32, 24, 128, 96));
            eq(pass.internalWidth(), 32, "setPlan re-allocates at the new internal width");
            eq(pass.internalHeight(), 24, "setPlan re-allocates at the new internal height");
            // The name may legally be recycled after deletion; what proves the
            // re-allocation is the new surface's own dimensions.
            glBindTexture(GL_TEXTURE_2D, pass.texture());
            int texW = 0, texH = 0;
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
            glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
            glBindTexture(GL_TEXTURE_2D, 0);
            eq(texW, 32, "the resized texture is 32 wide");
            eq(texH, 24, "the resized texture is 24 tall");
            (void)oldTex;
            ++casesRun;
        }
    }

    if (failures != 0) {
        std::fprintf(stderr, "present_gl tests: %d failure(s)\n", failures);
        return 1;
    }
    std::printf(
        "EVIDENCE present_gl cases=%d fbo=%s internal=%dx%d factor=4/3/1 "
        "dest=0,0,256,192|8,6,192,144|-8,-6,64,48 compared=%lld blended=%lld colors=%zu\n",
        casesRun, fboComplete ? "complete" : "incomplete", kIW, kIH, compared, blended,
        colorsSeen);
    std::printf("present_gl tests: PASS\n");
    return 0;
}
