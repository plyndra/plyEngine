#pragma once

#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <vector>

namespace ply {

// LightMap (M1b b8a, ARCHITECTURE_TARGET.md: "LightPass — ambient tint ×
// light map (multiply)" and the vision row's "Per-map ambient tint,
// additive light pools"): the per-map light field from (clock, lights) —
// E-b8.1: "structural light hash from (clock, lights); noon vs cave
// fixtures differ". Pure integer data: no GL, no window, no float.
//
// Model (documented once, pinned literally):
// - The AMBIENT tint is a two-keypoint day/night schedule driven by the
//   sim step (the clock, FixedTimestep's counter): midnightTint at each
//   day's start and end, noonTint at the midpoint, integer-linear between
//   (the day wraps: the tint is a function of step % stepsPerDay).
// - A LIGHT SPRITE is an additive pool: at world (x, y) its contribution
//   is intensity * (radius² - dist²) / radius² for dist² < radius² (integer
//   falloff — squared distances, no sqrt), 0 beyond.
// - sampleAt = ambient + the pools' sum, clamped to [0, 255] per channel.
// - hashAt(step) is the ADR 0014 structural hash over (the clock, the
//   lights in order, and the sample values at the NAMED COORDINATES — the
//   map's four corners and centre): deterministic on any host, and it
//   moves with the clock and with the lights.

struct LightTint {
    int r = 255, g = 255, b = 255;

    bool operator==(const LightTint&) const = default;
};

struct LightSprite {
    int x = 0, y = 0;      // the pool's centre, world px
    int radius = 0;        // px; the contribution falls to zero at the edge
    int intensity = 0;     // 0..255 additive strength at the centre

    bool operator==(const LightSprite&) const = default;
};

class LightMap {
public:
    struct Config {
        int mapWidth = 0, mapHeight = 0;   // world px (the named coordinates)
        LightTint noonTint;                // the schedule's midpoint
        LightTint midnightTint;            // the schedule's endpoints
        long long stepsPerDay = 1;         // > 0: the clock's day length
        std::vector<LightSprite> lights;
    };

    explicit LightMap(const Config& config);

    /// The ambient tint at sim step `step` (the clock).
    LightTint ambientAt(long long step) const;

    /// The light value at world (x, y) at sim step `step`: ambient plus the
    /// additive pools, clamped to [0, 255] per channel.
    LightTint sampleAt(int x, int y, long long step) const;

    /// The structural light hash (ADR 0014) at sim step `step`: FNV-1a over
    /// the clock, the lights in order, and the samples at the five named
    /// coordinates (the map's corners and centre).
    std::uint64_t hashAt(long long step) const;

private:
    Config config_{};
};

}  // namespace ply
