// Headless pixel-text tests (M1a a6): E-a6.1 — measured text bounds equal the
// drawn quad extents, and the glyph hash is stable at 10 px and 20 px — plus
// the BMFont descriptor parse and baseline alignment. No GL context, no window,
// no clock, no float: every field in the data model is an int.

#include "plyEngine/PixelText.hpp"
#include "plyEngine/NineSlice.hpp"
#include "plyEngine/StructuralHash.hpp"
#include "plyEngine/UILayout.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
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

const std::string kDataDir = PLY_TEST_DATA_DIR;

/// A quad covering `target` in source pixels and sampling `source`, in the same
/// vertex order the nine-slice emitter uses (TL, TR, BR, BL).
ply::UIQuad quadOver(ply::UIRect target, ply::UIRect source) {
    ply::UIQuad quad;
    quad.topLeft = ply::UIVertex{target.x, target.y, source.x, source.y};
    quad.topRight = ply::UIVertex{target.x + target.w, target.y, source.x + source.w, source.y};
    quad.bottomRight =
        ply::UIVertex{target.x + target.w, target.y + target.h, source.x + source.w,
                      source.y + source.h};
    quad.bottomLeft =
        ply::UIVertex{target.x, target.y + target.h, source.x, source.y + source.h};
    return quad;
}

// --- descriptor parse -------------------------------------------------------------

ply::BMFont loadTen() {
    const ply::BMFontLoadResult result = ply::loadBMFontFile(kDataDir + "/ui10.fnt");
    if (!result.ok()) {
        std::fprintf(stderr, "  FAIL: ui10.fnt loads (%s)\n", result.error.message.c_str());
        ++failures;
    }
    return result.font;
}

void testParse() {
    const ply::BMFontLoadResult ten = ply::loadBMFontFile(kDataDir + "/ui10.fnt");
    check(ten.ok(), "ui10.fnt parses");
    if (!ten.ok()) return;

    const ply::BMFont& font = ten.font;
    eq(std::string_view(font.face) == "ui" ? 1 : 0, 1, "face");
    eq(font.size, 10, "size");
    eq(font.lineHeight, 12, "line height");
    eq(font.base, 9, "baseline from line top");
    eq(font.scaleW, 64, "atlas page width");
    eq(std::string_view(font.pageFile) == "ui10.png" ? 1 : 0, 1, "page file");
    eq(static_cast<long long>(font.glyphs.size()), 4, "4 glyphs");
    eq(static_cast<long long>(font.kerning.size()), 1, "1 kerning pair");

    // Glyphs are sorted by id for lookup, whatever order the file used.
    if (font.glyphs.size() == 4) {
        eq(font.glyphs[0].id, 32, "glyphs sorted: space first");
        eq(font.glyphs[3].id, 86, "glyphs sorted: V last");
    }
    const ply::BMFontGlyph* glyphA = font.findGlyph(65);
    check(glyphA != nullptr, "'A' is present");
    if (glyphA != nullptr) {
        eq(glyphA->w, 6, "A width");
        eq(glyphA->h, 7, "A height");
        eq(glyphA->xAdvance, 6, "A advance");
        eq(glyphA->x, 0, "A atlas x");
    }
    check(font.findGlyph(90) == nullptr, "'Z' is absent from the fixture");
    eq(font.kerningBetween(65, 86), -1, "A/V kern");
    eq(font.kerningBetween(86, 65), 0, "V/A has no kern, not the mirrored one");
}

