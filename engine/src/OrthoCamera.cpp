#include "plyEngine/OrthoCamera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace ply {

void OrthoCamera::setViewport(int width, int height) {
    viewportWidth_ = width > 0 ? width : 1;
    viewportHeight_ = height > 0 ? height : 1;
}

void OrthoCamera::setPosition(const glm::vec2& position) { position_ = position; }
void OrthoCamera::setZoom(float zoom) { zoom_ = zoom > 0.0f ? zoom : 0.001f; }

const glm::vec2& OrthoCamera::position() const { return position_; }
float OrthoCamera::zoom() const { return zoom_; }

glm::mat4 OrthoCamera::viewProj() const {
    const float halfW = static_cast<float>(viewportWidth_) / (2.0f * zoom_);
    const float halfH = static_cast<float>(viewportHeight_) / (2.0f * zoom_);

    glm::mat4 proj = glm::ortho(-halfW, halfW, -halfH, halfH, -1.0f, 1.0f);
    glm::mat4 view =
        glm::translate(glm::mat4(1.0f), glm::vec3(-position_.x, -position_.y, 0.0f));
    return proj * view;
}

}  // namespace ply
