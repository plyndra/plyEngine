#include "plyEngine/UIAtlas.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ply {

const char* toString(UIAtlasErrorCode code) {
    switch (code) {
        case UIAtlasErrorCode::None: return "None";
        case UIAtlasErrorCode::FileNotReadable: return "FileNotReadable";
        case UIAtlasErrorCode::MalformedJson: return "MalformedJson";
        case UIAtlasErrorCode::MissingField: return "MissingField";
        case UIAtlasErrorCode::WrongType: return "WrongType";
        case UIAtlasErrorCode::EmptyRegionName: return "EmptyRegionName";
        case UIAtlasErrorCode::DuplicateRegion: return "DuplicateRegion";
        case UIAtlasErrorCode::NegativePosition: return "NegativePosition";
        case UIAtlasErrorCode::NonPositiveSize: return "NonPositiveSize";
        case UIAtlasErrorCode::NegativeMargin: return "NegativeMargin";
        case UIAtlasErrorCode::MarginTooLarge: return "MarginTooLarge";
        case UIAtlasErrorCode::UnknownMarginsSource: return "UnknownMarginsSource";
        case UIAtlasErrorCode::UnknownStretchMode: return "UnknownStretchMode";
        case UIAtlasErrorCode::UnknownSliceMode: return "UnknownSliceMode";
        case UIAtlasErrorCode::HotspotOutOfBounds: return "HotspotOutOfBounds";
        case UIAtlasErrorCode::EmptyAnimation: return "EmptyAnimation";
        case UIAtlasErrorCode::DuplicateAnimation: return "DuplicateAnimation";
        case UIAtlasErrorCode::FrameCountMismatch: return "FrameCountMismatch";
        case UIAtlasErrorCode::MissingRegion: return "MissingRegion";
        case UIAtlasErrorCode::NegativeDuration: return "NegativeDuration";
        case UIAtlasErrorCode::DuplicateFont: return "DuplicateFont";
        case UIAtlasErrorCode::EmptyFontSizes: return "EmptyFontSizes";
        case UIAtlasErrorCode::NonPositiveFontSize: return "NonPositiveFontSize";
    }
    return "Unknown";
}

const UIAtlasRegion* UIAtlas::findRegion(std::string_view name) const {
    // Linear: atlases are tens to low hundreds of entries, and a linear scan
    // stays correct for a hand-built atlas whose regions are not sorted yet.
    for (const UIAtlasRegion& region : regions) {
        if (std::string_view(region.name) == name) return &region;
    }
    return nullptr;
}

namespace {

using json = nlohmann::json;
using ojson = nlohmann::ordered_json;

UIAtlasError makeError(UIAtlasErrorCode code, std::string detail) {
    return UIAtlasError{code, std::string(toString(code)) + ": " + std::move(detail)};
}

const char* sliceToken(SliceMode mode) {
    switch (mode) {
        case SliceMode::Nine: return "nine";
        case SliceMode::ThreeH: return "three_h";
        case SliceMode::ThreeV: return "three_v";
        case SliceMode::Plain: return "plain";
    }
    return "nine";
}

const char* stretchToken(StretchMode mode) {
    return mode == StretchMode::Tile ? "tile" : "stretch";
}

std::string quoted(const std::string& text) { return "\"" + text + "\""; }

std::string marginsText(const UIMargins& m) {
    return "[" + std::to_string(m.left) + ", " + std::to_string(m.top) + ", " +
           std::to_string(m.right) + ", " + std::to_string(m.bottom) + "]";
}

// --- typed field readers ----------------------------------------------------
// Each returns false and fills `error` when the key is absent or has the wrong
// shape, so the caller can propagate a named error without exceptions.

bool readInt(const json& obj, const char* key, const std::string& context, int& out,
             UIAtlasError& error) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        error = makeError(UIAtlasErrorCode::MissingField, quoted(key) + " in " + context);
        return false;
    }
    if (it->is_number_unsigned()) {
        const std::uint64_t value = it->get<std::uint64_t>();
        if (value > 2147483647ull) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted(key) + " in " + context + " is outside the 32-bit range");
            return false;
        }
        out = static_cast<int>(value);
        return true;
    }
    if (it->is_number_integer()) {
        const std::int64_t value = it->get<std::int64_t>();
        if (value < -2147483647ll - 1 || value > 2147483647ll) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted(key) + " in " + context + " is outside the 32-bit range");
            return false;
        }
        out = static_cast<int>(value);
        return true;
    }
    error = makeError(UIAtlasErrorCode::WrongType,
                      quoted(key) + " in " + context + " must be an integer");
    return false;
}

