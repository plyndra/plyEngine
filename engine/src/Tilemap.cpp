#include "plyEngine/Tilemap.hpp"

#include <cmath>

namespace ply {

Tilemap::Tilemap(int widthCells, int heightCells, float tileSize)
    : widthCells_(widthCells > 0 ? widthCells : 0),
      heightCells_(heightCells > 0 ? heightCells : 0),
      tileSize_(tileSize > 0.0f ? tileSize : 1.0f),
      solid_(static_cast<size_t>(widthCells > 0 ? widthCells : 0) *
                 static_cast<size_t>(heightCells > 0 ? heightCells : 0),
             0) {}

void Tilemap::setSolid(int x, int y, bool solid) {
    if (x < 0 || y < 0 || x >= widthCells_ || y >= heightCells_) {
        return;
    }
    uint8_t& cell = solid_[static_cast<size_t>(y) * widthCells_ + x];
    const bool was = cell != 0;
    if (was == solid) {
        return;
    }
    cell = solid ? 1 : 0;
    solidCount_ += solid ? 1 : static_cast<size_t>(-1);
}

bool Tilemap::isSolid(int x, int y) const {
    if (x < 0 || y < 0 || x >= widthCells_ || y >= heightCells_) {
        return borderSolid_;
    }
    return solid_[static_cast<size_t>(y) * widthCells_ + x] != 0;
}

void Tilemap::query(const Aabb& area, std::vector<Aabb>& out) const {
    const float ts = tileSize_;

    // Convert the area to a cell range, clamped to the grid.
    int minX = static_cast<int>(std::floor(area.min.x / ts));
    int minY = static_cast<int>(std::floor(area.min.y / ts));
    int maxX = static_cast<int>(std::floor(area.max.x / ts));
    int maxY = static_cast<int>(std::floor(area.max.y / ts));

    // A box ending exactly on a cell boundary does not touch the next cell.
    if (static_cast<float>(maxX) * ts >= area.max.x && maxX > minX) {
        --maxX;
    }
    if (static_cast<float>(maxY) * ts >= area.max.y && maxY > minY) {
        --maxY;
    }

    minX = std::max(minX, 0);
    minY = std::max(minY, 0);
    maxX = std::min(maxX, widthCells_ - 1);
    maxY = std::min(maxY, heightCells_ - 1);

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (!isSolid(x, y)) {
                continue;
            }
            const glm::vec2 lo{static_cast<float>(x) * ts, static_cast<float>(y) * ts};
            out.push_back(Aabb{lo, lo + glm::vec2{ts, ts}});
        }
    }
}

ColliderQuery Tilemap::colliderQuery() const {
    return [this](const Aabb& area, std::vector<Aabb>& out) { query(area, out); };
}

}  // namespace ply
