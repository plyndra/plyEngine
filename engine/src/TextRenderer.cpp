#include "plyEngine/TextRenderer.hpp"

#include "plyEngine/SpriteAtlas.hpp"  // makeRegion

#include <vector>

namespace ply {

TextRenderer::TextRenderer(const Font& font)
    : font_(font),
      texture_(font.atlasWidth(), font.atlasHeight(), font.atlasPixels(),
               TextureFilter::Linear) {}

float TextRenderer::measure(const std::string& text, float scale) const {
    return font_.measure(text) * scale;
}

float TextRenderer::lineHeight(float scale) const {
    return font_.lineHeight() * scale;
}

void TextRenderer::draw(SpriteBatch& batch, const std::string& text,
                        const glm::vec2& position, const glm::vec4& color,
                        float scale) const {
    // stb_truetype reports y offsets with y growing downward; our world has y
    // growing up, so glyph quads hang below the pen line.
    glm::vec2 pen = position;
    const float lineAdvance = font_.lineHeight() * scale;

    for (char ch : text) {
        if (ch == '\n') {
            pen.x = position.x;
            pen.y -= lineAdvance;
            continue;
        }

        const Font::Glyph* glyph = font_.glyph(static_cast<unsigned char>(ch));
        if (!glyph) {
            continue;
        }

        if (glyph->w > 0 && glyph->h > 0) {
            const TextureRegion region =
                makeRegion(font_.atlasWidth(), font_.atlasHeight(), glyph->x,
                           glyph->y, glyph->w, glyph->h);

            const float w = static_cast<float>(glyph->w) * scale;
            const float h = static_cast<float>(glyph->h) * scale;

            const glm::vec2 center{
                pen.x + glyph->xoff * scale + w * 0.5f,
                pen.y - glyph->yoff * scale - h * 0.5f,
            };
            batch.draw(center, glm::vec2{w, h}, region, color);
        }

        pen.x += glyph->xadvance * scale;
    }
}

}  // namespace ply
