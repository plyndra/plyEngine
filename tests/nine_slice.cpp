// Headless nine-slice geometry tests: E-a2.1 (corner invariance), E-a2.2 (edges
// stretch or tile, never sampling outside the region) and E-a2.3 (degenerate
// targets compress caps instead of inverting them), plus the structural hash
// primitive they depend on. No GL context, no window, no floating point.

#include "plyEngine/NineSlice.hpp"
#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
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

void eqHash(std::uint64_t got, std::uint64_t want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got 0x%016llx, want 0x%016llx)\n", what,
                     static_cast<unsigned long long>(got),
                     static_cast<unsigned long long>(want));
        ++failures;
    }
}

// --- fixtures --------------------------------------------------------------

/// 48x48 source, 12 px margins, tiled edges, stretched centre.
ply::UIAtlasRegion panel() {
    ply::UIAtlasRegion region;
    region.name = "panel";
    region.x = 0;
    region.y = 0;
    region.w = 48;
    region.h = 48;
    region.hasMargins = true;
    region.margins = ply::UIMargins{12, 12, 12, 12};
    region.slice = ply::SliceMode::Nine;
    region.edges = ply::StretchMode::Tile;
    region.centre = ply::StretchMode::Stretch;
    return region;
}

/// 32x24 source, 8/6 margins, three-slice horizontal, stretched centre.
ply::UIAtlasRegion tooltip() {
    ply::UIAtlasRegion region;
    region.name = "tip";
    region.x = 0;
    region.y = 68;
    region.w = 32;
    region.h = 24;
    region.hasMargins = true;
    region.margins = ply::UIMargins{8, 6, 8, 6};
    region.slice = ply::SliceMode::ThreeH;
    region.centre = ply::StretchMode::Stretch;
    return region;
}

ply::UIAtlasRegion plainRegion() {
    ply::UIAtlasRegion region;
    region.name = "gear";
    region.x = 64;
    region.y = 0;
    region.w = 16;
    region.h = 16;
    region.slice = ply::SliceMode::Plain;
    return region;
}

// --- quad helpers ----------------------------------------------------------

int quadW(const ply::UIQuad& quad) { return quad.topRight.x - quad.topLeft.x; }
int quadH(const ply::UIQuad& quad) { return quad.bottomLeft.y - quad.topLeft.y; }
const ply::UIQuad& at(const std::vector<ply::UIQuad>& quads, std::size_t index) {
    return quads.at(index);
}

bool insideRegion(const ply::UIQuad& quad, const ply::UIAtlasRegion& region) {
    const int minU = region.x, maxU = region.x + region.w;
    const int minV = region.y, maxV = region.y + region.h;
    for (const ply::UIVertex* vertex :
         {&quad.topLeft, &quad.topRight, &quad.bottomRight, &quad.bottomLeft}) {
        if (vertex->u < minU || vertex->u > maxU || vertex->v < minV || vertex->v > maxV)
            return false;
    }
    return true;
}

bool positiveQuads(const std::vector<ply::UIQuad>& quads) {
    for (const ply::UIQuad& quad : quads) {
        if (quadW(quad) <= 0 || quadH(quad) <= 0) return false;
    }
    return true;
}

/// Corner size and source rect only, ignoring where the corner sits: this is the
/// number E-a2.1 requires to be identical at every panel size.
std::uint64_t cornerSignature(const std::vector<ply::UIQuad>& quads) {
    ply::StructuralHasher hasher;
    for (std::size_t i = 0; i < 4; ++i) {
        const ply::UIQuad& quad = at(quads, i);
        hasher.i32(quadW(quad));
        hasher.i32(quadH(quad));
        hasher.i32(quad.topLeft.u);
        hasher.i32(quad.topLeft.v);
        hasher.i32(quad.topRight.u - quad.topLeft.u);
        hasher.i32(quad.bottomLeft.v - quad.topLeft.v);
    }
    return hasher.value();
}

