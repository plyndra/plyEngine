#include "plyEngine/PixelText.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace ply {

namespace {

/// Tokenizes `key=value` pairs from one BMFont line. Values may be quoted
/// (face, file) or bare (numbers, comma lists).
std::map<std::string, std::string> fieldsOf(const std::string& line, std::size_t start) {
    std::map<std::string, std::string> fields;
    std::size_t i = start;
    while (i < line.size()) {
        while (i < line.size() && line[i] == ' ') ++i;
        const std::size_t keyStart = i;
        while (i < line.size() && line[i] != '=' && line[i] != ' ') ++i;
        if (i >= line.size() || line[i] != '=') break;  // bare token, ignore
        const std::string key = line.substr(keyStart, i - keyStart);
        ++i;  // skip '='
        std::string value;
        if (i < line.size() && line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') value.push_back(line[i++]);
            if (i < line.size()) ++i;  // closing quote
        } else {
            while (i < line.size() && line[i] != ' ') value.push_back(line[i++]);
        }
        if (!key.empty()) fields[key] = value;
    }
    return fields;
}

bool parseInt(const std::map<std::string, std::string>& fields, const std::string& key, int& out) {
    const auto it = fields.find(key);
    if (it == fields.end() || it->second.empty()) return false;
    char* end = nullptr;
    const long value = std::strtol(it->second.c_str(), &end, 10);
    if (end == nullptr || *end != '\0') return false;
    out = static_cast<int>(value);
    return true;
}

BMFontLoadResult fail(BMFontErrorCode code, std::string detail) {
    BMFontLoadResult result;
    result.error.code = code;
    result.error.message = std::string(toString(code)) + ": " + std::move(detail);
    return result;
}

}  // namespace

const char* toString(BMFontErrorCode code) {
    switch (code) {
        case BMFontErrorCode::None: return "None";
        case BMFontErrorCode::FileNotReadable: return "FileNotReadable";
        case BMFontErrorCode::MalformedLine: return "MalformedLine";
        case BMFontErrorCode::MissingField: return "MissingField";
        case BMFontErrorCode::WrongType: return "WrongType";
        case BMFontErrorCode::BinaryFormatUnsupported: return "BinaryFormatUnsupported";
        case BMFontErrorCode::DuplicateGlyph: return "DuplicateGlyph";
        case BMFontErrorCode::NoGlyphs: return "NoGlyphs";
        case BMFontErrorCode::NonPositiveSize: return "NonPositiveSize";
    }
    return "None";
}

const BMFontGlyph* BMFont::findGlyph(int id) const {
    // glyphs is kept sorted by id, so a binary search is enough.
    const auto it = std::lower_bound(glyphs.begin(), glyphs.end(), id,
                                    [](const BMFontGlyph& glyph, int want) {
                                        return glyph.id < want;
                                    });
    if (it == glyphs.end() || it->id != id) return nullptr;
    return &*it;
}

int BMFont::kerningBetween(int first, int second) const {
    for (const BMFontKerning& pair : kerning) {
        if (pair.first == first && pair.second == second) return pair.amount;
    }
    return 0;
}

