// Headless UIAtlasLoader tests: pure data, no GL context, no window.
//
// E-a1.1  load -> emit -> load produces identical JSON.
// E-a1.2  every invalid atlas fails with a named error.

#include "plyEngine/UIAtlas.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#ifndef PLY_TEST_DATA_DIR
#define PLY_TEST_DATA_DIR "tests/data"
#endif

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

void checkString(const std::string& got, const std::string& expected, const char* what) {
    if (got != expected) {
        std::fprintf(stderr, "  FAIL: %s (got \"%s\", expected \"%s\")\n", what, got.c_str(),
                     expected.c_str());
        ++failures;
    }
}

std::string fixturePath() { return std::string(PLY_TEST_DATA_DIR) + "/ui_atlas.json"; }

// A minimal valid document, used as the base for the invalid-case table.
std::string validAtlas(const std::string& regions = R"({"panel.wood": {"x": 0, "y": 0, "w": 48, "h": 48,
                 "margins": [12, 12, 12, 12], "margins_source": "declared"}})",
                       const std::string& animations = "{}") {
    return "{\n  \"image\": \"ui.png\",\n  \"regions\": " + regions + ",\n  \"animations\": " +
           animations + "\n}\n";
}

// The named-error contract: the code is what the caller branches on, the token
// is what a test or the art lint matches, and the detail must say which entry
// is at fault (a bare token is not a usable error).
void expectError(const char* what, const std::string& text, ply::UIAtlasErrorCode expected) {
    const ply::UIAtlasLoadResult result = ply::loadUIAtlasFromString(text);
    if (result.ok()) {
        std::fprintf(stderr, "  FAIL: %s (expected %s, got success)\n", what,
                     ply::toString(expected));
        ++failures;
        return;
    }
    if (result.error.code != expected) {
        std::fprintf(stderr, "  FAIL: %s (expected %s, got %s: %s)\n", what,
                     ply::toString(expected), result.error.name(), result.error.message.c_str());
        ++failures;
        return;
    }
    const std::string token = ply::toString(expected);
    if (!result.error.message.empty() &&
        result.error.message.rfind(token + ":", 0) != 0) {
        std::fprintf(stderr, "  FAIL: %s (message does not start with \"%s:\": %s)\n", what,
                     token.c_str(), result.error.message.c_str());
        ++failures;
    }
    if (result.error.message.size() <= token.size() + 2) {
        std::fprintf(stderr, "  FAIL: %s (message has no detail: %s)\n", what,
                     result.error.message.c_str());
        ++failures;
    }
    if (!result.atlas.image.empty() || !result.atlas.regions.empty()) {
        std::fprintf(stderr, "  FAIL: %s (failed load must not return a partial atlas)\n", what);
        ++failures;
    }
}

// --- E-a1.1: load -> emit -> load -----------------------------------------

