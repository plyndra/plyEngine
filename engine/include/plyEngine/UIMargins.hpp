#pragma once

#include "plyEngine/UIAtlas.hpp"
#include "plyEngine/UILayout.hpp"

#include <string>
#include <vector>

namespace ply {

class Image;

// Alpha-contour margin derivation (M1a a3, UI_FRAMEWORK.md §3): the fallback for
// art whose atlas entry declares no margins. Pure data — an in-memory RGBA
// buffer, no texture, no GL, no window, no clock — so the whole unit grades in
// ctest.
//
// The rule is §3's own wording, applied to the alpha channel: inside a region,
// walk inward from each edge and count the run of lines whose alpha equals the
// alpha of that edge's corner pixel. That run is the margin for that side. So a
// region with a fully transparent ring of thickness k derives k on every side of
// the ring, and a region with an opaque border derives nothing on the sides the
// border covers — which is why §3 calls this the fallback for *generated* art
// (transparent padding) and why art with an opaque frame must declare its
// margins instead. Deriving from alpha, not from colour, keeps the rule
// independent of palette.
//
// Derivation is a pure function of (image, region): it has no target-size
// parameter, so a derived margin cannot vary with the panel it is sliced into.
// That is what E-a3.2's "stable across 3 panel sizes" pins — the same region
// sliced at three sizes, with the derived margins unchanged and the caps fixed.

/// Every way derivation can refuse. Named tokens, like the loaders' error codes.
enum class MarginErrorCode {
    None,
    /// The region is empty, negative, or not fully inside the image.
    RegionOutOfBounds,
    /// The derived caps leave no centre on an axis (§3: "a source region must
    /// keep a centre"). This is what an opaque, margin-less region reports —
    /// the alpha contour says the whole region is border.
    NoCentre,
};

/// Stable token for the code, e.g. "NoCentre". Never renamed.
const char* toString(MarginErrorCode code);

struct MarginError {
    MarginErrorCode code = MarginErrorCode::None;
    /// "<Token>: <detail>". Empty when the code is None.
    std::string message;

    bool ok() const { return code == MarginErrorCode::None; }
    const char* name() const { return toString(code); }
};

struct MarginResult {
    UIMargins margins;
    MarginError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

/// Derives [left, top, right, bottom] for `region` from `image`'s alpha contour.
/// `region` is in image pixels, top-left origin, row 0 = top of the image.
MarginResult deriveMargins(const Image& image, const UIRect& region);

/// One `margins_source: derived` atlas entry, re-derived and compared with the
/// recorded values (§4: "derived entries are re-derived and re-checked at load
/// and fail the art lint if the derived values drift outside ±1 px").
struct DerivedMarginCheck {
    std::string region;
    UIMargins recorded;
    UIMargins derived;
    /// Recorded vs derived on each side, in source px.
    int dl = 0, dt = 0, dr = 0, db = 0;
    /// False when any side drifted beyond `tolerancePx`, or derivation failed.
    bool withinTolerance = false;
    MarginError error;

    bool operator==(const DerivedMarginCheck&) const = default;
};

/// Re-derives every `derived`-source region of `atlas` against `image` and
/// reports each one. Regions with other sources are not checked; entries that
/// fail derivation carry the error and `withinTolerance == false`.
std::vector<DerivedMarginCheck> checkDerivedMargins(const UIAtlas& atlas, const Image& image,
                                                    int tolerancePx = 1);

}  // namespace ply
