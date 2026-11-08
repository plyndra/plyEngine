#pragma once

#include "plyEngine/Physics.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ply {

/// Uniform spatial hash for broad-phase collision queries.
///
/// Entities are bucketed by the cells their AABB overlaps; a query only visits
/// the cells the query box touches. Pair with a brute-force path for very small
/// scenes, where hashing costs more than it saves.
class SpatialHashGrid {
public:
    explicit SpatialHashGrid(float cellSize = 64.0f);

    float cellSize() const { return cellSize_; }
    void setCellSize(float size);

    void clear();
    void insert(uint64_t id, const Aabb& box);

    /// Appends every id whose box overlaps `area` (may include duplicates when
    /// an item spans several cells; callers should de-duplicate if needed).
    void query(const Aabb& area, std::vector<uint64_t>& out) const;

    /// Appends every candidate box overlapping `area`, de-duplicated.
    void queryBoxes(const Aabb& area, std::vector<Aabb>& out) const;

    size_t itemCount() const { return items_.size(); }
    size_t bucketCount() const { return buckets_.size(); }

private:
    static int64_t cellCoord(float value, float cellSize);
    static uint64_t packKey(int64_t x, int64_t y);

    float cellSize_ = 64.0f;
    std::unordered_map<uint64_t, std::vector<uint64_t>> buckets_;
    std::unordered_map<uint64_t, Aabb> items_;
};

}  // namespace ply
