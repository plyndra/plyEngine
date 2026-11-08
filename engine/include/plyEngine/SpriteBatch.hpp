#pragma once

#include "plyEngine/Shader.hpp"
#include "plyEngine/SpriteAtlas.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace ply {

/// Batches textured quads from a single texture into as few draw calls as
/// possible. Call begin()/end() once per frame around your draws.
class SpriteBatch {
public:
    explicit SpriteBatch(size_t maxQuads = 8192);
    ~SpriteBatch();

    SpriteBatch(const SpriteBatch&) = delete;
    SpriteBatch& operator=(const SpriteBatch&) = delete;

    void begin(const Texture& texture, const glm::mat4& viewProj);

    /// Draw a quad with its bottom-left corner at `position`, size `size`,
    /// sampled from `region`, tinted by `color` (default white).
    void draw(const glm::vec2& position, const glm::vec2& size,
              const TextureRegion& region,
              const glm::vec4& color = {1.0f, 1.0f, 1.0f, 1.0f});

    void end();

private:
    void flush();

    struct Vertex {
        float x, y;
        float u, v;
        float r, g, b, a;
    };

    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    size_t maxQuads_ = 0;
    Shader shader_;
    std::vector<Vertex> vertices_;
    const Texture* texture_ = nullptr;
    glm::mat4 viewProj_{1.0f};
    bool drawing_ = false;
};

}  // namespace ply
