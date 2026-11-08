#include "plyEngine/Spritesheet.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace ply {

namespace {

using json = nlohmann::json;

// Aseprite writes "idle.png" or "idle 0.aseprite"; strip a trailing extension
// so region names are usable identifiers.
std::string stripExtension(const std::string& name) {
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return name;
    }
    return name.substr(0, dot);
}

bool rectFromJson(const json& node, NamedRect& rect, std::string& error,
                  const std::string& context) {
    // Accept either a flat {x,y,w,h} or a nested {"frame":{x,y,w,h}}.
    const json* source = &node;
    if (node.contains("frame") && node["frame"].is_object()) {
        source = &node["frame"];
    }

    auto readInt = [&](const char* key, int& value) -> bool {
        if (!source->contains(key) || !(*source)[key].is_number()) {
            error = context + ": missing numeric '" + key + "'";
            return false;
        }
        value = (*source)[key].get<int>();
        return true;
    };

    if (!readInt("x", rect.x) || !readInt("y", rect.y) || !readInt("w", rect.w) ||
        !readInt("h", rect.h)) {
        return false;
    }
    if (rect.w <= 0 || rect.h <= 0) {
        error = context + ": non-positive size";
        return false;
    }
    return true;
}

}  // namespace

bool parseGenericSpritesheetJson(const std::string& text,
                                 std::vector<NamedRect>& out,
                                 std::string& error) {
    out.clear();
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        error = "invalid JSON";
        return false;
    }

    if (doc.contains("frames") && doc["frames"].is_array()) {
        const json& frames = doc["frames"];
        for (size_t i = 0; i < frames.size(); ++i) {
            const json& entry = frames[i];
            NamedRect rect;
            if (!entry.contains("name") || !entry["name"].is_string()) {
                error = "frames[" + std::to_string(i) + "]: missing 'name'";
                return false;
            }
            rect.name = entry["name"].get<std::string>();
            if (!rectFromJson(entry, rect, error,
                              "frames[" + std::to_string(i) + "]")) {
                return false;
            }
            out.push_back(std::move(rect));
        }
        return true;
    }

    if (doc.is_object()) {
        // Top-level map: { "name": {x,y,w,h} }
        bool any = false;
        for (auto it = doc.begin(); it != doc.end(); ++it) {
            NamedRect rect;
            rect.name = it.key();
            if (!rectFromJson(it.value(), rect, error, "frame '" + it.key() + "'")) {
                return false;
            }
            out.push_back(std::move(rect));
            any = true;
        }
        if (any) {
            return true;
        }
    }

    error = "no 'frames' array or frame map found";
    return false;
}

bool parseAsepriteJson(const std::string& text, std::vector<NamedRect>& out,
                       std::string& error,
                       std::vector<std::string>* animationTags) {
    out.clear();
    if (animationTags) {
        animationTags->clear();
    }

    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        error = "invalid JSON";
        return false;
    }
    if (!doc.contains("frames")) {
        error = "missing 'frames'";
        return false;
    }

    const json& frames = doc["frames"];

    // Hash form: { "sprite 0.aseprite": { "frame": {...}, "rotated": bool } }
    if (frames.is_object()) {
        for (auto it = frames.begin(); it != frames.end(); ++it) {
            NamedRect rect;
            rect.name = stripExtension(it.key());
            if (!rectFromJson(it.value(), rect, error, "frame '" + it.key() + "'")) {
                return false;
            }
            if (it.value().contains("rotated")) {
                rect.rotated = it.value()["rotated"].get<bool>();
            }
            if (it.value().contains("trimmed")) {
                rect.trimmed = it.value()["trimmed"].get<bool>();
            }
            out.push_back(std::move(rect));
        }
        // Aseprite keeps the duration/tags metadata under "meta".
        if (doc.contains("meta") && doc["meta"].is_object()) {
            const json& meta = doc["meta"];
            if (animationTags && meta.contains("frameTags") &&
                meta["frameTags"].is_array()) {
                for (const json& tag : meta["frameTags"]) {
                    if (tag.contains("name") && tag["name"].is_string()) {
                        animationTags->push_back(tag["name"].get<std::string>());
                    }
                }
            }
        }
        return true;
    }

    // Array form (Aseprite 1.3+ "array" export): [ { "filename": ..., "frame": {...} } ]
    if (frames.is_array()) {
        for (size_t i = 0; i < frames.size(); ++i) {
            const json& entry = frames[i];
            NamedRect rect;
            if (entry.contains("filename") && entry["filename"].is_string()) {
                rect.name = stripExtension(entry["filename"].get<std::string>());
            } else {
                rect.name = "frame_" + std::to_string(i);
            }
            if (!rectFromJson(entry, rect, error, "frames[" + std::to_string(i) + "]")) {
                return false;
            }
            if (entry.contains("rotated")) {
                rect.rotated = entry["rotated"].get<bool>();
            }
            if (entry.contains("trimmed")) {
                rect.trimmed = entry["trimmed"].get<bool>();
            }
            out.push_back(std::move(rect));
        }
        return true;
    }

    error = "'frames' is neither an object nor an array";
    return false;
}