void testFixtureLoads() {
    const ply::UIAtlasLoadResult result = ply::loadUIAtlasFile(fixturePath());
    if (!result.ok()) {
        std::fprintf(stderr, "  FAIL: fixture does not load: %s\n", result.error.message.c_str());
        ++failures;
        return;
    }
    const ply::UIAtlas& atlas = result.atlas;

    checkString(atlas.image, "ui.png", "fixture image");
    checkString(atlas.palette, "ui-32", "fixture palette");
    check(atlas.regions.size() == 12, "fixture region count");
    check(atlas.animations.size() == 1, "fixture animation count");
    check(atlas.fonts.size() == 1, "fixture font count");

    // Loading canonicalises order, which is what makes emit deterministic.
    std::string previous;
    for (const ply::UIAtlasRegion& region : atlas.regions) {
        check(previous < region.name, "regions sorted by name");
        previous = region.name;
    }

    const ply::UIAtlasRegion* panel = atlas.findRegion("panel.wood");
    check(panel != nullptr, "panel.wood found");
    if (panel != nullptr) {
        check(panel->x == 0 && panel->y == 0 && panel->w == 48 && panel->h == 48,
              "panel.wood rect");
        check(panel->hasMargins, "panel.wood declares margins");
        check(panel->margins == ply::UIMargins{12, 12, 12, 12}, "panel.wood margins");
        check(panel->marginSource == ply::MarginSource::Declared, "panel.wood margins_source");
        check(panel->slice == ply::SliceMode::Nine, "panel.wood slice mode");
        check(panel->edges == ply::StretchMode::Tile, "panel.wood edges tile");
        check(panel->centre == ply::StretchMode::Stretch, "panel.wood centre stretch");
        check(!panel->hasHotspot, "panel.wood has no hotspot");
    }

    const ply::UIAtlasRegion* bar = atlas.findRegion("bar.frame");
    check(bar != nullptr, "bar.frame found");
    if (bar != nullptr) {
        check(bar->marginSource == ply::MarginSource::Derived, "bar.frame margins_source derived");
        check(bar->margins == ply::UIMargins{4, 4, 4, 4}, "bar.frame margins");
        check(bar->edges == ply::StretchMode::Stretch && bar->centre == ply::StretchMode::Stretch,
              "bar.frame default stretch modes");
    }

    const ply::UIAtlasRegion* gear = atlas.findRegion("icon.gear");
    check(gear != nullptr && gear->slice == ply::SliceMode::Plain, "icon.gear slice plain");
    check(gear != nullptr && !gear->hasMargins, "icon.gear declares no margins");

    const ply::UIAtlasRegion* cursor = atlas.findRegion("cursor.point");
    check(cursor != nullptr && cursor->hasHotspot, "cursor.point hotspot present");
    if (cursor != nullptr) {
        check(cursor->hotspotX == 1 && cursor->hotspotY == 1, "cursor.point hotspot value");
    }

    const ply::UIAtlasRegion* tooltip = atlas.findRegion("panel.tooltip");
    check(tooltip != nullptr && tooltip->slice == ply::SliceMode::ThreeH,
          "panel.tooltip slice three_h");
    check(tooltip != nullptr && tooltip->margins == ply::UIMargins{8, 6, 8, 6},
          "panel.tooltip margins");

    const ply::UIAtlasRegion* divider = atlas.findRegion("divider.h");
    check(divider != nullptr && divider->edges == ply::StretchMode::Stretch &&
              divider->centre == ply::StretchMode::Tile,
          "divider.h stretched edges, tiled centre");
    check(divider != nullptr && divider->margins.top == 0 && divider->margins.bottom == 0,
          "divider.h horizontal-only margins");

    check(atlas.findRegion("does.not.exist") == nullptr, "unknown region lookup is nullptr");

    if (atlas.animations.size() == 1) {
        const ply::UIAtlasAnimation& animation = atlas.animations.front();
        checkString(animation.name, "next-arrow", "animation name");
        check(animation.frames.size() == 3, "animation frame count");
        check(animation.durationMs == std::vector<int>{120, 120, 200}, "animation durations");
        if (animation.frames.size() == 3) {
            checkString(animation.frames[0], "arrow.0", "animation frame 0");
            checkString(animation.frames[2], "arrow.2", "animation frame 2");
        }
    }

    if (atlas.fonts.size() == 1) {
        const ply::UIAtlasFont& font = atlas.fonts.front();
        checkString(font.name, "ui", "font name");
        checkString(font.file, "ui.ttf", "font file");
        check(font.sizes == std::vector<int>{10, 20, 30}, "font sizes");
    }
}

void testRoundTrip() {
    const ply::UIAtlasLoadResult first = ply::loadUIAtlasFile(fixturePath());
    check(first.ok(), "round-trip: fixture loads");
    if (!first.ok()) return;

    const std::string emittedOnce = ply::emitUIAtlas(first.atlas);
    const ply::UIAtlasLoadResult second = ply::loadUIAtlasFromString(emittedOnce);
    check(second.ok(), "round-trip: emitted document reloads");
    if (!second.ok()) {
        std::fprintf(stderr, "  FAIL: reload error: %s\n", second.error.message.c_str());
        return;
    }

    // E-a1.1: identical JSON, byte for byte, on the second pass.
    const std::string emittedTwice = ply::emitUIAtlas(second.atlas);
    if (emittedOnce != emittedTwice) {
        std::fprintf(stderr, "  FAIL: emitted JSON differs after reload\n");
        ++failures;
    }

    // ...and identical state, compared field by field.
    check(first.atlas == second.atlas, "round-trip: atlas state identical");

    // Canonical form: the emitted document is itself a fixed point.
    const ply::UIAtlasLoadResult third = ply::loadUIAtlasFromString(emittedTwice);
    check(third.ok(), "round-trip: second emit reloads");
    check(third.ok() && ply::emitUIAtlas(third.atlas) == emittedTwice,
          "round-trip: emit is a fixed point");
}