bool readString(const json& obj, const char* key, const std::string& context, std::string& out,
                UIAtlasError& error) {
    const auto it = obj.find(key);
    if (it == obj.end()) {
        error = makeError(UIAtlasErrorCode::MissingField, quoted(key) + " in " + context);
        return false;
    }
    if (!it->is_string()) {
        error = makeError(UIAtlasErrorCode::WrongType,
                          quoted(key) + " in " + context + " must be a string");
        return false;
    }
    out = it->get<std::string>();
    return true;
}

bool readIntArray(const json& value, const std::string& context, std::vector<int>& out,
                  UIAtlasError& error) {
    if (!value.is_array()) {
        error = makeError(UIAtlasErrorCode::WrongType, context + " must be an array");
        return false;
    }
    out.clear();
    for (const json& item : value) {
        if (!item.is_number_integer()) {
            error = makeError(UIAtlasErrorCode::WrongType, context + " must contain integers");
            return false;
        }
        out.push_back(static_cast<int>(item.get<std::int64_t>()));
    }
    return true;
}

bool readMargins(const json& value, const std::string& context, UIMargins& out,
                 UIAtlasError& error) {
    std::vector<int> values;
    if (!readIntArray(value, context, values, error)) return false;
    if (values.size() != 4) {
        error = makeError(UIAtlasErrorCode::WrongType,
                          context + " must have exactly 4 entries (left, top, right, bottom)");
        return false;
    }
    out = UIMargins{values[0], values[1], values[2], values[3]};
    return true;
}

bool readStretchMode(const json& value, const std::string& context, StretchMode& out,
                     UIAtlasError& error) {
    if (!value.is_string()) {
        error = makeError(UIAtlasErrorCode::WrongType, context + " must be a string");
        return false;
    }
    const std::string text = value.get<std::string>();
    if (text == "stretch") {
        out = StretchMode::Stretch;
        return true;
    }
    if (text == "tile") {
        out = StretchMode::Tile;
        return true;
    }
    error = makeError(UIAtlasErrorCode::UnknownStretchMode,
                      quoted(text) + " in " + context + " (expected \"stretch\" or \"tile\")");
    return false;
}

// --- parsing ---------------------------------------------------------------