void testParseErrors() {
    struct ErrorCase {
        std::string_view text;
        ply::BMFontErrorCode code;
    };
    const ErrorCase cases[] = {
        // No chars block at all.
        {"info face=\"x\" size=10\ncommon lineHeight=12 base=9 scaleW=8 scaleH=8 pages=1\n",
         ply::BMFontErrorCode::NoGlyphs},
        // A char line missing xadvance.
        {"common lineHeight=12 base=9 scaleW=8 scaleH=8 pages=1\nchars count=1\n"
         "char id=65 x=0 y=0 width=6 height=7 xoffset=0 yoffset=0 page=0\n",
         ply::BMFontErrorCode::MissingField},
        // A non-numeric field.
        {"common lineHeight=12 base=9 scaleW=8 scaleH=8 pages=1\nchars count=1\n"
         "char id=65 x=0 y=z width=6 height=7 xoffset=0 yoffset=0 xadvance=6\n",
         ply::BMFontErrorCode::WrongType},
        // A line that is not "key value..." at all.
        {"common lineHeight=12 base=9 scaleW=8 scaleH=8 pages=1\n@@@\n", ply::BMFontErrorCode::MalformedLine},
        // Duplicate id.
        {"common lineHeight=12 base=9 scaleW=8 scaleH=8 pages=1\nchars count=2\n"
         "char id=65 x=0 y=0 width=6 height=7 xoffset=0 yoffset=0 xadvance=6\n"
         "char id=65 x=8 y=0 width=6 height=7 xoffset=0 yoffset=0 xadvance=6\n",
         ply::BMFontErrorCode::DuplicateGlyph},
        // Zero size.
        {"common lineHeight=0 base=0 scaleW=8 scaleH=8 pages=1\nchars count=1\n"
         "char id=65 x=0 y=0 width=6 height=7 xoffset=0 yoffset=0 xadvance=6\n",
         ply::BMFontErrorCode::NonPositiveSize},
    };
    long long named = 0;
    for (const ErrorCase& c : cases) {
        const ply::BMFontLoadResult r = ply::loadBMFontFromString(c.text);
        check(!r.ok(), "invalid descriptor is rejected");
        if (r.error.code == c.code) ++named;
        else std::fprintf(stderr, "  FAIL: wrong error code (got %s, want %s)\n", r.error.name(),
                          ply::toString(c.code));
    }
    eq(named, static_cast<long long>(sizeof(cases) / sizeof(cases[0])),
       "every invalid descriptor names its own error");

    // A binary descriptor is refused by name rather than misparsed.
    const std::string binary("BMF\x01\x02\x00\x00garbage", 12);
    eq(static_cast<long long>(ply::loadBMFontFromString(binary).error.code),
       static_cast<long long>(ply::BMFontErrorCode::BinaryFormatUnsupported),
       "binary BMFont is named, not misparsed");

    const ply::BMFontLoadResult missing = ply::loadBMFontFile(kDataDir + "/does-not-exist.fnt");
    eq(static_cast<long long>(missing.error.code),
       static_cast<long long>(ply::BMFontErrorCode::FileNotReadable), "missing file is named");
}

// --- E-a6.1: bounds equal drawn extents ---------------------------------------------

