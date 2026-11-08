// Headless unit tests for pure-math engine code (no GL context required).

#include "plyEngine/SpriteAtlas.hpp"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

void near(float a, float b, const char* what) {
    if (std::fabs(a - b) > 1e-6f) {
        std::fprintf(stderr, "  FAIL: %s (%.9f != %.9f)\n", what, a, b);
        ++failures;
    }
}

}  // namespace

int main() {
    // --- makeRegion maps pixel rects to normalized UVs ----------------------
    {
        const ply::TextureRegion full = ply::makeRegion(128, 128, 0, 0, 128, 128);
        near(full.u0, 0.0f, "full u0");
        near(full.v0, 0.0f, "full v0");
        near(full.u1, 1.0f, "full u1");
        near(full.v1, 1.0f, "full v1");
        check(full.width == 128 && full.height == 128, "full size");
    }
    {
        // Top-left cell of a 2x2 grid in a 128x128 atlas.
        const ply::TextureRegion r = ply::makeRegion(128, 128, 0, 0, 64, 64);
        near(r.u0, 0.0f, "tl u0");
        near(r.v0, 0.0f, "tl v0");
        near(r.u1, 0.5f, "tl u1");
        near(r.v1, 0.5f, "tl v1");
    }
    {
        // Bottom-right cell: (u, v) = (0.5, 0.5) .. (1, 1).
        const ply::TextureRegion r = ply::makeRegion(128, 128, 64, 64, 64, 64);
        near(r.u0, 0.5f, "br u0");
        near(r.v0, 0.5f, "br v0");
        near(r.u1, 1.0f, "br u1");
        near(r.v1, 1.0f, "br v1");
    }
    {
        // Non-square atlas must scale u and v independently.
        const ply::TextureRegion r = ply::makeRegion(256, 64, 128, 16, 64, 32);
        near(r.u0, 0.5f, "ns u0");
        near(r.v0, 0.25f, "ns v0");
        near(r.u1, 0.75f, "ns u1");
        near(r.v1, 0.75f, "ns v1");
    }
    {
        // Every in-bounds region must stay inside [0, 1] and be positive-area.
        // x steps by 24 so x + 32 <= 128 for all samples.
        for (int i = 0; i <= 4; ++i) {
            const int x = i * 24;
            const int y = i * 24;
            const ply::TextureRegion r = ply::makeRegion(128, 128, x, y, 32, 32);
            check(r.u0 >= 0.0f && r.u1 <= 1.0f, "region u within [0,1]");
            check(r.v0 >= 0.0f && r.v1 <= 1.0f, "region v within [0,1]");
            check(r.u1 > r.u0 && r.v1 > r.v0, "region positive area");
        }
    }

    if (failures == 0) {
        std::printf("atlas tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "atlas tests: %d failure(s)\n", failures);
    return 1;
}
