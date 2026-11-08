#include "plyEngine/SpriteBatch.hpp"

#include <glad/gl.h>

namespace ply {

namespace {

const char* kVertexSrc = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
out vec2 vTexCoord;
out vec4 vColor;
uniform mat4 uViewProj;
void main() {
    vTexCoord = aTexCoord;
    vColor = aColor;
    gl_Position = uViewProj * vec4(aPos, 0.0, 1.0);
})";

const char* kFragmentSrc = R"(#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, vTexCoord) * vColor;
})";

}  // namespace

SpriteBatch::SpriteBatch(size_t maxQuads)
    : maxQuads_(maxQuads), shader_(kVertexSrc, kFragmentSrc) {
    std::vector<unsigned int> indices(maxQuads_ * 6);
    size_t offset = 0;
    for (size_t q = 0; q < maxQuads_; ++q) {
        unsigned int i = static_cast<unsigned int>(q * 4);
        indices[offset++] = i + 0;
        indices[offset++] = i + 1;
        indices[offset++] = i + 2;
        indices[offset++] = i + 0;
        indices[offset++] = i + 2;
        indices[offset++] = i + 3;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(maxQuads_ * 4 * sizeof(Vertex)), nullptr,
                 GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    const auto stride = static_cast<GLsizei>(sizeof(Vertex));
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, x)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, u)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(Vertex, r)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

SpriteBatch::~SpriteBatch() {
    glDeleteBuffers(1, &ebo_);
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
}

void SpriteBatch::begin(const Texture& texture, const glm::mat4& viewProj) {
    texture_ = &texture;
    viewProj_ = viewProj;
    vertices_.clear();
    drawing_ = true;
}

void SpriteBatch::draw(const glm::vec2& position, const glm::vec2& size,
                       const TextureRegion& region, const glm::vec4& color) {
    if (!drawing_) {
        return;
    }
    if (vertices_.size() >= maxQuads_ * 4) {
        flush();
    }

    const float x = position.x;
    const float y = position.y;
    const float w = size.x;
    const float h = size.y;

    // Vertex order per quad: bottom-left, bottom-right, top-right, top-left.
    vertices_.push_back({x, y, region.u0, region.v1, color.r, color.g, color.b, color.a});
    vertices_.push_back({x + w, y, region.u1, region.v1, color.r, color.g, color.b, color.a});
    vertices_.push_back({x + w, y + h, region.u1, region.v0, color.r, color.g, color.b, color.a});
    vertices_.push_back({x, y + h, region.u0, region.v0, color.r, color.g, color.b, color.a});
}

void SpriteBatch::end() {
    if (!drawing_) {
        return;
    }
    flush();
    glBindVertexArray(0);
    drawing_ = false;
}

void SpriteBatch::flush() {
    if (vertices_.empty()) {
        return;
    }

    shader_.use();
    shader_.setMat4("uViewProj", viewProj_);
    shader_.setInt("uTexture", 0);
    texture_->bind(0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)),
                    vertices_.data());
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(vertices_.size() / 4 * 6),
                   GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    vertices_.clear();
}

}  // namespace ply
