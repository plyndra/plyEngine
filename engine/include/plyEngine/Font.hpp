#pragma once

#include <string>
#include <vector>

namespace ply {

/// A TTF font baked into a single alpha texture atlas at a fixed pixel height.
///
/// Glyphs cover printable ASCII (32..126). The atlas is RGBA8 with white RGB
/// and the coverage in alpha, so tinting works through the sprite shader.
class Font {
public:
    struct Glyph {
        /// Atlas pixel rect (valid when w>0 and h>0; space has an empty rect).
        int x = 0, y = 0, w = 0, h = 0;
        /// Offsets from the pen position to the glyph's top-left, in pixels.
        float xoff = 0.0f, yoff = 0.0f;
        /// Horizontal advance in pixels.
        float xadvance = 0.0f;
    };

    Font() = default;

    /// Bakes the font at `pixelHeight`. Returns false on failure.
    bool loadFromFile(const std::string& path, int pixelHeight);
    bool loadFromMemory(const void* data, size_t size, int pixelHeight);

    bool valid() const { return atlasWidth_ > 0 && !glyphs_.empty(); }

    int atlasWidth() const { return atlasWidth_; }
    int atlasHeight() const { return atlasHeight_; }
    const unsigned char* atlasPixels() const { return atlasPixels_.data(); }

    int pixelHeight() const { return pixelHeight_; }
    float ascent() const { return ascent_; }
    float descent() const { return descent_; }
    float lineHeight() const { return lineHeight_; }

    /// Metrics for a printable-ASCII codepoint, or nullptr when unsupported.
    const Glyph* glyph(int codepoint) const;

    /// Advance width of a single-line string, in pixels.
    float measure(const std::string& text) const;

    static constexpr int kFirstChar = 32;
    static constexpr int kCharCount = 95;  // 32..126 inclusive

private:
    bool bake(const unsigned char* ttf, int length, int pixelHeight);

    int atlasWidth_ = 0;
    int atlasHeight_ = 0;
    int pixelHeight_ = 0;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineHeight_ = 0.0f;
    std::vector<Glyph> glyphs_;
    std::vector<unsigned char> atlasPixels_;
};

}  // namespace ply
