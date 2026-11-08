#include "plyEngine/Font.hpp"

#include <stb_truetype.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace ply {

namespace {

std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file),
                                      std::istreambuf_iterator<char>());
}

}  // namespace

bool Font::loadFromFile(const std::string& path, int pixelHeight) {
    const std::vector<unsigned char> bytes = readFile(path);
    if (bytes.empty()) {
        std::fprintf(stderr, "plyEngine: could not read font '%s'\n", path.c_str());
        return false;
    }
    return loadFromMemory(bytes.data(), bytes.size(), pixelHeight);
}

bool Font::loadFromMemory(const void* data, size_t size, int pixelHeight) {
    return bake(static_cast<const unsigned char*>(data), static_cast<int>(size),
                pixelHeight);
}

bool Font::bake(const unsigned char* ttf, int length, int pixelHeight) {
    glyphs_.clear();
    atlasPixels_.clear();
    atlasWidth_ = 0;
    atlasHeight_ = 0;

    if (pixelHeight <= 0) {
        return false;
    }

    // Wide enough for the full printable ASCII set at typical sizes.
    const int bitmapW = 512;
    const int bitmapH = 512;
    std::vector<unsigned char> bitmap(static_cast<size_t>(bitmapW) * bitmapH, 0);
    std::vector<stbtt_bakedchar> baked(kCharCount);

    const int result =
        stbtt_BakeFontBitmap(ttf, 0.0f, static_cast<float>(pixelHeight),
                             bitmap.data(), bitmapW, bitmapH, kFirstChar,
                             kCharCount, baked.data());
    if (result <= 0) {
        std::fprintf(stderr, "plyEngine: font atlas too small for %dpx\n", pixelHeight);
        return false;
    }

    stbtt_fontinfo info;
    if (!stbtt_InitFont(&info, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        return false;
    }
    const float scale =
        stbtt_ScaleForPixelHeight(&info, static_cast<float>(pixelHeight));
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    ascent_ = static_cast<float>(ascent) * scale;
    descent_ = static_cast<float>(descent) * scale;
    lineHeight_ = static_cast<float>(ascent - descent + lineGap) * scale;

    glyphs_.resize(kCharCount);
    for (int i = 0; i < kCharCount; ++i) {
        const stbtt_bakedchar& ch = baked[i];
        Glyph& glyph = glyphs_[i];
        glyph.x = ch.x0;
        glyph.y = ch.y0;
        glyph.w = ch.x1 - ch.x0;
        glyph.h = ch.y1 - ch.y0;
        glyph.xoff = ch.xoff;
        glyph.yoff = ch.yoff;
        glyph.xadvance = ch.xadvance;
    }

    // Expand the single-channel coverage bitmap to white RGBA.
    atlasPixels_.resize(static_cast<size_t>(bitmapW) * bitmapH * 4, 0);
    for (size_t i = 0; i < bitmap.size(); ++i) {
        const unsigned char coverage = bitmap[i];
        atlasPixels_[i * 4 + 0] = 255;
        atlasPixels_[i * 4 + 1] = 255;
        atlasPixels_[i * 4 + 2] = 255;
        atlasPixels_[i * 4 + 3] = coverage;
    }

    atlasWidth_ = bitmapW;
    atlasHeight_ = bitmapH;
    pixelHeight_ = pixelHeight;
    return true;
}

const Font::Glyph* Font::glyph(int codepoint) const {
    const int index = codepoint - kFirstChar;
    if (index < 0 || index >= static_cast<int>(glyphs_.size())) {
        return nullptr;
    }
    return &glyphs_[static_cast<size_t>(index)];
}

float Font::measure(const std::string& text) const {
    float width = 0.0f;
    for (char c : text) {
        const Glyph* g = glyph(static_cast<unsigned char>(c));
        if (g) {
            width += g->xadvance;
        }
    }
    return width;
}

}  // namespace ply