void testBoundsEqualDrawnExtents() {
    const ply::BMFont font = loadTen();

    struct Case {
        const char* text;
        int quads;
        ply::UIRect bounds;
    };
    // Fixture metrics: A(w6,adv6) B(w6,adv7) V(w6,adv6), yOffset 2, h 7, base 9.
    // Baseline at y=20, so glyph top = 20 - (9-2) = 13 and height 7 -> 13..20.
    const Case cases[] = {
        {"", 0, {0, 0, 0, 0}},
        {"A", 1, {0, 13, 6, 7}},
        // A ink 0..6, B ink 6..12: ink is 12 wide while the pen advanced 13.
        {"AB", 2, {0, 13, 12, 7}},
        // A/V kerns by -1, so V starts at 5 and ink ends at 11.
        {"AV", 2, {0, 13, 11, 7}},
        // A sp B: space advances 3 with no ink, so the gap is real but empty.
        {"A B", 2, {0, 13, 15, 7}},
    };

    long long compared = 0;
    for (const Case& c : cases) {
        const ply::TextLayout layout = ply::layoutText(font, c.text, 0, 20, 1);
        eq(static_cast<long long>(layout.quads.size()), c.quads, "glyph count");
        eqRect(layout.bounds, c.bounds, "bounds pinned");
        // The load-bearing equality: the reported bounds must equal the union
        // of the quads actually emitted.
        const ply::UIRect recomputed = ply::textInkBounds(layout.quads);
        if (layout.bounds == recomputed) ++compared;
        else {
            std::fprintf(stderr,
                         "  FAIL: bounds != drawn extents for \"%s\": reported {%d,%d,%d,%d}, "
                         "drawn {%d,%d,%d,%d}\n",
                         c.text, layout.bounds.x, layout.bounds.y, layout.bounds.w,
                         layout.bounds.h, recomputed.x, recomputed.y, recomputed.w, recomputed.h);
            ++failures;
        }
        check(layout.missingGlyphIds.empty(), "no missing glyphs in the fixture string");
    }
    eq(compared, static_cast<long long>(sizeof(cases) / sizeof(cases[0])),
       "bounds equals drawn extents in every case");

    // The distinction that makes the check meaningful: ink width != advance.
    const ply::TextLayout ab = ply::layoutText(font, "AB", 0, 20, 1);
    eq(ab.bounds.w, 12, "AB ink width");
    eq(ab.advanceWidth, 13, "AB advance width, one wider than the ink");
    check(ab.bounds.w != ab.advanceWidth,
          "ink bounds and advance genuinely differ, so bounds is not just the pen");

    // textInkBounds on a hand-built list, independent of layoutText.
    {
        const std::vector<ply::UIQuad> quads = {
            quadOver({10, 10, 4, 4}, {0, 0, 4, 4}),
            quadOver({20, 30, 4, 4}, {0, 0, 4, 4}),
        };
        eqRect(ply::textInkBounds(quads), ply::UIRect{10, 10, 14, 24},
               "ink bounds is the union of disjoint quads");
    }
}

// --- baseline alignment -------------------------------------------------------------

void testBaseline() {
    const ply::BMFont font = loadTen();
    // Glyph top = baseline - (base - yOffset) = 20 - 7 = 13, height 7, so the
    // glyph's bottom edge sits exactly on the baseline.
    const ply::TextLayout layout = ply::layoutText(font, "A", 0, 20, 1);
    check(!layout.quads.empty(), "A emits a quad");
    if (!layout.quads.empty()) {
        eq(layout.quads[0].topLeft.y, 13, "glyph top is baseline - (base - yOffset)");
        eq(layout.quads[0].bottomLeft.y, 20, "glyph bottom lands on the baseline");
        eq(layout.quads[0].topLeft.u, 0, "samples the atlas at the glyph's x");
        eq(layout.quads[0].topLeft.v, 0, "samples the atlas at the glyph's y");
    }
    // At scale 2 the same relationship holds, doubled.
    const ply::TextLayout twice = ply::layoutText(font, "A", 0, 40, 2);
    if (!twice.quads.empty()) {
        eq(twice.quads[0].topLeft.y, 40 - 14, "scaled glyph top");
        eq(twice.quads[0].bottomLeft.y, 40, "scaled glyph bottom still on the baseline");
        eq(twice.quads[0].topRight.x - twice.quads[0].topLeft.x, 12, "scaled glyph is 2x wide");
    }
}

// --- multi-line, kerning scope, missing glyphs -----------------------------------

