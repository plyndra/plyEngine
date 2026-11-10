#pragma once

#include "plyEngine/NineSlice.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ply {

/// Pixel text at integer sizes (M1a a6, UI_FRAMEWORK.md §7).
///
/// The source is a BMFont `.fnt` descriptor rather than a TTF rasterized at
/// load, decided for M1a because BMFont metrics are integers written by the
/// exporter: a glyph hash over them is exact and reproducible across hosts,
/// while a rasterizer's sub-pixel placement is not. TTF-at-load still needs to
/// land for the sizes a designer will want after M1a; it can emit the same
/// BMFont struct once it has rasterized, so nothing here would change.
///
/// "No AA, integer sizes" is not an assertion here but a property of the data
/// model: every field is an int, so a quad can only land on whole pixels.

/// One glyph's metrics, verbatim from the `.fnt` descriptor. `x`,`y`,`w`,`h`
/// locate the glyph in the atlas page; `xOffset`/`yOffset` place it relative to
/// the pen and the line top; `xAdvance` is pen movement.
struct BMFontGlyph {
    int id = 0;
    int x = 0, y = 0, w = 0, h = 0;
    int xOffset = 0, yOffset = 0;
    int xAdvance = 0;
    int page = 0;

    bool operator==(const BMFontGlyph&) const = default;
};

/// Kerning adjustment applied between two glyph ids, in px.
struct BMFontKerning {
    int first = 0, second = 0, amount = 0;

    bool operator==(const BMFontKerning&) const = default;
};

/// A parsed BMFont descriptor. `base` is the distance from the line top to the
/// baseline, so a glyph's top sits at `baseline - (base - yOffset)`.
struct BMFont {
    std::string face;
    int size = 0;
    int lineHeight = 0;
    int base = 0;
    int scaleW = 0, scaleH = 0;
    std::string pageFile;

    /// Sorted by id, so lookup is a binary search and two fonts that differ only
    /// in file order compare equal.
    std::vector<BMFontGlyph> glyphs;
    std::vector<BMFontKerning> kerning;

    bool operator==(const BMFont&) const = default;

    /// Glyph lookup by codepoint; nullptr when the font does not have it.
    const BMFontGlyph* findGlyph(int id) const;

    /// Kerning between two ids, or 0 when the pair has none.
    int kerningBetween(int first, int second) const;
};

/// Every way a descriptor can be rejected. The names are part of the contract —
/// tests and the art lint match the token, not the message prose (the same
/// discipline as the atlas error codes).
enum class BMFontErrorCode {
    None,
    FileNotReadable,
    MalformedLine,
    MissingField,
    WrongType,
    /// The file is BMFont's binary form; this loader reads the text form only.
    BinaryFormatUnsupported,
    DuplicateGlyph,
    NoGlyphs,
    NonPositiveSize,
};

const char* toString(BMFontErrorCode code);

struct BMFontError {
    BMFontErrorCode code = BMFontErrorCode::None;
    /// "<Token>: <detail>", naming the offending line or field.
    std::string message;

    bool ok() const { return code == BMFontErrorCode::None; }
    const char* name() const { return toString(code); }
};

struct BMFontLoadResult {
    BMFont font;
    BMFontError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

/// Parses a BMFont text-format descriptor. Unknown lines are ignored, so a
/// newer exporter's extra fields still load.
BMFontLoadResult loadBMFontFromString(std::string_view text);

/// Reads the file, then behaves like loadBMFontFromString.
BMFontLoadResult loadBMFontFile(const std::string& path);

/// One laid-out string.
struct TextLayout {
    /// Ink quads in internal-resolution px; `u`/`v` are the glyph's source rect
    /// in the atlas page. Empty when the string has no drawable glyph.
    std::vector<UIQuad> quads;

    /// Codepoints the font does not have, in first-encountered order. The spec
    /// fails missing glyphs at the art lint rather than drawing blanks; this is
    /// the data that lint reads, so a layout never silently loses text.
    std::vector<int> missingGlyphIds;

    /// The union of the drawn quads: ink extents, not the advance box. A string
    /// whose last glyph has right-side bearing does not measure as wide as the
    /// pen moved, and E-a6.1 requires bounds and drawn extents to agree, so this
    /// is the honest definition rather than the advance width.
    UIRect bounds;

    /// Pen movement for the whole string (advance widths and kerning), which is
    /// what a following widget needs to place itself. Not the same number as
    /// bounds.w.
    int advanceWidth = 0;

    /// Lines laid out, and the line height scaled by the layout scale.
    int lineCount = 0;
    int scaledLineHeight = 0;
};

/// Lays `text` out with its pen starting at `originX`, `baselineY` on the first
/// line, at integer `scale` (values below 1 are treated as 1).
///
/// Contract:
///  - Glyph top = baselineY - (font.base - glyph.yOffset) * scale: baseline
///    alignment by ascent, not by the top of the glyph box.
///  - '\n' moves the pen to originX and the baseline down one line.
///  - Kerning is applied between consecutive glyphs on the same line, never
///    across a line break.
///  - Bytes >= 0x80 are not decoded as UTF-8: they are reported as missing
///    glyphs, which is honest at this stage rather than drawing the wrong glyph.
///  - Every emitted coordinate and size is an integer, so text can never be
///    drawn at a fractional offset.
TextLayout layoutText(const BMFont& font, std::string_view text, int originX, int baselineY,
                      int scale);

/// The union of the quads' rects, recomputed from the quads alone. Zero-size
/// when there are no quads. layoutText's `bounds` must equal this — that
/// equality is what E-a6.1 checks, so a bounds implementation that drifted to
/// advance widths would fail rather than agree with itself.
UIRect textInkBounds(const std::vector<UIQuad>& quads);

/// FNV-1a 64 over the layout: glyph count, then each quad's four vertices, then
/// the missing-glyph list and the advance width. Integers only.
std::uint64_t structuralHash(const TextLayout& layout);

}  // namespace ply
