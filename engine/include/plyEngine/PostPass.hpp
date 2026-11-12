#pragma once

#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <vector>

namespace ply {

// PostPass (M1b b8b, ARCHITECTURE_TARGET.md: "PostPass — vignette, optional
// palette LUT"): the frame-edge vignette and an optional palette remap —
// E-b8.2: "off/on hashes differ; off is the default in tests". Pure
// integer data: no GL, no window, no float.
//
// Model (documented once, pinned literally):
// - The vignette darkens toward the frame's corners: at frame px (x, y)
//   the darkening FACTOR is 255 - strength * d² / maxd², where d² is the
//   squared distance from the frame's centre and maxd² the squared
//   half-diagonal (the corner's d²) — integer math, no sqrt. The centre
//   keeps 255 (untouched); the corners land exactly on 255 - strength.
// - OFF (the default — the b8b clause) means the factor is 255 everywhere.
// - The palette LUT is optional (empty by default): 256 entries mapping an
//   8-bit palette index to a palette index; it participates in the hash
//   when present (a LUT change moves the pass's structural hash).
// - hash() is the ADR 0014 structural hash over the pass state: the
//   vignette flag, the strength, the factor field at the five named
//   coordinates (the frame's corners and centre), and the LUT. The
//   off/on hashes differ — the E-b8.2 clause.

class PostPass {
public:
    struct Config {
        int frameWidth = 640, frameHeight = 360;  // ADR 0012's internal res
        bool vignette = false;                    // OFF is the default
        int vignetteStrength = 0;                 // 0..255
        std::vector<int> paletteLut;              // optional, 256 entries
    };

    explicit PostPass(const Config& config);

    bool vignetteEnabled() const;

    /// The darkening factor at frame px (x, y): 255 untouched down to
    /// 255 - strength at the corners (255 everywhere when off).
    int vignetteFactorAt(int x, int y) const;

    /// The structural hash of the pass state (the E-b8.2 off/on clause).
    std::uint64_t hash() const;

private:
    Config config_{};
};

}  // namespace ply