bool parseTexturePackerAtlas(const std::string& text, std::vector<NamedRect>& out,
                             std::string& error) {
    out.clear();

    std::istringstream stream(text);
    std::string line;
    NamedRect current;
    bool haveCurrent = false;

    auto flush = [&]() {
        if (haveCurrent) {
            if (current.w <= 0 || current.h <= 0) {
                error = "sprite '" + current.name + "': missing or invalid size";
                return false;
            }
            out.push_back(current);
            haveCurrent = false;
        }
        return true;
    };

    while (std::getline(stream, line)) {
        // Strip trailing CR (CRLF files).
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos) {
            // Blank line separates the header/sprite blocks.
            if (!flush()) {
                return false;
            }
            continue;
        }

        const bool indented = firstNonSpace > 0;
        const std::string trimmed = line.substr(firstNonSpace);

        if (!indented) {
            // A non-indented line is either the page header (the image name,
            // which appears before any "key: value" line) or a sprite name.
            if (trimmed.find(':') == std::string::npos) {
                if (!flush()) {
                    return false;
                }
                if (out.empty() && !haveCurrent && trimmed.find('.') != std::string::npos) {
                    // Page image header; skip it.
                    continue;
                }
                current = NamedRect{};
                current.name = trimmed;
                haveCurrent = true;
                continue;
            }
        }

        if (!haveCurrent) {
            continue;  // header key/value lines
        }

        const size_t colon = trimmed.find(':');
        if (colon == std::string::npos) {
            continue;
        }
        std::string key = trimmed.substr(0, colon);
        std::string value = trimmed.substr(colon + 1);
        key.erase(std::remove_if(key.begin(), key.end(), [](unsigned char c) {
                      return std::isspace(c) != 0;
                  }),
                  key.end());
        value.erase(0, value.find_first_not_of(" \t"));

        auto parseInts = [&](int& a, int& b) -> bool {
            std::string cleaned = value;
            std::replace(cleaned.begin(), cleaned.end(), ',', ' ');
            std::istringstream vs(cleaned);
            return static_cast<bool>(vs >> a >> b);
        };

        if (key == "rotate") {
            current.rotated = (value.rfind("true", 0) == 0);
        } else if (key == "xy") {
            if (!parseInts(current.x, current.y)) {
                error = "sprite '" + current.name + "': bad xy";
                return false;
            }
        } else if (key == "size") {
            if (!parseInts(current.w, current.h)) {
                error = "sprite '" + current.name + "': bad size";
                return false;
            }
        } else if (key == "orig") {
            // Original (untrimmed) size; not needed for M4.
        }
    }

    if (!flush()) {
        return false;
    }

    if (out.empty()) {
        error = "no sprites found";
        return false;
    }
    return true;
}

}  // namespace ply
