#pragma once

#include "plyEngine/Font.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Texture.hpp"

#include <glm/glm.hpp>

#include <string>

namespace ply {

/// Draws text using a baked Font. Owns the font's atlas texture.
///
/// Text batches separately from sprites because SpriteBatch groups by texture:
/// begin a batch with texture(), draw all your text, then end() it.
class TextRenderer {
public:
    explicit TextRenderer(const Font& font);

    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    const Texture& texture() const { return texture_; }
    const Font& font() const { return font_; }

    /// Advance width of one line, in world units.
    float measure(const std::string& text, float scale = 1.0f) const;

    /// Line height for layout, in world units.
    float lineHeight(float scale = 1.0f) const;

    /// Draws `text` with its baseline-top pen starting at `position`
    /// (y-up world space). Handles '\n'. The batch must have been begun with
    /// texture().
    void draw(SpriteBatch& batch, const std::string& text, const glm::vec2& position,
              const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f}, float scale = 1.0f) const;

private:
    const Font& font_;
    Texture texture_;
};

}  // namespace ply