BMFontLoadResult loadBMFontFromString(std::string_view text) {
    // BMFont's binary form starts with 'B','M','F'.
    if (text.size() >= 3 && text[0] == 'B' && text[1] == 'M' && text[2] == 'F') {
        return fail(BMFontErrorCode::BinaryFormatUnsupported,
                    "binary descriptor; this loader reads the text form only");
    }

    BMFont font;
    std::vector<BMFontGlyph> glyphs;
    std::vector<BMFontKerning> kerning;
    bool sawCommon = false;
    std::string pageFile;

    std::istringstream stream{std::string(text)};
    std::string line;
    int lineNumber = 0;
    while (std::getline(stream, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        const std::size_t space = line.find(' ');
        const std::string tag = line.substr(0, space);

        if (tag == "info") {
            const auto fields = fieldsOf(line, space == std::string::npos ? line.size() : space);
            const auto face = fields.find("face");
            if (face != fields.end()) font.face = face->second;
            if (!parseInt(fields, "size", font.size)) {
                return fail(BMFontErrorCode::MissingField,
                            "info without a numeric size at line " + std::to_string(lineNumber));
            }
        } else if (tag == "common") {
            const auto fields = fieldsOf(line, space == std::string::npos ? line.size() : space);
            if (!parseInt(fields, "lineHeight", font.lineHeight) ||
                !parseInt(fields, "base", font.base) || !parseInt(fields, "scaleW", font.scaleW) ||
                !parseInt(fields, "scaleH", font.scaleH)) {
                return fail(BMFontErrorCode::MissingField,
                            "common missing lineHeight/base/scaleW/scaleH at line " +
                                std::to_string(lineNumber));
            }
            if (font.lineHeight <= 0 || font.scaleW <= 0 || font.scaleH <= 0) {
                return fail(BMFontErrorCode::NonPositiveSize,
                            "non-positive common metric at line " + std::to_string(lineNumber));
            }
            sawCommon = true;
        } else if (tag == "page") {
            const auto fields = fieldsOf(line, space == std::string::npos ? line.size() : space);
            const auto file = fields.find("file");
            if (file != fields.end() && pageFile.empty()) pageFile = file->second;
        } else if (tag == "char") {
            const auto fields = fieldsOf(line, space == std::string::npos ? line.size() : space);
            BMFontGlyph glyph;
            const bool ok =
                parseInt(fields, "id", glyph.id) && parseInt(fields, "x", glyph.x) &&
                parseInt(fields, "y", glyph.y) && parseInt(fields, "width", glyph.w) &&
                parseInt(fields, "height", glyph.h) && parseInt(fields, "xoffset", glyph.xOffset) &&
                parseInt(fields, "yoffset", glyph.yOffset) &&
                parseInt(fields, "xadvance", glyph.xAdvance);
            if (!ok) {
                // Distinguish "key absent" from "key present but not a number".
                const bool presentButBad = fields.count("xadvance") > 0;
                return fail(presentButBad ? BMFontErrorCode::WrongType
                                          : BMFontErrorCode::MissingField,
                            "char at line " + std::to_string(lineNumber) +
                                " is missing a numeric field");
            }
            if (!parseInt(fields, "page", glyph.page)) glyph.page = 0;
            for (const BMFontGlyph& existing : glyphs) {
                if (existing.id == glyph.id) {
                    return fail(BMFontErrorCode::DuplicateGlyph,
                                "duplicate glyph id " + std::to_string(glyph.id));
                }
            }
            glyphs.push_back(glyph);
        } else if (tag == "kerning") {
            const auto fields = fieldsOf(line, space == std::string::npos ? line.size() : space);
            BMFontKerning pair;
            if (!parseInt(fields, "first", pair.first) || !parseInt(fields, "second", pair.second) ||
                !parseInt(fields, "amount", pair.amount)) {
                return fail(BMFontErrorCode::MissingField,
                            "kerning at line " + std::to_string(lineNumber) + " is incomplete");
            }
            kerning.push_back(pair);
        } else if (tag == "chars" || tag == "kernings") {
            // Count lines are advisory; the glyph lines themselves are the data.
            continue;
        } else if (tag.empty() || tag[0] == '@' || line.find('=') == std::string::npos) {
            // A line that carries no key=value pair is not a BMFont line at all.
            return fail(BMFontErrorCode::MalformedLine,
                        "unrecognised line " + std::to_string(lineNumber) + ": " + line);
        }
    }

    if (!sawCommon) return fail(BMFontErrorCode::MissingField, "no common line");
    if (glyphs.empty()) return fail(BMFontErrorCode::NoGlyphs, "no char lines");
    if (font.size <= 0) font.size = font.lineHeight;

    // Sorted by id so lookup can binary-search and two fonts compare equal
    // regardless of the order the exporter wrote them.
    std::sort(glyphs.begin(), glyphs.end(),
              [](const BMFontGlyph& a, const BMFontGlyph& b) { return a.id < b.id; });

    BMFontLoadResult result;
    font.glyphs = std::move(glyphs);
    font.kerning = std::move(kerning);
    font.pageFile = std::move(pageFile);
    result.font = std::move(font);
    return result;
}

BMFontLoadResult loadBMFontFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return fail(BMFontErrorCode::FileNotReadable, path);
    std::ostringstream contents;
    contents << file.rdbuf();
    return loadBMFontFromString(contents.str());
}