bool parseRegion(const std::string& name, const json& entry, UIAtlasRegion& out,
                 UIAtlasError& error) {
    const std::string context = "region " + quoted(name);
    if (!entry.is_object()) {
        error = makeError(UIAtlasErrorCode::WrongType, context + " must be an object");
        return false;
    }

    out = UIAtlasRegion{};
    out.name = name;
    if (!readInt(entry, "x", context, out.x, error)) return false;
    if (!readInt(entry, "y", context, out.y, error)) return false;
    if (!readInt(entry, "w", context, out.w, error)) return false;
    if (!readInt(entry, "h", context, out.h, error)) return false;

    if (const auto it = entry.find("margins"); it != entry.end()) {
        if (!readMargins(*it, quoted("margins") + " in " + context, out.margins, error))
            return false;
        out.hasMargins = true;
    }

    if (const auto it = entry.find("margins_source"); it != entry.end()) {
        if (!it->is_string()) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted("margins_source") + " in " + context + " must be a string");
            return false;
        }
        const std::string text = it->get<std::string>();
        if (text == "declared") {
            out.marginSource = MarginSource::Declared;
        } else if (text == "derived") {
            out.marginSource = MarginSource::Derived;
        } else {
            error = makeError(UIAtlasErrorCode::UnknownMarginsSource,
                              quoted(text) + " in " + context +
                                  " (expected \"declared\" or \"derived\")");
            return false;
        }
    }

    if (const auto it = entry.find("slice"); it != entry.end()) {
        if (!it->is_string()) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted("slice") + " in " + context + " must be a string");
            return false;
        }
        const std::string text = it->get<std::string>();
        if (text == "nine") {
            out.slice = SliceMode::Nine;
        } else if (text == "three_h") {
            out.slice = SliceMode::ThreeH;
        } else if (text == "three_v") {
            out.slice = SliceMode::ThreeV;
        } else if (text == "plain") {
            out.slice = SliceMode::Plain;
        } else {
            error = makeError(UIAtlasErrorCode::UnknownSliceMode,
                              quoted(text) + " in " + context +
                                  " (expected \"nine\", \"three_h\", \"three_v\" or \"plain\")");
            return false;
        }
    }

    if (const auto it = entry.find("stretch"); it != entry.end()) {
        if (!it->is_object()) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted("stretch") + " in " + context + " must be an object");
            return false;
        }
        if (const auto edges = it->find("edges"); edges != it->end()) {
            if (!readStretchMode(*edges, quoted("stretch.edges") + " in " + context, out.edges,
                                 error))
                return false;
        }
        if (const auto centre = it->find("centre"); centre != it->end()) {
            if (!readStretchMode(*centre, quoted("stretch.centre") + " in " + context,
                                 out.centre, error))
                return false;
        }
    }

    if (const auto it = entry.find("hotspot"); it != entry.end()) {
        std::vector<int> point;
        if (!readIntArray(*it, quoted("hotspot") + " in " + context, point, error)) return false;
        if (point.size() != 2) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted("hotspot") + " in " + context + " must be [x, y]");
            return false;
        }
        out.hasHotspot = true;
        out.hotspotX = point[0];
        out.hotspotY = point[1];
    }

    return true;
}

bool parseAnimation(const std::string& name, const json& entry, UIAtlasAnimation& out,
                    UIAtlasError& error) {
    const std::string context = "animation " + quoted(name);
    if (!entry.is_object()) {
        error = makeError(UIAtlasErrorCode::WrongType, context + " must be an object");
        return false;
    }

    out = UIAtlasAnimation{};
    out.name = name;

    const auto frames = entry.find("frames");
    if (frames == entry.end()) {
        error = makeError(UIAtlasErrorCode::MissingField, quoted("frames") + " in " + context);
        return false;
    }
    if (!frames->is_array()) {
        error = makeError(UIAtlasErrorCode::WrongType,
                          quoted("frames") + " in " + context + " must be an array");
        return false;
    }
    for (const json& frame : *frames) {
        if (!frame.is_string()) {
            error = makeError(UIAtlasErrorCode::WrongType,
                              quoted("frames") + " in " + context + " must contain strings");
            return false;
        }
        out.frames.push_back(frame.get<std::string>());
    }

    const auto durations = entry.find("duration_ms");
    if (durations == entry.end()) {
        error = makeError(UIAtlasErrorCode::MissingField,
                          quoted("duration_ms") + " in " + context);
        return false;
    }
    if (!readIntArray(*durations, quoted("duration_ms") + " in " + context, out.durationMs,
                      error))
        return false;

    return true;
}

bool parseFont(const std::string& name, const json& entry, UIAtlasFont& out,
               UIAtlasError& error) {
    const std::string context = "font " + quoted(name);
    if (!entry.is_object()) {
        error = makeError(UIAtlasErrorCode::WrongType, context + " must be an object");
        return false;
    }

    out = UIAtlasFont{};
    out.name = name;
    if (!readString(entry, "file", context, out.file, error)) return false;

    const auto sizes = entry.find("sizes");
    if (sizes == entry.end()) {
        error = makeError(UIAtlasErrorCode::MissingField, quoted("sizes") + " in " + context);
        return false;
    }
    return readIntArray(*sizes, quoted("sizes") + " in " + context, out.sizes, error);
}

UIAtlasLoadResult fail(UIAtlasError error) {
    UIAtlasLoadResult result;
    result.error = std::move(error);
    return result;
}

}  // namespace