// --- E-a2.1: corners are fixed ---------------------------------------------

void testCornerInvariance() {
    const ply::UIAtlasRegion region = panel();
    const int sizes[3][2] = {{48, 48}, {96, 64}, {200, 120}};

    std::vector<std::uint64_t> cornerHashes;
    std::vector<std::uint64_t> fullHashes;

    for (const auto& size : sizes) {
        const int targetWidth = size[0];
        const int targetHeight = size[1];
        const std::vector<ply::UIQuad> quads =
            ply::nineSliceGeometry(region, targetWidth, targetHeight);
        check(quads.size() >= 4, "corners emitted first");
        if (quads.size() < 4) return;

        // Each corner keeps the 12 px source size and its own source rect.
        eq(quadW(at(quads, 0)), 12, "top-left width at every size");
        eq(quadH(at(quads, 0)), 12, "top-left height at every size");
        eq(at(quads, 0).topLeft.x, 0, "top-left anchored at x=0");
        eq(at(quads, 0).topLeft.y, 0, "top-left anchored at y=0");
        eq(at(quads, 1).topRight.x, targetWidth, "top-right flush with the right edge");
        eq(at(quads, 2).bottomLeft.y, targetHeight, "bottom-left flush with the bottom edge");
        eq(at(quads, 3).topRight.x, targetWidth, "bottom-right flush right");
        eq(at(quads, 3).bottomLeft.y, targetHeight, "bottom-right flush bottom");
        eq(at(quads, 1).topLeft.u, region.x + region.w - 12, "top-right samples the right corner");

        cornerHashes.push_back(cornerSignature(quads));
        fullHashes.push_back(ply::structuralHash(quads));
    }

    eqHash(cornerHashes[0], cornerHashes[1], "corner signature identical at 96x64");
    eqHash(cornerHashes[0], cornerHashes[2], "corner signature identical at 200x120");
    check(fullHashes[0] != fullHashes[2], "whole-geometry hash still differs by size");
}

// --- E-a2.2: edges and centre stretch or tile ------------------------------

void testTiledEdges() {
    const ply::UIAtlasRegion region = panel();  // midW 76, source centre strip 24
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 100, 48);

    // 4 corners, 4 top tiles (24+24+24+4), 4 bottom tiles, 1 left, 1 right, 1 centre.
    eq(static_cast<long long>(quads.size()), 15, "tiled edge quad count");

    const std::size_t topFirst = 4;
    const int widths[4] = {24, 24, 24, 4};
    for (std::size_t i = 0; i < 4; ++i) {
        const ply::UIQuad& quad = at(quads, topFirst + i);
        eq(quadW(quad), widths[i], "top tile width, last one clipped");
        eq(quad.topRight.u - quad.topLeft.u, widths[i], "top tile samples 1:1");
        eq(quadH(quad), 12, "top edge height stays the source margin");
    }
    eq(at(quads, topFirst).topLeft.x, 12, "top run starts after the corner");
    eq(at(quads, topFirst + 3).topRight.x, 100 - 12, "last tile flush to the right corner");
    eq(at(quads, topFirst + 3).topLeft.u, 12, "clipped tile samples from the strip start");

    for (const ply::UIQuad& quad : quads) {
        check(insideRegion(quad, region), "tiled edge never samples outside the region");
    }
}

void testStretchedEdges() {
    ply::UIAtlasRegion region = panel();
    region.edges = ply::StretchMode::Stretch;
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 100, 48);

    eq(static_cast<long long>(quads.size()), 9, "stretched edge quad count");
    eq(quadW(at(quads, 4)), 76, "stretched top edge spans the gap");
    eq(at(quads, 4).topRight.u - at(quads, 4).topLeft.u, 24,
       "stretched edge keeps the whole source strip");
}

