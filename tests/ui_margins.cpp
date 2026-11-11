// Headless margin-derivation tests (M1a a3): E-a3.1 — alpha-derived margins
// equal the margins declared in ui_atlas.json, exactly — and E-a3.2 — the
// derived margins are stable across three panel sizes (the same region sliced
// at 48x48, 96x64 and 200x120 keeps its caps fixed, ±0 px).
//
// The fixture pixels (tests/data/ui.png) are built by tests/data/make_ui_png.py
// from ui_atlas.json's geometry, and pinned here by SHA-256 so a fixture change
// cannot pass unnoticed. Pure data: an in-memory RGBA buffer, no texture, no
// GL, no window, no clock.

#include "plyEngine/Image.hpp"
#include "plyEngine/NineSlice.hpp"
#include "plyEngine/UIAtlas.hpp"
#include "plyEngine/UILayout.hpp"
#include "plyEngine/UIMargins.hpp"

#include <cstdint>
#include <cstdio>
#include <fstream>
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

void eqStr(const std::string& got, const std::string& want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got '%s', want '%s')\n", what, got.c_str(),
                     want.c_str());
        ++failures;
    }
}

void eqMargins(const ply::UIMargins& got, const ply::UIMargins& want, const char* what) {
    if (!(got == want)) {
        std::fprintf(stderr, "  FAIL: %s (got [%d,%d,%d,%d], want [%d,%d,%d,%d])\n", what,
                     got.left, got.top, got.right, got.bottom, want.left, want.top, want.right,
                     want.bottom);
        ++failures;
    }
}

using ply::UIMargins;
using ply::UIRect;

std::string dataPath(const char* name) { return std::string(PLY_TEST_DATA_DIR) + "/" + name; }

// --- SHA-256, so the fixture pin is a literal digest checked in-test ---------
//
// No SHA-256 exists in the engine (manifests will need one with b-row
// provenance), so the grader carries a compact FIPS 180-4 implementation
// rather than trusting a comment. Verified against the empty-string digest in
// testFixtureIntegrity below.

std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