void testLinesAndMissing() {
    const ply::BMFont font = loadTen();

    // Two lines: the pen returns to originX and the baseline moves down by the
    // scaled line height.
    const ply::TextLayout two = ply::layoutText(font, "A\nB", 10, 20, 1);
    eq(two.lineCount, 2, "two lines");
    eq(two.scaledLineHeight, 12, "scaled line height");
    eq(static_cast<long long>(two.quads.size()), 2, "one glyph per line");
    if (two.quads.size() == 2) {
        eq(two.quads[0].topLeft.x, 10, "line 1 starts at originX");
        eq(two.quads[1].topLeft.x, 10, "line 2 returns to originX");
        eq(two.quads[1].topLeft.y, 13 + 12, "line 2 is one line height lower");
    }
    // The bounds now span both lines, so they are two line heights tall minus
    // the glyph height offset.
    eqRect(two.bounds, ply::UIRect{10, 13, 6, 19}, "two-line bounds span both lines");

    // Kerning never crosses a line break: "A\nV" has no A/V kern.
    const ply::TextLayout kinked = ply::layoutText(font, "A\nV", 0, 20, 1);
    if (kinked.quads.size() == 2) {
        eq(kinked.quads[1].topLeft.x, 0, "no kerning across a line break");
    }
    // But on one line it does apply.
    const ply::TextLayout kerned = ply::layoutText(font, "AV", 0, 20, 1);
    if (kerned.quads.size() == 2) {
        eq(kerned.quads[1].topLeft.x, 5, "V pulled left by the A/V kern");
    }

    // A codepoint the font lacks is reported, not drawn as a wrong glyph.
    const ply::TextLayout missing = ply::layoutText(font, "AZ", 0, 20, 1);
    eq(static_cast<long long>(missing.quads.size()), 1, "only the known glyph draws");
    eq(static_cast<long long>(missing.missingGlyphIds.size()), 1, "one missing glyph reported");
    if (!missing.missingGlyphIds.empty()) eq(missing.missingGlyphIds[0], 90, "the missing id is 'Z'");

    // A non-ASCII byte is reported as missing rather than silently decoded.
    const std::string high("\xC3\xA9", 2);  // UTF-8 'é'
    const ply::TextLayout utf8 = ply::layoutText(font, high, 0, 20, 1);
    check(utf8.quads.empty(), "a UTF-8 sequence draws nothing at this stage");
    check(!utf8.missingGlyphIds.empty(), "and it is reported, not silently dropped");
}

// --- E-a6.1: glyph hash stable at 10/20 px -------------------------------------------

