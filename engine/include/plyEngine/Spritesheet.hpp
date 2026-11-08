#pragma once

#include <string>
#include <vector>

namespace ply {

/// A named rectangle in a spritesheet image, in pixels with (0,0) top-left.
struct NamedRect {
    std::string name;
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    /// Aseprite/TexturePacker can pack rotated frames. M4 records the flag but
    /// does not yet un-rotate at draw time.
    bool rotated = false;
    /// Frame was trimmed to its opaque bounds.
    bool trimmed = false;
};

/// Parses our own minimal format:
///   { "frames": [ { "name": "idle_0", "x": 0, "y": 0, "w": 32, "h": 32 } ] }
/// A top-level object map { "name": {x,y,w,h} } is also accepted.
/// Returns false and sets `error` on malformed input.
bool parseGenericSpritesheetJson(const std::string& text,
                                 std::vector<NamedRect>& out,
                                 std::string& error);

/// Parses Aseprite's JSON export (hash and array "frames" forms), including
/// frameTags (exposed via `animationTags` when provided).
///
/// WARNING: in the hash form, frames are returned in JSON object-key order
/// (which nlohmann/json keeps sorted), NOT in sheet/animation order. Consume
/// `out` by frame NAME, or use the array form if ordering matters.
bool parseAsepriteJson(const std::string& text, std::vector<NamedRect>& out,
                       std::string& error,
                       std::vector<std::string>* animationTags = nullptr);

/// Parses the TexturePacker / libGDX text atlas format.
bool parseTexturePackerAtlas(const std::string& text, std::vector<NamedRect>& out,
                             std::string& error);

}  // namespace ply