void testTiledCentre() {
    ply::UIAtlasRegion region = panel();
    region.centre = ply::StretchMode::Tile;
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 100, 48);

    // 4 corners, 4+4 edge tiles, 1+1 side tiles, 4 centre tiles.
    eq(static_cast<long long>(quads.size()), 18, "tiled centre quad count");
    const std::size_t centreFirst = 14;
    const int widths[4] = {24, 24, 24, 4};
    for (std::size_t i = 0; i < 4; ++i) {
        const ply::UIQuad& quad = at(quads, centreFirst + i);
        eq(quadW(quad), widths[i], "centre tile width, last one clipped");
        eq(quadH(quad), 24, "centre tile height");
        eq(quad.topRight.u - quad.topLeft.u, widths[i], "centre tile samples 1:1");
    }
    for (const ply::UIQuad& quad : quads) check(insideRegion(quad, region), "centre tile in bounds");
}

void testPlainAndThreeSlice() {
    {
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(plainRegion(), 40, 20);
        eq(static_cast<long long>(quads.size()), 1, "plain emits one quad");
        eq(quadW(at(quads, 0)), 40, "plain stretches across the target");
        eq(at(quads, 0).topRight.u - at(quads, 0).topLeft.u, 16, "plain samples the whole region");
    }
    {
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(tooltip(), 100, 24);
        eq(static_cast<long long>(quads.size()), 3, "three_h emits two caps and a centre");
        eq(quadW(at(quads, 0)), 8, "left cap keeps its source width");
        eq(quadH(at(quads, 0)), 24, "left cap fills the target height");
        eq(quadW(at(quads, 1)), 84, "centre fills the gap");
        eq(at(quads, 1).topRight.u - at(quads, 1).topLeft.u, 16, "centre samples the source strip");
        eq(at(quads, 2).topLeft.x, 92, "right cap sits at the far edge");
        eq(at(quads, 2).topLeft.u, 24, "right cap samples the right strip");
    }
    {
        // Tiled three-slice centre: 84 px over a 16 px strip = 5 whole + 4 clipped.
        ply::UIAtlasRegion region = tooltip();
        region.centre = ply::StretchMode::Tile;
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 100, 24);
        eq(static_cast<long long>(quads.size()), 8, "three_h tiled centre quad count");
        eq(quadW(at(quads, 6)), 4, "three_h last tile clipped");
        eq(at(quads, 6).topRight.x, 92, "three_h last tile flush to the right cap");
        for (const ply::UIQuad& quad : quads) check(insideRegion(quad, region), "three_h in bounds");
    }
    {
        // ThreeV: vertical caps, horizontal centre.
        ply::UIAtlasRegion region = tooltip();
        region.slice = ply::SliceMode::ThreeV;
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 32, 60);
        eq(static_cast<long long>(quads.size()), 3, "three_v emits two caps and a centre");
        eq(quadH(at(quads, 0)), 6, "top cap keeps its source height");
        eq(quadW(at(quads, 0)), 32, "top cap fills the target width");
        eq(at(quads, 1).topLeft.y, 6, "three_v centre starts below the cap");
        eq(at(quads, 2).topLeft.y, 60 - 6, "three_v bottom cap sits at the far edge");
    }
}

// --- E-a2.3: degenerate targets -------------------------------------------

void testDegenerateWidth() {
    const ply::UIAtlasRegion region = panel();  // 12 + 12 caps against a 10 px target
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 10, 48);
    check(!quads.empty(), "degenerate width still emits geometry");
    check(positiveQuads(quads), "degenerate width never inverts a quad");
    if (quads.size() < 4) return;

    // shared = 9: left 12*9/24 = 4, right takes the remaining 5.
    eq(quadW(at(quads, 0)), 4, "top-left cap compressed");
    eq(quadW(at(quads, 1)), 5, "top-right cap takes the remainder");
    eq(at(quads, 1).topRight.x, 10, "caps plus centre fill the target exactly");
    eq(at(quads, 0).topRight.u - at(quads, 0).topLeft.u, 12,
       "compressed cap still samples the full corner");
    for (const ply::UIQuad& quad : quads) check(insideRegion(quad, region), "compressed in bounds");
}