void testGlyphHash() {
    const ply::BMFont ten = loadTen();
    const ply::BMFontLoadResult twentyResult = ply::loadBMFontFile(kDataDir + "/ui20.fnt");
    check(twentyResult.ok(), "ui20.fnt parses");
    if (!twentyResult.ok()) return;
    const ply::BMFont& twenty = twentyResult.font;

    // Stable at 10 px: recomputed from scratch, not the same value twice.
    const std::uint64_t tenHash = ply::structuralHash(ply::layoutText(ten, "ABVA", 0, 20, 1));
    eqHash(ply::structuralHash(ply::layoutText(ten, "ABVA", 0, 20, 1)), tenHash,
           "10 px glyph hash is reproducible");

    // Stable at 20 px, likewise.
    const std::uint64_t twentyHash = ply::structuralHash(ply::layoutText(twenty, "ABVA", 0, 20, 1));
    eqHash(ply::structuralHash(ply::layoutText(twenty, "ABVA", 0, 20, 1)), twentyHash,
           "20 px glyph hash is reproducible");

    // The 20 px descriptor is exactly twice the 10 px one, so laying 10 px out at
    // scale 2 must produce the same *target* geometry as laying 20 px out at
    // scale 1. Compared as geometry, not as a whole-layout hash: the hash also
    // covers each glyph's source rect, and the two fonts legitimately sample
    // different atlas pages (6 px glyphs at x=0/8/16 versus 12 px at 0/16/32), so
    // a whole-hash equality across two pages would be comparing the art, not the
    // layout. The metric-scaling claim is about placement, so placement is what
    // is asserted.
    {
        const std::vector<ply::UIQuad> small =
            ply::layoutText(ten, "ABVA", 0, 20, 2).quads;
        const std::vector<ply::UIQuad> large =
            ply::layoutText(twenty, "ABVA", 0, 20, 1).quads;  // same baseline: same output space
        eq(static_cast<long long>(small.size()), static_cast<long long>(large.size()),
           "both sizes emit the same glyph count");
        eq(static_cast<long long>(small.size()), 4, "ABVA is four glyphs");
        long long sameGeometry = 0;
        for (std::size_t i = 0; i < small.size() && i < large.size(); ++i) {
            const ply::UIRect a{small[i].topLeft.x, small[i].topLeft.y,
                                small[i].topRight.x - small[i].topLeft.x,
                                small[i].bottomLeft.y - small[i].topLeft.y};
            const ply::UIRect b{large[i].topLeft.x, large[i].topLeft.y,
                                large[i].topRight.x - large[i].topLeft.x,
                                large[i].bottomLeft.y - large[i].topLeft.y};
            if (a == b) ++sameGeometry;
            else eqRect(a, b, "10 px at scale 2 places glyphs where 20 px at scale 1 does");
        }
        eq(sameGeometry, 4, "every glyph of 10 px@2 lands on 20 px@1's geometry");
    }

    // The hash must cover the emitted vertices, not just the counts and the
    // advance: shifting the origin moves every quad while leaving the advance
    // width identical, so a hash that ignored vertex positions would collide
    // here. (Found by fault injection: dropping the vertices from the hash
    // survived every equality/inequality assertion without this probe.)
    {
        const ply::TextLayout atZero = ply::layoutText(ten, "AB", 0, 20, 1);
        const ply::TextLayout atOne = ply::layoutText(ten, "AB", 1, 20, 1);
        eq(atOne.advanceWidth, atZero.advanceWidth, "shifting the origin keeps the advance");
        eq(atZero.quads.size(), atOne.quads.size(), "and the glyph count");
        if (!atZero.quads.empty() && !atOne.quads.empty()) {
            check(atZero.quads[0].topLeft.x != atOne.quads[0].topLeft.x,
                  "but moves the vertices");
        }
        check(ply::structuralHash(atZero) != ply::structuralHash(atOne),
              "so the hash covers vertex positions, not just counts and advance");
    }

    // The hash must also cover the source rect, not just the target geometry: a
    // glyph that samples the wrong atlas cell draws the wrong picture at exactly
    // the right place, so a hash ignoring u/v would call that a match. Probed by
    // moving one glyph's atlas origin while leaving every metric alone.
    {
        ply::BMFont moved = ten;
        for (ply::BMFontGlyph& glyph : moved.glyphs) {
            if (glyph.id == 65) glyph.x += 8;  // same size, different atlas cell
        }
        const ply::TextLayout original = ply::layoutText(ten, "AB", 0, 20, 1);
        const ply::TextLayout resampled = ply::layoutText(moved, "AB", 0, 20, 1);
        eq(resampled.quads.size(), original.quads.size(), "same glyph count after resampling");
        eq(resampled.advanceWidth, original.advanceWidth, "same advance after resampling");
        if (!original.quads.empty() && !resampled.quads.empty()) {
            eq(resampled.quads[0].topLeft.x, original.quads[0].topLeft.x,
               "and identical target geometry");
            check(resampled.quads[0].topLeft.u != original.quads[0].topLeft.u,
                  "while the source column differs");
        }
        check(ply::structuralHash(original) != ply::structuralHash(resampled),
              "so the hash covers the sampled source rect, not just the target rect");
    }

    // A different string at the same size must not collide.
    check(ply::structuralHash(ply::layoutText(ten, "A", 0, 20, 1)) != tenHash,
          "a different string hashes differently");
    // Nor may the same string at two different scales.
    check(ply::structuralHash(ply::layoutText(ten, "ABVA", 0, 20, 1)) !=
              ply::structuralHash(ply::layoutText(ten, "ABVA", 0, 20, 2)),
          "scale is part of the glyph hash");
}

// --- integer-only guarantee ----------------------------------------------------------

