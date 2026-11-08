#pragma once

#include <glm/glm.hpp>

namespace ply {

/// A 2D orthographic camera (y-up world coordinates, origin at view center).
class OrthoCamera {
public:
    void setViewport(int width, int height);
    void setPosition(const glm::vec2& position);
    void setZoom(float zoom);  // > 0; higher = zoomed in (zoom=1: 1 unit = 1 px)

    const glm::vec2& position() const;
    float zoom() const;

    /// Combined projection * view matrix for the sprite shader.
    glm::mat4 viewProj() const;

private:
    glm::vec2 position_{0.0f};
    float zoom_ = 1.0f;
    int viewportWidth_ = 1;
    int viewportHeight_ = 1;
};

}  // namespace ply