UIAtlasLoadResult loadUIAtlasFromString(std::string_view text) {
    json doc;
    try {
        doc = json::parse(text.begin(), text.end());
    } catch (const std::exception& exception) {
        return fail(makeError(UIAtlasErrorCode::MalformedJson, exception.what()));
    }
    if (!doc.is_object()) {
        return fail(makeError(UIAtlasErrorCode::MalformedJson, "top level must be an object"));
    }

    UIAtlas atlas;

    const auto image = doc.find("image");
    if (image == doc.end()) {
        return fail(makeError(UIAtlasErrorCode::MissingField, "\"image\""));
    }
    if (!image->is_string()) {
        return fail(makeError(UIAtlasErrorCode::WrongType, "\"image\" must be a string"));
    }
    atlas.image = image->get<std::string>();

    if (const auto palette = doc.find("palette"); palette != doc.end()) {
        if (!palette->is_string()) {
            return fail(makeError(UIAtlasErrorCode::WrongType, "\"palette\" must be a string"));
        }
        atlas.palette = palette->get<std::string>();
    }

    if (const auto regions = doc.find("regions"); regions != doc.end()) {
        if (!regions->is_object()) {
            return fail(makeError(UIAtlasErrorCode::WrongType, "\"regions\" must be an object"));
        }
        for (const auto& [name, entry] : regions->items()) {
            UIAtlasRegion region;
            UIAtlasError error;
            if (!parseRegion(name, entry, region, error)) return fail(std::move(error));
            atlas.regions.push_back(std::move(region));
        }
    }

    if (const auto animations = doc.find("animations"); animations != doc.end()) {
        if (!animations->is_object()) {
            return fail(
                makeError(UIAtlasErrorCode::WrongType, "\"animations\" must be an object"));
        }
        for (const auto& [name, entry] : animations->items()) {
            UIAtlasAnimation animation;
            UIAtlasError error;
            if (!parseAnimation(name, entry, animation, error)) return fail(std::move(error));
            atlas.animations.push_back(std::move(animation));
        }
    }

    if (const auto fonts = doc.find("fonts"); fonts != doc.end()) {
        if (!fonts->is_object()) {
            return fail(makeError(UIAtlasErrorCode::WrongType, "\"fonts\" must be an object"));
        }
        for (const auto& [name, entry] : fonts->items()) {
            UIAtlasFont font;
            UIAtlasError error;
            if (!parseFont(name, entry, font, error)) return fail(std::move(error));
            atlas.fonts.push_back(std::move(font));
        }
    }

    // Canonical order so state comparison is content-only. nlohmann's `json` is a
    // std::map, so this is already true today; the sort keeps it true if the
    // parser ever preserves document order.
    const auto byName = [](const auto& a, const auto& b) { return a.name < b.name; };
    std::sort(atlas.regions.begin(), atlas.regions.end(), byName);
    std::sort(atlas.animations.begin(), atlas.animations.end(), byName);
    std::sort(atlas.fonts.begin(), atlas.fonts.end(), byName);

    UIAtlasError error = validateUIAtlas(atlas);
    if (!error.ok()) return fail(std::move(error));

    UIAtlasLoadResult result;
    result.atlas = std::move(atlas);
    return result;
}

UIAtlasLoadResult loadUIAtlasFile(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return fail(makeError(UIAtlasErrorCode::FileNotReadable, quoted(path)));
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    if (stream.bad()) {
        return fail(makeError(UIAtlasErrorCode::FileNotReadable, quoted(path) + " (read error)"));
    }
    return loadUIAtlasFromString(buffer.str());
}