void testKeyOrderDoesNotMatter() {
    // Same content, different key order and different region order: the loader
    // canonicalises, so both emit the same bytes.
    const std::string first = R"({
      "image": "ui.png",
      "regions": {
        "a": {"x": 0, "y": 0, "w": 8, "h": 8},
        "b": {"w": 16, "h": 16, "y": 8, "x": 8}
      }
    })";
    const std::string second = R"({
      "regions": {
        "b": {"x": 8, "y": 8, "w": 16, "h": 16},
        "a": {"y": 0, "h": 8, "x": 0, "w": 8}
      },
      "image": "ui.png"
    })";

    const ply::UIAtlasLoadResult a = ply::loadUIAtlasFromString(first);
    const ply::UIAtlasLoadResult b = ply::loadUIAtlasFromString(second);
    check(a.ok() && b.ok(), "key-order fixtures load");
    if (!a.ok() || !b.ok()) return;
    check(a.atlas == b.atlas, "key order does not change atlas state");
    checkString(ply::emitUIAtlas(a.atlas), ply::emitUIAtlas(b.atlas),
                "key order does not change emitted JSON");
}

void testEmitIsOrderIndependent() {
    // Same content, opposite vector order: emit must not depend on it.
    ply::UIAtlas forward;
    forward.image = "ui.png";
    forward.regions = {ply::UIAtlasRegion{"a", 0, 0, 8, 8},
                       ply::UIAtlasRegion{"b", 8, 0, 8, 8},
                       ply::UIAtlasRegion{"c", 16, 0, 8, 8}};
    forward.animations = {ply::UIAtlasAnimation{"one", {"a"}, {100}},
                          ply::UIAtlasAnimation{"two", {"b"}, {100}}};
    forward.fonts = {ply::UIAtlasFont{"aa", "aa.ttf", {10}},
                     ply::UIAtlasFont{"bb", "bb.ttf", {20}}};

    ply::UIAtlas backward = forward;
    std::reverse(backward.regions.begin(), backward.regions.end());
    std::reverse(backward.animations.begin(), backward.animations.end());
    std::reverse(backward.fonts.begin(), backward.fonts.end());

    check(ply::validateUIAtlas(forward).ok(), "order-independent atlas is valid");
    const std::string first = ply::emitUIAtlas(forward);
    const std::string second = ply::emitUIAtlas(backward);
    checkString(first, second, "emit ignores struct order");

    const ply::UIAtlasLoadResult reloaded = ply::loadUIAtlasFromString(second);
    check(reloaded.ok(), "order-independent atlas reloads");
    check(reloaded.ok() && ply::emitUIAtlas(reloaded.atlas) == first,
          "emit order-independent through a reload");
}

// --- E-a1.2: named errors --------------------------------------------------

