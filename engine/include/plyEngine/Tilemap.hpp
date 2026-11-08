#pragma once

#include "plyEngine/Physics.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ply {

/// A grid of solid/empty cells used as static level collision.
///
/// Cell (x, y) covers world area [x*size, (x+1)*size) x [y*size, (y+1)*size).
/// Out-of-bounds cells are treated as solid by default so bodies cannot leave
/// the level, which is what a single-screen game wants.
class Tilemap {
public:
    Tilemap(int widthCells, int heightCells, float tileSize);

    int widthCells() const { return widthCells_; }
    int heightCells() const { return heightCells_; }
    float tileSize() const { return tileSize_; }

    void setSolid(int x, int y, bool solid);
    bool isSolid(int x, int y) const;

    /// When true (default), cells outside the grid count as solid.
    void setBorderSolid(bool solid) { borderSolid_ = solid; }
    bool borderSolid() const { return borderSolid_; }

    /// Fills `out` with the boxes of every solid cell overlapping `area`.
    void query(const Aabb& area, std::vector<Aabb>& out) const;

    size_t solidCellCount() const { return solidCount_; }

    /// Convenience ColliderQuery bound to this map.
    ColliderQuery colliderQuery() const;

private:
    int widthCells_ = 0;
    int heightCells_ = 0;
    float tileSize_ = 1.0f;
    bool borderSolid_ = true;
    std::vector<uint8_t> solid_;
    size_t solidCount_ = 0;
};

}  // namespace ply