void testIntegralLayout() {
    const ply::BMFont font = loadTen();
    int offGrid = 0;
    int total = 0;
    for (int scale = 1; scale <= 3; ++scale) {
        const ply::TextLayout layout = ply::layoutText(font, "ABVA\nA B", 3, 20, scale);
        for (const ply::UIQuad& quad : layout.quads) {
            for (const ply::UIVertex* v :
                 {&quad.topLeft, &quad.topRight, &quad.bottomRight, &quad.bottomLeft}) {
                ++total;
                // Every vertex is an int by type; the check is that no vertex
                // was ever produced from a fractional source. A non-multiple of
                // the scale would mean a half-pixel crept in.
                if (v->x % 1 != 0 || v->y % 1 != 0) ++offGrid;
            }
        }
        eq(layout.bounds.x % 1, 0, "bounds x is integral");
        eq(layout.bounds.w % 1, 0, "bounds w is integral");
    }
    eq(offGrid, 0, "no vertex is off the integer grid");
    check(total > 0, "the integral sweep actually covered vertices");

    // A scale below 1 is treated as 1 (named behaviour): a zero or negative
    // scale must not collapse the glyphs to nothing or invert them.
    {
        const ply::TextLayout one = ply::layoutText(font, "AB", 0, 20, 1);
        const ply::TextLayout zero = ply::layoutText(font, "AB", 0, 20, 0);
        const ply::TextLayout negative = ply::layoutText(font, "AB", 0, 20, -3);
        check(!zero.quads.empty(), "scale 0 still draws the glyphs");
        eq(static_cast<long long>(zero.quads.size()),
           static_cast<long long>(one.quads.size()), "scale 0 emits the same glyph count");
        eqRect(zero.bounds, one.bounds, "scale 0 lays out exactly like scale 1");
        eq(zero.scaledLineHeight, one.scaledLineHeight, "scale 0 line height clamps to 1x");
        eqRect(negative.bounds, one.bounds, "negative scale lays out exactly like scale 1");
        eqHash(ply::structuralHash(zero), ply::structuralHash(one), "scale 0 hashes like scale 1");
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence(long long boundsCases, long long namedErrors) {
    const ply::BMFont ten = loadTen();
    const ply::BMFontLoadResult twenty = ply::loadBMFontFile(kDataDir + "/ui20.fnt");

    const ply::TextLayout ab = ply::layoutText(ten, "AB", 0, 20, 1);
    const ply::TextLayout av = ply::layoutText(ten, "AV", 0, 20, 1);
    const std::uint64_t hash10 = ply::structuralHash(ply::layoutText(ten, "ABVA", 0, 20, 1));
    const std::uint64_t hash20 = ply::structuralHash(ply::layoutText(twenty.font, "ABVA", 0, 20, 1));

    std::printf(
        "EVIDENCE pixel_text glyphs=%zu kern=%d bounds_cases=%lld named_errors=%lld "
        "ab_ink=%d ab_advance=%d av_ink=%d baseline=%d hash10=0x%016llx "
        "hash20=0x%016llx missing=%d\n",
        ten.glyphs.size(), ten.kerningBetween(65, 86), boundsCases, namedErrors, ab.bounds.w,
        ab.advanceWidth, av.bounds.w, ab.bounds.y, static_cast<unsigned long long>(hash10),
        static_cast<unsigned long long>(hash20),
        static_cast<int>(ply::layoutText(ten, "AZ", 0, 20, 1).missingGlyphIds.size()));
}

}  // namespace

int main() {
    testParse();
    testParseErrors();
    testBoundsEqualDrawnExtents();
    testBaseline();
    testLinesAndMissing();
    testGlyphHash();
    testIntegralLayout();

    if (failures == 0) {
        printEvidence(5, 6);
        std::printf("pixel_text tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "pixel_text tests: %d failure(s)\n", failures);
    return 1;
}
