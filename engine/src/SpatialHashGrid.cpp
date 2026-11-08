#include "plyEngine/SpatialHashGrid.hpp"

#include <algorithm>
#include <cmath>

namespace ply {

SpatialHashGrid::SpatialHashGrid(float cellSize)
    : cellSize_(cellSize > 0.0f ? cellSize : 64.0f) {}

void SpatialHashGrid::setCellSize(float size) {
    if (size > 0.0f) {
        cellSize_ = size;
    }
}

void SpatialHashGrid::clear() {
    buckets_.clear();
    items_.clear();
}

int64_t SpatialHashGrid::cellCoord(float value, float cellSize) {
    return static_cast<int64_t>(std::floor(value / cellSize));
}

uint64_t SpatialHashGrid::packKey(int64_t x, int64_t y) {
    // Two 32-bit halves. Levels are far smaller than that range.
    const uint64_t ux = static_cast<uint64_t>(x + 0x80000000LL) & 0xFFFFFFFFull;
    const uint64_t uy = static_cast<uint64_t>(y + 0x80000000LL) & 0xFFFFFFFFull;
    return (uy << 32) | ux;
}

void SpatialHashGrid::insert(uint64_t id, const Aabb& box) {
    items_[id] = box;

    const int64_t minX = cellCoord(box.min.x, cellSize_);
    const int64_t minY = cellCoord(box.min.y, cellSize_);
    const int64_t maxX = cellCoord(box.max.x, cellSize_);
    const int64_t maxY = cellCoord(box.max.y, cellSize_);

    for (int64_t y = minY; y <= maxY; ++y) {
        for (int64_t x = minX; x <= maxX; ++x) {
            buckets_[packKey(x, y)].push_back(id);
        }
    }
}

void SpatialHashGrid::query(const Aabb& area, std::vector<uint64_t>& out) const {
    const int64_t minX = cellCoord(area.min.x, cellSize_);
    const int64_t minY = cellCoord(area.min.y, cellSize_);
    const int64_t maxX = cellCoord(area.max.x, cellSize_);
    const int64_t maxY = cellCoord(area.max.y, cellSize_);

    for (int64_t y = minY; y <= maxY; ++y) {
        for (int64_t x = minX; x <= maxX; ++x) {
            auto it = buckets_.find(packKey(x, y));
            if (it == buckets_.end()) {
                continue;
            }
            out.insert(out.end(), it->second.begin(), it->second.end());
        }
    }
}

void SpatialHashGrid::queryBoxes(const Aabb& area, std::vector<Aabb>& out) const {
    std::vector<uint64_t> ids;
    query(area, ids);
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());

    for (uint64_t id : ids) {
        auto it = items_.find(id);
        if (it == items_.end()) {
            continue;
        }
        if (overlaps(it->second, area)) {
            out.push_back(it->second);
        }
    }
}

}  // namespace ply
