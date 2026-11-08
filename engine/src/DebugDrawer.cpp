#include "plyEngine/DebugDrawer.hpp"

#include <cmath>
#include <vector>

namespace ply {

namespace {
const unsigned char kWhitePixel[4] = {255, 255, 255, 255};
}

DebugDrawer::DebugDrawer()
    : white_(1, 1, kWhitePixel, TextureFilter::Nearest),
      region_(makeRegion(1, 1, 0, 0, 1, 1)) {}

void DebugDrawer::begin(SpriteBatch& batch, const glm::mat4& viewProj) {
    batch_ = &batch;
    batch.begin(white_, viewProj);
}

void DebugDrawer::end() {
    if (batch_) {
        batch_->end();
        batch_ = nullptr;
    }
}

void DebugDrawer::drawFilledBox(const Aabb& box, const glm::vec4& color) {
    if (!batch_) {
        return;
    }
    const glm::vec2 center = box.center();
    const glm::vec2 size = box.size();
    if (size.x <= 0.0f || size.y <= 0.0f) {
        return;
    }
    batch_->draw(center, size, region_, color);
}

void DebugDrawer::drawBox(const Aabb& box, const glm::vec4& color, float thickness) {
    if (!batch_) {
        return;
    }
    const glm::vec2 min = box.min;
    const glm::vec2 max = box.max;
    const float t = thickness > 0.0f ? thickness : 1.0f;

    // Bottom, top, left, right edges as thin filled quads.
    drawFilledBox(Aabb{min, {max.x, min.y + t}}, color);
    drawFilledBox(Aabb{{min.x, max.y - t}, max}, color);
    drawFilledBox(Aabb{{min.x, min.y + t}, {min.x + t, max.y - t}}, color);
    drawFilledBox(Aabb{{max.x - t, min.y + t}, {max.x, max.y - t}}, color);
}

void DebugDrawer::drawGrid(const Aabb& area, float cellSize,
                           const glm::vec4& color, float lineWidth) {
    if (!batch_ || cellSize <= 0.0f) {
        return;
    }
    const float t = lineWidth > 0.0f ? lineWidth : 1.0f;

    const int firstX = static_cast<int>(std::floor(area.min.x / cellSize));
    const int lastX = static_cast<int>(std::ceil(area.max.x / cellSize));
    const int firstY = static_cast<int>(std::floor(area.min.y / cellSize));
    const int lastY = static_cast<int>(std::ceil(area.max.y / cellSize));

    // Guard against pathological ranges producing millions of quads.
    if (lastX - firstX > 512 || lastY - firstY > 512) {
        return;
    }

    for (int x = firstX; x <= lastX; ++x) {
        const float px = static_cast<float>(x) * cellSize;
        drawFilledBox(Aabb{{px, area.min.y}, {px + t, area.max.y}}, color);
    }
    for (int y = firstY; y <= lastY; ++y) {
        const float py = static_cast<float>(y) * cellSize;
        drawFilledBox(Aabb{{area.min.x, py}, {area.max.x, py + t}}, color);
    }
}

void DebugDrawer::drawPoint(const glm::vec2& position, float size,
                            const glm::vec4& color) {
    const float half = size * 0.5f;
    drawFilledBox(Aabb{position - glm::vec2{half}, position + glm::vec2{half}}, color);
}

}  // namespace ply