void testDegenerateHeight() {
    const ply::UIAtlasRegion region = panel();
    const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(region, 48, 10);
    check(positiveQuads(quads), "degenerate height never inverts a quad");
    if (quads.size() < 4) return;
    eq(quadH(at(quads, 0)), 4, "top cap compressed");
    eq(quadH(at(quads, 2)), 5, "bottom cap takes the remainder");
    eq(at(quads, 2).bottomLeft.y, 10, "caps plus centre fill the target height");
}

void testTinyAndEmpty() {
    {
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(panel(), 1, 1);
        eq(static_cast<long long>(quads.size()), 1, "1x1 target emits only the centre");
        eq(quadW(at(quads, 0)), 1, "1x1 centre width");
        eq(quadH(at(quads, 0)), 1, "1x1 centre height");
    }
    check(ply::nineSliceGeometry(panel(), 0, 48).empty(), "zero width emits nothing");
    check(ply::nineSliceGeometry(panel(), 48, 0).empty(), "zero height emits nothing");
    check(ply::nineSliceGeometry(panel(), -8, -8).empty(), "negative target emits nothing");
}

// --- hash primitive -------------------------------------------------------

std::uint64_t naiveFnv(const std::string& text) {
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

void testStructuralHasher() {
    // Published FNV-1a 64 vectors: they pin the algorithm, so an accidental
    // change to the folding order cannot pass silently.
    eqHash(ply::fnv1a64("", 0), 0xcbf29ce484222325ull, "FNV-1a 64 of empty string");
    eqHash(ply::fnv1a64("a", 1), 0xaf63dc4c8601ec8cull, "FNV-1a 64 of \"a\"");
    eqHash(ply::fnv1a64("foobar", 6), 0x85944171f73967e8ull, "FNV-1a 64 of \"foobar\"");

    for (const std::string text : {"", "a", "plyEngine", "nine-slice"}) {
        eqHash(ply::fnv1a64(text.data(), text.size()), naiveFnv(text), "matches a naive reference");
    }

    // The declared widths are little-endian regardless of host byte order.
    {
        ply::StructuralHasher hasher;
        hasher.u32(0x01020304u);
        const unsigned char bytes[4] = {0x04, 0x03, 0x02, 0x01};
        eqHash(hasher.value(), ply::fnv1a64(bytes, 4), "u32 hashes little-endian bytes");
    }
    {
        ply::StructuralHasher hasher;
        hasher.i32(-1);
        const unsigned char bytes[4] = {0xff, 0xff, 0xff, 0xff};
        eqHash(hasher.value(), ply::fnv1a64(bytes, 4), "i32 is two's complement");
    }
    {
        ply::StructuralHasher hasher;
        hasher.i32(7);
        const std::uint64_t first = hasher.value();
        hasher.reset();
        hasher.i32(7);
        eqHash(hasher.value(), first, "reset restores the offset basis");
    }

    // The quad hash covers every field: one changed vertex changes the value.
    {
        const std::vector<ply::UIQuad> quads = ply::nineSliceGeometry(panel(), 96, 96);
        std::vector<ply::UIQuad> nudged = quads;
        nudged[0].topLeft.u += 1;
        check(ply::structuralHash(quads) != ply::structuralHash(nudged),
              "quad hash covers the source rect");
        eqHash(ply::structuralHash(quads), ply::structuralHash(quads), "quad hash is stable");
    }
}

}  // namespace

int main() {
    testStructuralHasher();
    testCornerInvariance();
    testTiledEdges();
    testStretchedEdges();
    testTiledCentre();
    testPlainAndThreeSlice();
    testDegenerateWidth();
    testDegenerateHeight();
    testTinyAndEmpty();

    if (failures == 0) {
        std::printf("nine_slice tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "nine_slice tests: %d failure(s)\n", failures);
    return 1;
}