std::string sha256Hex(const std::string& bytes) {
    static const std::uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2,
    };
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                          0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    std::string message = bytes;
    const std::uint64_t bitLength = static_cast<std::uint64_t>(bytes.size()) * 8;
    message.push_back(static_cast<char>(0x80));
    while (message.size() % 64 != 56) message.push_back('\0');
    for (int i = 7; i >= 0; --i) message.push_back(static_cast<char>((bitLength >> (i * 8)) & 0xff));

    for (std::size_t chunk = 0; chunk < message.size(); chunk += 64) {
        std::uint32_t w[64] = {};
        for (int i = 0; i < 16; ++i) {
            const auto* p = reinterpret_cast<const unsigned char*>(message.data() + chunk + i * 4);
            w[i] = (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
                   (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + s1 + ch + k[i] + w[i];
            const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = s0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }

    char out[65];
    for (int i = 0; i < 8; ++i) std::snprintf(out + i * 8, 9, "%08x", h[i]);
    return std::string(out, 64);
}

// --- the fixture ------------------------------------------------------------

const ply::UIAtlas& atlas() {
    static const ply::UIAtlas loaded = ply::loadUIAtlasFile(dataPath("ui_atlas.json")).atlas;
    return loaded;
}

const ply::Image& uiImage() {
    static const ply::Image image = ply::Image::loadFromFile(dataPath("ui.png"));
    return image;
}

std::string readBytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

UIRect regionRect(const char* name) {
    const ply::UIAtlasRegion* r = atlas().findRegion(name);
    if (r == nullptr) return UIRect{};
    return UIRect{r->x, r->y, r->w, r->h};
}

void testFixtureIntegrity() {
    check(atlas().findRegion("panel.wood") != nullptr, "the atlas fixture loads");

    const ply::Image& image = uiImage();
    check(image.valid(), "tests/data/ui.png decodes");
    eq(image.width(), 160, "fixture width");
    eq(image.height(), 128, "fixture height");

    // The digest is pinned so a fixture edit cannot silently change what the
    // margins are derived from. Regenerate with tests/data/make_ui_png.py;
    // `--check` reports drift without writing.
    eqStr(sha256Hex(readBytes(dataPath("ui.png"))),
          "7c49a0becfa0551ee00132cb18a16bdec991dc734d4488a4a84fb39ee4630348",
          "ui.png sha256 is pinned");
    // The digest function itself is checked against a published vector, so a
    // broken SHA-256 cannot make the pin above vacuously pass.
    eqStr(sha256Hex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256 matches the FIPS empty-string vector");

    // Pinned alpha probes: the ring is transparent, the interior is opaque.
    const auto alphaAt = [&](int x, int y) {
        return image.pixels()[static_cast<std::size_t>(y) * image.width() * 4 +
                               static_cast<std::size_t>(x) * 4 + 3];
    };
    eq(alphaAt(0, 0), 0, "panel.wood's ring is transparent at (0,0)");
    eq(alphaAt(11, 24), 0, "still transparent at the ring's last column (11,24)");
    eq(alphaAt(12, 24), 255, "opaque at the first interior column (12,24)");
    eq(alphaAt(49, 1), 0, "bar.frame's 4 px top band is transparent at (49,1)");
    eq(alphaAt(52, 5), 255, "bar.frame's interior is opaque at (52,5)");
    eq(alphaAt(0, 92), 0, "divider.h's left band is transparent at (0,92)");
    eq(alphaAt(10, 92), 255, "divider.h has NO top band: (10,92) is opaque");
}

// --- E-a3.1: derived == declared, exactly -----------------------------------

struct MarginCase {
    const char* region;
    int left, top, right, bottom;
};

// The four margined regions of the fixture. bar.frame is the entry whose
// margins_source is `derived` — the one §4 re-derives at load — and the other
// three are declared, so this is the roadmap's "derived == declared on the
// fixture" comparison rather than a self-check of one entry.
constexpr MarginCase kMarginCases[] = {
    {"panel.wood", 12, 12, 12, 12},
    {"bar.frame", 4, 4, 4, 4},
    {"panel.tooltip", 8, 6, 8, 6},
    {"divider.h", 8, 0, 8, 0},  // 0 is a legitimate margin: no vertical padding
};

void testDerivedEqualsDeclared() {
    long long checked = 0;
    for (const MarginCase& c : kMarginCases) {
        const ply::UIAtlasRegion* entry = atlas().findRegion(c.region);
        check(entry != nullptr, "margined region exists in the fixture atlas");
        if (entry == nullptr) continue;

        const ply::MarginResult derived =
            ply::deriveMargins(uiImage(), UIRect{entry->x, entry->y, entry->w, entry->h});
        check(derived.ok(), "derivation succeeds on a margined fixture region");
        const UIMargins want{c.left, c.top, c.right, c.bottom};
        eqMargins(derived.margins, want, c.region);

        // The atlas's own recorded value is what the exactness is against.
        check(entry->hasMargins, "the fixture entry records margins");
        eqMargins(entry->margins, want, "the atlas records the same margins");
        ++checked;
    }
    eq(checked, 4, "all 4 margined fixture regions were compared (one loop)");
}

// --- E-a3.2: stable across three panel sizes --------------------------------

// a2's reference panel sizes.
constexpr int kTargets[3][2] = {{48, 48}, {96, 64}, {200, 120}};

void testStableAcrossPanelSizes() {
    long long sliced = 0;
    for (const MarginCase& c : kMarginCases) {
        const ply::UIAtlasRegion* entry = atlas().findRegion(c.region);
        if (entry == nullptr || !entry->hasMargins) continue;

        // Build the region the nine-slice emitter consumes, using the DERIVED
        // margins rather than the declared ones: a3 feeds a2.
        ply::UIAtlasRegion withDerived = *entry;
        withDerived.margins = ply::deriveMargins(uiImage(), regionRect(c.region)).margins;
        const UIMargins caps = withDerived.margins;

        int firstCapW = -1, firstCapH = -1;
        for (const auto& target : kTargets) {
            // Deriving again for this panel size returns the same integers: the
            // derivation has no size input, which is the ±0 px the criterion
            // asks for.
            const UIMargins again = ply::deriveMargins(uiImage(), regionRect(c.region)).margins;
            eqMargins(again, caps, "derived margins are identical for every panel size");

            const std::vector<ply::UIQuad> quads =
                ply::nineSliceGeometry(withDerived, target[0], target[1]);
            check(!quads.empty(), "slicing with the derived margins emits geometry");
            ++sliced;

            // Cap extents follow a2's slice contract: `nine` fixes both caps,
            // `three_h` fixes only the side caps (the quad spans the target
            // height) and `three_v` only the top/bottom ones. So the pinned
            // invariant is per-mode, and a mode that fixed the wrong axis would
            // fail here rather than pass silently.
            if (quads.empty()) continue;
            const ply::UIQuad& first = quads[0];
            const int capW = first.topRight.x - first.topLeft.x;
            const int capH = first.bottomLeft.y - first.topLeft.y;
            switch (withDerived.slice) {
                case ply::SliceMode::Nine:
                    eq(capW, caps.left, "nine-slice top-left corner keeps the left cap (width)");
                    eq(capH, caps.top, "nine-slice top-left corner keeps the top cap (height)");
                    if (firstCapW < 0) {
                        firstCapW = capW;
                        firstCapH = capH;
                    }
                    eq(capW, firstCapW, "cap width is identical across the 3 panel sizes (±0 px)");
                    eq(capH, firstCapH, "cap height is identical across the 3 panel sizes (±0 px)");
                    break;
                case ply::SliceMode::ThreeH:
                    eq(capW, caps.left, "three_h cap keeps the derived left cap (width)");
                    eq(capH, target[1], "three_h cap spans the target height");
                    if (firstCapW < 0) firstCapW = capW;
                    eq(capW, firstCapW, "three_h cap width is identical across sizes (±0 px)");
                    break;
                case ply::SliceMode::ThreeV:
                    eq(capH, caps.top, "three_v cap keeps the derived top cap (height)");
                    eq(capW, target[0], "three_v cap spans the target width");
                    if (firstCapH < 0) firstCapH = capH;
                    eq(capH, firstCapH, "three_v cap height is identical across sizes (±0 px)");
                    break;
                case ply::SliceMode::Plain:
                    eq(capW, target[0], "plain ignores margins: one full-width quad");
                    eq(capH, target[1], "plain ignores margins: one full-height quad");
                    break;
            }
        }
    }
    eq(sliced, 12, "4 regions x 3 panel sizes were sliced (one loop)");
}

// --- refusals, named --------------------------------------------------------

void testNamedRefusals() {
    // An opaque, margin-less region has no alpha contour to follow: the run
    // covers the whole axis, so there is no centre left (§3) and derivation
    // refuses rather than inventing zero margins.
    const ply::MarginResult opaque = ply::deriveMargins(uiImage(), regionRect("slot.normal"));
    check(!opaque.ok(), "an opaque margin-less region is refused");
    eqStr(opaque.error.name(), "NoCentre", "the refusal is named NoCentre");

    // A fully transparent region reports the same: every line matches the
    // corner's alpha, so the run consumes the axis.
    const ply::MarginResult hollow = ply::deriveMargins(uiImage(), UIRect{0, 0, 8, 8});
    check(!hollow.ok(), "an all-transparent region is refused");
    eqStr(hollow.error.name(), "NoCentre", "the refusal is named NoCentre");

    // Regions that are not inside the image cannot be measured.
    const ply::MarginResult past = ply::deriveMargins(uiImage(), UIRect{150, 120, 40, 40});
    check(!past.ok(), "a region past the image edge is refused");
    eqStr(past.error.name(), "RegionOutOfBounds", "the refusal is named RegionOutOfBounds");
    const ply::MarginResult empty = ply::deriveMargins(uiImage(), UIRect{0, 0, 0, 8});
    check(!empty.ok(), "an empty region is refused");
    eqStr(empty.error.name(), "RegionOutOfBounds", "the refusal is named RegionOutOfBounds");

    // The boundary the bounds check must not cross: a region that exactly fills
    // the image is inside it, so it must be measured, not refused.
    const ply::MarginResult exact = ply::deriveMargins(
        uiImage(), UIRect{0, 0, uiImage().width(), uiImage().height()});
    check(exact.ok(), "a region that exactly fills the image is inside it, not out of bounds");

    // §4's re-derive-and-re-check: bar.frame is the fixture's one derived
    // entry and re-derives exactly, while the declared-source entries are not
    // this function's business.
    const std::vector<ply::DerivedMarginCheck> report = ply::checkDerivedMargins(atlas(), uiImage());
    eq(report.size(), 1, "exactly one fixture entry has margins_source: derived");
    if (!report.empty()) {
        eqStr(report[0].region, "bar.frame", "the checked entry is bar.frame");
        check(report[0].withinTolerance, "bar.frame re-derives within tolerance");
        eq(report[0].dl + report[0].dt + report[0].dr + report[0].db, 0,
           "bar.frame's re-derivation drift is 0 on every side");
        eqMargins(report[0].derived, UIMargins{4, 4, 4, 4}, "bar.frame derives [4,4,4,4]");
    }
    // A zero tolerance still passes on exact art: the rule is an exact match
    // here, not a lucky ±1.
    const std::vector<ply::DerivedMarginCheck> strict =
        ply::checkDerivedMargins(atlas(), uiImage(), 0);
    check(strict.size() == 1 && strict[0].withinTolerance,
          "bar.frame re-derives exactly (tolerance 0)");
}

// One line of measured values, printed so the ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence() {
    const auto derived = [](const char* name) {
        return ply::deriveMargins(uiImage(), regionRect(name)).margins;
    };
    const UIMargins wood = derived("panel.wood");
    const UIMargins bar = derived("bar.frame");
    const UIMargins tip = derived("panel.tooltip");
    const UIMargins div = derived("divider.h");
    const std::vector<ply::DerivedMarginCheck> report = ply::checkDerivedMargins(atlas(), uiImage());

    std::printf(
        "EVIDENCE ui_margins sha256=%s px=%dx%d wood=%d/%d/%d/%d bar=%d/%d/%d/%d "
        "tip=%d/%d/%d/%d div=%d/%d/%d/%d derived_entries=%zu drift=%d sliced=12 "
        "no_centre=%s bounds=%s\n",
        sha256Hex(readBytes(dataPath("ui.png"))).substr(0, 16).c_str(), uiImage().width(),
        uiImage().height(), wood.left, wood.top, wood.right, wood.bottom, bar.left, bar.top,
        bar.right, bar.bottom, tip.left, tip.top, tip.right, tip.bottom, div.left, div.top,
        div.right, div.bottom, report.size(),
        report.empty() ? -1
                       : report[0].dl + report[0].dt + report[0].dr + report[0].db,
        ply::deriveMargins(uiImage(), regionRect("slot.normal")).error.name(),
        ply::deriveMargins(uiImage(), UIRect{150, 120, 40, 40}).error.name());
}

}  // namespace

int main() {
    testFixtureIntegrity();
    testDerivedEqualsDeclared();
    testStableAcrossPanelSizes();
    testNamedRefusals();

    if (failures == 0) {
        if (uiImage().valid() && atlas().findRegion("bar.frame") != nullptr) {
            printEvidence();
        } else {
            std::fprintf(stderr, "  FAIL: evidence preconditions\n");
            return 1;
        }
        std::printf("ui_margins tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_margins tests: %d failure(s)\n", failures);
    return 1;
}