TextLayout layoutText(const BMFont& font, std::string_view text, int originX, int baselineY,
                      int scale) {
    const int k = scale < 1 ? 1 : scale;

    TextLayout layout;
    layout.scaledLineHeight = font.lineHeight * k;
    layout.lineCount = 1;

    int penX = originX;
    int baseline = baselineY;
    int previous = -1;
    bool havePrevious = false;

    int minX = 0, minY = 0, maxX = 0, maxY = 0;
    bool haveInk = false;

    for (const unsigned char byte : text) {
        if (byte == '\n') {
            penX = originX;
            baseline += font.lineHeight * k;
            ++layout.lineCount;
            havePrevious = false;  // kerning never crosses a line break
            continue;
        }
        if (byte == '\r') continue;

        const int id = static_cast<int>(byte);
        const BMFontGlyph* glyph = font.findGlyph(id);
        if (glyph == nullptr) {
            // Reported rather than drawn: drawing a wrong glyph would hide the
            // gap from the art lint that is meant to fail on it.
            if (std::find(layout.missingGlyphIds.begin(), layout.missingGlyphIds.end(), id) ==
                layout.missingGlyphIds.end()) {
                layout.missingGlyphIds.push_back(id);
            }
            havePrevious = false;  // no kerning against an unknown advance
            continue;
        }

        if (havePrevious) penX += font.kerningBetween(previous, id) * k;

        // Baseline alignment by ascent: the glyph's top sits `base - yOffset`
        // above the baseline, so its bottom row lands on the baseline.
        const int top = baseline - (font.base - glyph->yOffset) * k;
        const int left = penX + glyph->xOffset * k;
        const int width = glyph->w * k;
        const int height = glyph->h * k;

        if (width > 0 && height > 0) {
            UIQuad quad;
            quad.topLeft = UIVertex{left, top, glyph->x, glyph->y};
            quad.topRight = UIVertex{left + width, top, glyph->x + glyph->w, glyph->y};
            quad.bottomRight =
                UIVertex{left + width, top + height, glyph->x + glyph->w, glyph->y + glyph->h};
            quad.bottomLeft = UIVertex{left, top + height, glyph->x, glyph->y + glyph->h};
            layout.quads.push_back(quad);

            if (!haveInk) {
                minX = left;
                minY = top;
                maxX = left + width;
                maxY = top + height;
                haveInk = true;
            } else {
                minX = std::min(minX, left);
                minY = std::min(minY, top);
                maxX = std::max(maxX, left + width);
                maxY = std::max(maxY, top + height);
            }
        }

        penX += glyph->xAdvance * k;
        previous = id;
        havePrevious = true;
    }

    layout.advanceWidth = penX - originX;
    layout.bounds = haveInk ? UIRect{minX, minY, maxX - minX, maxY - minY} : UIRect{};
    return layout;
}

UIRect textInkBounds(const std::vector<UIQuad>& quads) {
    if (quads.empty()) return UIRect{};
    int minX = quads.front().topLeft.x;
    int minY = quads.front().topLeft.y;
    int maxX = minX;
    int maxY = minY;
    for (const UIQuad& quad : quads) {
        for (const UIVertex* v : {&quad.topLeft, &quad.topRight, &quad.bottomRight, &quad.bottomLeft}) {
            minX = std::min(minX, v->x);
            minY = std::min(minY, v->y);
            maxX = std::max(maxX, v->x);
            maxY = std::max(maxY, v->y);
        }
    }
    return UIRect{minX, minY, maxX - minX, maxY - minY};
}

std::uint64_t structuralHash(const TextLayout& layout) {
    StructuralHasher hasher;
    hasher.u64(layout.quads.size());
    for (const UIQuad& quad : layout.quads) {
        for (const UIVertex* vertex :
             {&quad.topLeft, &quad.topRight, &quad.bottomRight, &quad.bottomLeft}) {
            hasher.i32(vertex->x);
            hasher.i32(vertex->y);
            hasher.i32(vertex->u);
            hasher.i32(vertex->v);
        }
    }
    hasher.u64(layout.missingGlyphIds.size());
    for (const int id : layout.missingGlyphIds) hasher.i32(id);
    hasher.i32(layout.advanceWidth);
    return hasher.value();
}

}  // namespace ply
