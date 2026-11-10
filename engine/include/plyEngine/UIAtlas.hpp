#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace ply {

/// How a nine-slice edge or centre segment is filled when the target rect is
/// larger than the source region.
enum class StretchMode { Stretch, Tile };

/// Which slicing rule applies to a region.
///
///   Nine    corners fixed, edges and centre fill between them
///   ThreeH  left/right caps fixed, centre fills horizontally
///   ThreeV  top/bottom caps fixed, centre fills vertically
///   Plain   the whole region stretches, margins are ignored
enum class SliceMode { Nine, ThreeH, ThreeV, Plain };

/// Where a region's margins came from. `Derived` margins are re-derived from
/// the alpha contour and checked against the recorded values (a3); the loader
/// only records the declaration.
enum class MarginSource { Declared, Derived };

/// Nine-slice margins in source pixels, pre-scale.
struct UIMargins {
    int left = 0, top = 0, right = 0, bottom = 0;

    bool operator==(const UIMargins&) const = default;
};

/// One named sub-rectangle of the atlas image.
struct UIAtlasRegion {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;

    /// False when the entry declares no margins at all. Generated art usually
    /// does not; a3 derives them from the alpha contour, so an entry without
    /// margins behaves as `Plain` until then.
    bool hasMargins = false;
    UIMargins margins;
    MarginSource marginSource = MarginSource::Declared;

    SliceMode slice = SliceMode::Nine;
    StretchMode edges = StretchMode::Stretch;
    StretchMode centre = StretchMode::Stretch;

    /// Cursor hotspots are in source pixels relative to the region origin.
    bool hasHotspot = false;
    int hotspotX = 0, hotspotY = 0;

    bool operator==(const UIAtlasRegion&) const = default;
};

/// A named frame sequence. `durationMs` has one entry per frame.
struct UIAtlasAnimation {
    std::string name;
    std::vector<std::string> frames;
    std::vector<int> durationMs;

    bool operator==(const UIAtlasAnimation&) const = default;
};

/// A font reference: the file to rasterize and the integer sizes to bake.
struct UIAtlasFont {
    std::string name;
    std::string file;
    std::vector<int> sizes;

    bool operator==(const UIAtlasFont&) const = default;
};

/// A parsed UI atlas (schema: docs/planning/UI_FRAMEWORK.md §4). Pure data:
/// no texture, no GL, no window, so it is constructible and testable headlessly.
///
/// The roadmap calls this unit `UIAtlasLoader`. It is the load/emit/validate
/// function trio below rather than a class, matching how the 1.x spritesheet
/// parsers are shaped.
///
/// Loading canonicalises order, so two atlases that differ only in JSON key
/// order compare equal and emit identical bytes.
struct UIAtlas {
    std::string image;
    std::string palette;

    /// Sorted by name.
    std::vector<UIAtlasRegion> regions;
    /// Sorted by name.
    std::vector<UIAtlasAnimation> animations;
    /// Sorted by name.
    std::vector<UIAtlasFont> fonts;

    bool operator==(const UIAtlas&) const = default;

    /// Region lookup by name; nullptr when absent.
    const UIAtlasRegion* findRegion(std::string_view name) const;
};

/// Every way an atlas can be rejected. The names are part of the contract:
/// tests and the art lint match on the token, not on the message prose.
enum class UIAtlasErrorCode {
    None,
    FileNotReadable,
    MalformedJson,
    MissingField,
    WrongType,
    EmptyRegionName,
    DuplicateRegion,
    NegativePosition,
    NonPositiveSize,
    NegativeMargin,
    MarginTooLarge,
    UnknownMarginsSource,
    UnknownStretchMode,
    UnknownSliceMode,
    HotspotOutOfBounds,
    EmptyAnimation,
    DuplicateAnimation,
    FrameCountMismatch,
    MissingRegion,
    NegativeDuration,
    DuplicateFont,
    EmptyFontSizes,
    NonPositiveFontSize,
};

/// Stable token for the code, e.g. "MarginTooLarge". Never renamed.
const char* toString(UIAtlasErrorCode code);

struct UIAtlasError {
    UIAtlasErrorCode code = UIAtlasErrorCode::None;
    /// "<Token>: <detail>", where the detail names the offending
    /// region/field/animation. Empty when the code is None.
    std::string message;

    bool ok() const { return code == UIAtlasErrorCode::None; }
    const char* name() const { return toString(code); }
};

struct UIAtlasLoadResult {
    UIAtlas atlas;
    UIAtlasError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

/// Parses and validates an atlas document. Returns a named error and an empty
/// atlas on failure. Unknown keys are ignored, so a newer document still loads
/// in an older build. Duplicate JSON object keys are collapsed by the parser
/// (last one wins), so the loader cannot report them.
UIAtlasLoadResult loadUIAtlasFromString(std::string_view text);

/// Reads the file, then behaves like loadUIAtlasFromString.
/// `FileNotReadable` on an I/O failure.
UIAtlasLoadResult loadUIAtlasFile(const std::string& path);

/// Canonical JSON for `atlas`: region/animation/font entries in name order
/// regardless of struct order, default-valued fields omitted, two-space
/// indent, trailing newline. Deterministic for a given atlas, and stable
/// across load→emit→load.
///
/// Emitting does not validate; call validateUIAtlas first for hand-built
/// atlases (everything the loader returns is already valid).
std::string emitUIAtlas(const UIAtlas& atlas);

/// Checks a hand-built atlas against exactly the rules the loader applies.
/// Returns code None when the atlas is valid.
UIAtlasError validateUIAtlas(const UIAtlas& atlas);

}  // namespace ply