void testNamedErrors() {
    // Malformed input.
    expectError("malformed json", "{ not json", ply::UIAtlasErrorCode::MalformedJson);
    expectError("top-level array", "[]", ply::UIAtlasErrorCode::MalformedJson);
    expectError("missing image",
                R"({"regions": {}})", ply::UIAtlasErrorCode::MissingField);
    expectError("wrong-typed image", R"({"image": 3})", ply::UIAtlasErrorCode::WrongType);
    expectError("empty image name", R"({"image": ""})", ply::UIAtlasErrorCode::MissingField);
    expectError("regions given as an array",
                R"({"image": "ui.png", "regions": []})", ply::UIAtlasErrorCode::WrongType);
    expectError("font entry given as a string",
                R"({"image": "ui.png", "fonts": {"ui": "ui.ttf"}})",
                ply::UIAtlasErrorCode::WrongType);
    expectError("wrong-typed coordinate",
                R"({"image": "ui.png", "regions": {"a": {"x": "0", "y": 0, "w": 8, "h": 8}}})",
                ply::UIAtlasErrorCode::WrongType);

    // Region geometry.
    expectError("zero-width region",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 0, "h": 8}})"),
                ply::UIAtlasErrorCode::NonPositiveSize);
    expectError("negative position",
                validAtlas(R"({"a": {"x": -1, "y": 0, "w": 8, "h": 8}})"),
                ply::UIAtlasErrorCode::NegativePosition);
    expectError("hotspot outside the region",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8, "hotspot": [8, 0]}})"),
                ply::UIAtlasErrorCode::HotspotOutOfBounds);

    // Margins.
    expectError("negative margin",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [-4, 4, 4, 4], "margins_source": "declared"}})"),
                ply::UIAtlasErrorCode::NegativeMargin);
    expectError("margin wider than half the sprite",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [30, 4, 4, 4], "margins_source": "declared"}})"),
                ply::UIAtlasErrorCode::MarginTooLarge);
    expectError("margins leave no centre column",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [24, 4, 24, 4], "margins_source": "declared"}})"),
                ply::UIAtlasErrorCode::MarginTooLarge);
    expectError("margins leave no centre row",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [4, 24, 4, 24], "margins_source": "declared"}})"),
                ply::UIAtlasErrorCode::MarginTooLarge);
    expectError("three margins instead of four",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [4, 4, 4], "margins_source": "declared"}})"),
                ply::UIAtlasErrorCode::WrongType);
    expectError("unknown margins_source",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [4, 4, 4, 4], "margins_source": "guessed"}})"),
                ply::UIAtlasErrorCode::UnknownMarginsSource);
    expectError("derived margins without values",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins_source": "derived"}})"),
                ply::UIAtlasErrorCode::MissingField);

    // Slice and stretch modes.
    expectError("unknown stretch mode",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "margins": [4, 4, 4, 4],
                                     "stretch": {"edges": "stretchh"}}})"),
                ply::UIAtlasErrorCode::UnknownStretchMode);
    expectError("unknown centre stretch mode",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 48, "h": 48,
                                     "stretch": {"centre": "repeat"}}})"),
                ply::UIAtlasErrorCode::UnknownStretchMode);
    expectError("stretch given as a string",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8, "stretch": "tile"}})"),
                ply::UIAtlasErrorCode::WrongType);
    expectError("unknown slice mode",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8, "slice": "nine_slice"}})"),
                ply::UIAtlasErrorCode::UnknownSliceMode);

    // Animations.
    expectError("animation frame references a missing region",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8}})",
                           R"({"next-arrow": {"frames": ["a", "arrow.9"], "duration_ms": [120, 120]}})"),
                ply::UIAtlasErrorCode::MissingRegion);
    expectError("animation without frames",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8}})",
                           R"({"next-arrow": {"frames": [], "duration_ms": []}})"),
                ply::UIAtlasErrorCode::EmptyAnimation);
    expectError("frame count differs from duration count",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8}})",
                           R"({"next-arrow": {"frames": ["a", "a"], "duration_ms": [120]}})"),
                ply::UIAtlasErrorCode::FrameCountMismatch);
    expectError("negative animation duration",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8}})",
                           R"({"next-arrow": {"frames": ["a"], "duration_ms": [-1]}})"),
                ply::UIAtlasErrorCode::NegativeDuration);
    expectError("animation without durations",
                validAtlas(R"({"a": {"x": 0, "y": 0, "w": 8, "h": 8}})",
                           R"({"next-arrow": {"frames": ["a"]}})"),
                ply::UIAtlasErrorCode::MissingField);

    // Fonts.
    expectError("font without sizes",
                R"({"image": "ui.png", "fonts": {"ui": {"file": "ui.ttf", "sizes": []}}})",
                ply::UIAtlasErrorCode::EmptyFontSizes);
    expectError("non-positive font size",
                R"({"image": "ui.png", "fonts": {"ui": {"file": "ui.ttf", "sizes": [10, 0]}}})",
                ply::UIAtlasErrorCode::NonPositiveFontSize);
    expectError("font without a file",
                R"({"image": "ui.png", "fonts": {"ui": {"sizes": [10]}}})",
                ply::UIAtlasErrorCode::MissingField);

    // I/O.
    const ply::UIAtlasLoadResult missing =
        ply::loadUIAtlasFile(std::string(PLY_TEST_DATA_DIR) + "/no-such-atlas.json");
    check(!missing.ok() && missing.error.code == ply::UIAtlasErrorCode::FileNotReadable,
          "unreadable file reports FileNotReadable");

    // The detail has to name the offending entry, not just the class of fault.
    const ply::UIAtlasLoadResult named = ply::loadUIAtlasFromString(
        validAtlas(R"({"panel.wood": {"x": 0, "y": 0, "w": 48, "h": 48,
                                      "margins": [30, 4, 4, 4], "margins_source": "declared"}})"));
    check(!named.ok(), "named-detail case fails");
    check(named.error.message.find("panel.wood") != std::string::npos,
          "margin error names the offending region");
}