UIAtlasError validateUIAtlas(const UIAtlas& atlas) {
    if (atlas.image.empty()) {
        return makeError(UIAtlasErrorCode::MissingField, "\"image\" is empty");
    }

    std::vector<std::string> seenRegions;
    for (const UIAtlasRegion& region : atlas.regions) {
        if (region.name.empty()) {
            return makeError(UIAtlasErrorCode::EmptyRegionName, "a region has an empty name");
        }
        if (std::find(seenRegions.begin(), seenRegions.end(), region.name) != seenRegions.end()) {
            return makeError(UIAtlasErrorCode::DuplicateRegion, "region " + quoted(region.name));
        }
        seenRegions.push_back(region.name);

        if (region.x < 0 || region.y < 0) {
            return makeError(UIAtlasErrorCode::NegativePosition,
                             "region " + quoted(region.name) + " at (" +
                                 std::to_string(region.x) + ", " + std::to_string(region.y) + ")");
        }
        if (region.w <= 0 || region.h <= 0) {
            return makeError(UIAtlasErrorCode::NonPositiveSize,
                             "region " + quoted(region.name) + " is " +
                                 std::to_string(region.w) + "x" + std::to_string(region.h));
        }

        if (region.hasMargins) {
            const UIMargins& m = region.margins;
            if (m.left < 0 || m.top < 0 || m.right < 0 || m.bottom < 0) {
                return makeError(UIAtlasErrorCode::NegativeMargin,
                                 "region " + quoted(region.name) + " margins " + marginsText(m));
            }
            // A margin wider than half the sprite leaves no centre to slice, and
            // left+right == w leaves a zero-width centre that cannot tile.
            if (m.left > region.w / 2 || m.right > region.w / 2 ||
                m.left + m.right >= region.w) {
                return makeError(UIAtlasErrorCode::MarginTooLarge,
                                 "region " + quoted(region.name) + " horizontal margins [" +
                                     std::to_string(m.left) + ", " + std::to_string(m.right) +
                                     "] against width " + std::to_string(region.w));
            }
            if (m.top > region.h / 2 || m.bottom > region.h / 2 ||
                m.top + m.bottom >= region.h) {
                return makeError(UIAtlasErrorCode::MarginTooLarge,
                                 "region " + quoted(region.name) + " vertical margins [" +
                                     std::to_string(m.top) + ", " + std::to_string(m.bottom) +
                                     "] against height " + std::to_string(region.h));
            }
        } else if (region.marginSource == MarginSource::Derived) {
            return makeError(UIAtlasErrorCode::MissingField,
                             "\"margins\" in region " + quoted(region.name) +
                                 " (margins_source is \"derived\")");
        }

        if (region.hasHotspot) {
            if (region.hotspotX < 0 || region.hotspotY < 0 || region.hotspotX >= region.w ||
                region.hotspotY >= region.h) {
                return makeError(UIAtlasErrorCode::HotspotOutOfBounds,
                                 "region " + quoted(region.name) + " hotspot (" +
                                     std::to_string(region.hotspotX) + ", " +
                                     std::to_string(region.hotspotY) + ") outside " +
                                     std::to_string(region.w) + "x" +
                                     std::to_string(region.h));
            }
        }
    }

    std::vector<std::string> seenAnimations;
    for (const UIAtlasAnimation& animation : atlas.animations) {
        if (std::find(seenAnimations.begin(), seenAnimations.end(), animation.name) !=
            seenAnimations.end()) {
            return makeError(UIAtlasErrorCode::DuplicateAnimation,
                             "animation " + quoted(animation.name));
        }
        seenAnimations.push_back(animation.name);

        if (animation.frames.empty()) {
            return makeError(UIAtlasErrorCode::EmptyAnimation,
                             "animation " + quoted(animation.name) + " has no frames");
        }
        if (animation.frames.size() != animation.durationMs.size()) {
            return makeError(UIAtlasErrorCode::FrameCountMismatch,
                             "animation " + quoted(animation.name) + " has " +
                                 std::to_string(animation.frames.size()) + " frames but " +
                                 std::to_string(animation.durationMs.size()) + " durations");
        }
        for (std::size_t i = 0; i < animation.frames.size(); ++i) {
            if (atlas.findRegion(animation.frames[i]) == nullptr) {
                return makeError(UIAtlasErrorCode::MissingRegion,
                                 "animation " + quoted(animation.name) + " frame " +
                                     std::to_string(i) + " references unknown region " +
                                     quoted(animation.frames[i]));
            }
            if (animation.durationMs[i] < 0) {
                return makeError(UIAtlasErrorCode::NegativeDuration,
                                 "animation " + quoted(animation.name) + " frame " +
                                     std::to_string(i) + " duration " +
                                     std::to_string(animation.durationMs[i]));
            }
        }
    }

    std::vector<std::string> seenFonts;
    for (const UIAtlasFont& font : atlas.fonts) {
        if (std::find(seenFonts.begin(), seenFonts.end(), font.name) != seenFonts.end()) {
            return makeError(UIAtlasErrorCode::DuplicateFont, "font " + quoted(font.name));
        }
        seenFonts.push_back(font.name);

        if (font.file.empty()) {
            return makeError(UIAtlasErrorCode::MissingField,
                             "\"file\" in font " + quoted(font.name));
        }
        if (font.sizes.empty()) {
            return makeError(UIAtlasErrorCode::EmptyFontSizes,
                             "font " + quoted(font.name) + " has no sizes");
        }
        for (const int size : font.sizes) {
            if (size <= 0) {
                return makeError(UIAtlasErrorCode::NonPositiveFontSize,
                                 "font " + quoted(font.name) + " size " + std::to_string(size));
            }
        }
    }

    return {};
}

