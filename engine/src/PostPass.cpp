// PostPass (M1b b8b): the vignette and optional palette LUT. The contract
// is documented in PostPass.hpp; this file is that contract, implemented in
// exact integer arithmetic.
#include "plyEngine/PostPass.hpp"

namespace ply {

PostPass::PostPass(const Config& config) : config_(config) {}

bool PostPass::vignetteEnabled() const { return config_.vignette; }

int PostPass::vignetteFactorAt(int x, int y) const {
    if (!config_.vignette) return 255;  // off: nothing darkens
    const long long cx = config_.frameWidth / 2;
    const long long cy = config_.frameHeight / 2;
    const long long dx = static_cast<long long>(x) - cx;
    const long long dy = static_cast<long long>(y) - cy;
    const long long d2 = dx * dx + dy * dy;
    const long long maxd2 = cx * cx + cy * cy;
    if (maxd2 <= 0) return 255;
    const long long strength =
        config_.vignetteStrength < 0
            ? 0
            : (config_.vignetteStrength > 255 ? 255 : config_.vignetteStrength);
    // factor = 255 - strength * d² / maxd² (truncation toward zero).
    return static_cast<int>(255 - strength * d2 / maxd2);
}

std::uint64_t PostPass::hash() const {
    StructuralHasher hasher;
    hasher.byte(config_.vignette ? 1 : 0);
    hasher.i32(config_.vignetteStrength);
    hasher.i32(config_.frameWidth);
    hasher.i32(config_.frameHeight);
    // The factor field at the five named coordinates (corners + centre).
    const int xs[5] = {0, config_.frameWidth - 1, 0, config_.frameWidth - 1,
                       config_.frameWidth / 2};
    const int ys[5] = {0, 0, config_.frameHeight - 1, config_.frameHeight - 1,
                       config_.frameHeight / 2};
    for (int i = 0; i < 5; ++i) hasher.i32(vignetteFactorAt(xs[i], ys[i]));
    hasher.u64(config_.paletteLut.size());
    for (int value : config_.paletteLut) hasher.i32(value);
    return hasher.value();
}

}  // namespace ply