// Codes that JSON cannot express (duplicate keys collapse in the parser) are
// still reachable from a hand-built atlas, so validate() must name them too.
void testValidateNamesProgrammaticFaults() {
    ply::UIAtlas atlas;
    atlas.image = "ui.png";
    atlas.regions.push_back(ply::UIAtlasRegion{"a", 0, 0, 8, 8});
    check(ply::validateUIAtlas(atlas).ok(), "hand-built atlas validates");

    ply::UIAtlas duplicated = atlas;
    duplicated.regions.push_back(ply::UIAtlasRegion{"a", 8, 0, 8, 8});
    check(ply::validateUIAtlas(duplicated).code == ply::UIAtlasErrorCode::DuplicateRegion,
          "duplicate region named");

    ply::UIAtlas duplicateAnimation = atlas;
    duplicateAnimation.animations.push_back(
        ply::UIAtlasAnimation{"spin", {"a"}, {100}});
    duplicateAnimation.animations.push_back(
        ply::UIAtlasAnimation{"spin", {"a"}, {200}});
    check(ply::validateUIAtlas(duplicateAnimation).code ==
              ply::UIAtlasErrorCode::DuplicateAnimation,
          "duplicate animation named");

    ply::UIAtlas duplicateFont = atlas;
    duplicateFont.fonts.push_back(ply::UIAtlasFont{"ui", "ui.ttf", {10}});
    duplicateFont.fonts.push_back(ply::UIAtlasFont{"ui", "ui.ttf", {20}});
    check(ply::validateUIAtlas(duplicateFont).code == ply::UIAtlasErrorCode::DuplicateFont,
          "duplicate font named");

    ply::UIAtlas unnamed = atlas;
    unnamed.regions.push_back(ply::UIAtlasRegion{"", 0, 0, 8, 8});
    check(ply::validateUIAtlas(unnamed).code == ply::UIAtlasErrorCode::EmptyRegionName,
          "empty region name named");

    ply::UIAtlas badMargins = atlas;
    badMargins.regions.push_back(ply::UIAtlasRegion{"b", 0, 0, 48, 48, true,
                                                    ply::UIMargins{24, 0, 24, 0}});
    check(ply::validateUIAtlas(badMargins).code == ply::UIAtlasErrorCode::MarginTooLarge,
          "programmatic margin fault named");

    // Every code has a distinct, published token; the sentinel is "None".
    const std::vector<ply::UIAtlasErrorCode> codes = {
        ply::UIAtlasErrorCode::None,
        ply::UIAtlasErrorCode::FileNotReadable,
        ply::UIAtlasErrorCode::MalformedJson,
        ply::UIAtlasErrorCode::MissingField,
        ply::UIAtlasErrorCode::WrongType,
        ply::UIAtlasErrorCode::EmptyRegionName,
        ply::UIAtlasErrorCode::DuplicateRegion,
        ply::UIAtlasErrorCode::NegativePosition,
        ply::UIAtlasErrorCode::NonPositiveSize,
        ply::UIAtlasErrorCode::NegativeMargin,
        ply::UIAtlasErrorCode::MarginTooLarge,
        ply::UIAtlasErrorCode::UnknownMarginsSource,
        ply::UIAtlasErrorCode::UnknownStretchMode,
        ply::UIAtlasErrorCode::UnknownSliceMode,
        ply::UIAtlasErrorCode::HotspotOutOfBounds,
        ply::UIAtlasErrorCode::EmptyAnimation,
        ply::UIAtlasErrorCode::DuplicateAnimation,
        ply::UIAtlasErrorCode::FrameCountMismatch,
        ply::UIAtlasErrorCode::MissingRegion,
        ply::UIAtlasErrorCode::NegativeDuration,
        ply::UIAtlasErrorCode::DuplicateFont,
        ply::UIAtlasErrorCode::EmptyFontSizes,
        ply::UIAtlasErrorCode::NonPositiveFontSize,
    };
    checkString(ply::toString(ply::UIAtlasErrorCode::None), "None", "None token");
    std::vector<std::string> tokens;
    for (const ply::UIAtlasErrorCode code : codes) {
        const std::string token = ply::toString(code);
        check(token != "Unknown", "every code has a token");
        check(std::find(tokens.begin(), tokens.end(), token) == tokens.end(),
              "error tokens are distinct");
        tokens.push_back(token);
    }
}

}  // namespace

int main() {
    testFixtureLoads();
    testRoundTrip();
    testKeyOrderDoesNotMatter();
    testEmitIsOrderIndependent();
    testNamedErrors();
    testValidateNamesProgrammaticFaults();

    if (failures == 0) {
        std::printf("ui_atlas tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_atlas tests: %d failure(s)\n", failures);
    return 1;
}