std::string emitUIAtlas(const UIAtlas& atlas) {
    // ordered_json so the document reads in schema order; determinism comes
    // from emitting entries in name order, whatever order the caller put them in.
    const auto byName = [](const auto& a, const auto& b) { return a.name < b.name; };
    auto regionsByName = atlas.regions;
    std::sort(regionsByName.begin(), regionsByName.end(), byName);
    auto animationsByName = atlas.animations;
    std::sort(animationsByName.begin(), animationsByName.end(), byName);
    auto fontsByName = atlas.fonts;
    std::sort(fontsByName.begin(), fontsByName.end(), byName);

    ojson doc = ojson::object();
    doc["image"] = atlas.image;
    if (!atlas.palette.empty()) doc["palette"] = atlas.palette;

    if (!regionsByName.empty()) {
        ojson regions = ojson::object();
        for (const UIAtlasRegion& region : regionsByName) {
            ojson entry = ojson::object();
            entry["x"] = region.x;
            entry["y"] = region.y;
            entry["w"] = region.w;
            entry["h"] = region.h;
            if (region.hasMargins) {
                entry["margins"] =
                    ojson::array({region.margins.left, region.margins.top, region.margins.right,
                                  region.margins.bottom});
                entry["margins_source"] =
                    region.marginSource == MarginSource::Declared ? "declared" : "derived";
            }
            if (region.slice != SliceMode::Nine) entry["slice"] = sliceToken(region.slice);
            if (region.edges != StretchMode::Stretch || region.centre != StretchMode::Stretch) {
                ojson stretch = ojson::object();
                if (region.edges != StretchMode::Stretch)
                    stretch["edges"] = stretchToken(region.edges);
                if (region.centre != StretchMode::Stretch)
                    stretch["centre"] = stretchToken(region.centre);
                entry["stretch"] = std::move(stretch);
            }
            if (region.hasHotspot) {
                entry["hotspot"] = ojson::array({region.hotspotX, region.hotspotY});
            }
            regions[region.name] = std::move(entry);
        }
        doc["regions"] = std::move(regions);
    }

    if (!animationsByName.empty()) {
        ojson animations = ojson::object();
        for (const UIAtlasAnimation& animation : animationsByName) {
            ojson entry = ojson::object();
            entry["frames"] = animation.frames;
            entry["duration_ms"] = animation.durationMs;
            animations[animation.name] = std::move(entry);
        }
        doc["animations"] = std::move(animations);
    }

    if (!fontsByName.empty()) {
        ojson fonts = ojson::object();
        for (const UIAtlasFont& font : fontsByName) {
            ojson entry = ojson::object();
            entry["file"] = font.file;
            entry["sizes"] = font.sizes;
            fonts[font.name] = std::move(entry);
        }
        doc["fonts"] = std::move(fonts);
    }

    return doc.dump(2) + "\n";
}

}  // namespace ply
