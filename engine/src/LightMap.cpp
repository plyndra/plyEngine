// LightMap (M1b b8a): the per-map light field from (clock, lights). The
// contract — the two-keypoint schedule, the additive pool falloff, the
// clamp, the structural hash — is documented in LightMap.hpp; this file is
// that contract, implemented in exact integer arithmetic.
#include "plyEngine/LightMap.hpp"

namespace ply {

namespace {
/// a + (b-a)*k/span with C++ truncation toward zero (documented).
int lerp(int a, int b, long long k, long long span) {
    return a + static_cast<int>((static_cast<long long>(b) - a) * k / span);
}

int clamp255(long long value) {
    return value < 0 ? 0 : (value > 255 ? 255 : static_cast<int>(value));
}
}  // namespace

LightMap::LightMap(const Config& config) : config_(config) {}

LightTint LightMap::ambientAt(long long step) const {
    const long long day = config_.stepsPerDay > 0 ? config_.stepsPerDay : 1;
    const long long phase = ((step % day) + day) % day;  // exact for any clock
    const long long half = day / 2 > 0 ? day / 2 : 1;
    LightTint tint;
    if (phase <= half) {
        // midnight -> noon over the first half of the day.
        tint.r = lerp(config_.midnightTint.r, config_.noonTint.r, phase, half);
        tint.g = lerp(config_.midnightTint.g, config_.noonTint.g, phase, half);
        tint.b = lerp(config_.midnightTint.b, config_.noonTint.b, phase, half);
    } else {
        // noon -> midnight over the second half (truncation toward zero).
        const long long k = phase - half;
        const long long span = day - half > 0 ? day - half : 1;
        tint.r = lerp(config_.noonTint.r, config_.midnightTint.r, k, span);
        tint.g = lerp(config_.noonTint.g, config_.midnightTint.g, k, span);
        tint.b = lerp(config_.noonTint.b, config_.midnightTint.b, k, span);
    }
    return tint;
}

LightTint LightMap::sampleAt(int x, int y, long long step) const {
    long long r = ambientAt(step).r;
    long long g = ambientAt(step).g;
    long long b = ambientAt(step).b;
    for (const LightSprite& light : config_.lights) {
        const long long dx = static_cast<long long>(x) - light.x;
        const long long dy = static_cast<long long>(y) - light.y;
        const long long d2 = dx * dx + dy * dy;
        const long long radius2 =
            static_cast<long long>(light.radius) * static_cast<long long>(light.radius);
        if (radius2 <= 0 || d2 >= radius2) continue;  // beyond the pool: nothing
        // Integer falloff: intensity * (r² - d²) / r² — no sqrt anywhere.
        const long long add = static_cast<long long>(light.intensity) * (radius2 - d2) / radius2;
        r += add;
        g += add;
        b += add;
    }
    return {clamp255(r), clamp255(g), clamp255(b)};
}

std::uint64_t LightMap::hashAt(long long step) const {
    StructuralHasher hasher;
    hasher.u64(static_cast<std::uint64_t>(step));
    hasher.u64(config_.lights.size());
    for (const LightSprite& light : config_.lights) {
        hasher.i32(light.x);
        hasher.i32(light.y);
        hasher.i32(light.radius);
        hasher.i32(light.intensity);
    }
    // The ADR 0014 rule: light sample values at NAMED coordinates — the
    // map's four corners and centre.
    const int xs[5] = {0, config_.mapWidth - 1, 0, config_.mapWidth - 1, config_.mapWidth / 2};
    const int ys[5] = {0, 0, config_.mapHeight - 1, config_.mapHeight - 1, config_.mapHeight / 2};
    for (int i = 0; i < 5; ++i) {
        const LightTint tint = sampleAt(xs[i], ys[i], step);
        hasher.i32(tint.r);
        hasher.i32(tint.g);
        hasher.i32(tint.b);
    }
    return hasher.value();
}

}  // namespace ply
