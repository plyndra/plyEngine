#pragma once

#include "plyEngine/Physics.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Texture.hpp"

#include <glm/glm.hpp>

#include <vector>

namespace ply {

/// Immediate-mode debug drawing: collision boxes, tile grids and the spatial
/// hash behind a toggle. Physics bugs are nearly invisible without it.
class DebugDrawer {
public:
    DebugDrawer();

    /// Starts a debug-draw pass. The batch is ended by end().
    void begin(SpriteBatch& batch, const glm::mat4& viewProj);
    void end();

    /// Outline of a box, drawn as four thin filled rectangles.
    void drawBox(const Aabb& box, const glm::vec4& color, float thickness = 2.0f);

    /// Solid filled box (used for the grid overlay and markers).
    void drawFilledBox(const Aabb& box, const glm::vec4& color);

    /// Cell grid over a region, e.g. a tilemap.
    void drawGrid(const Aabb& area, float cellSize, const glm::vec4& color,
                  float lineWidth = 1.0f);

    /// Marks a point with a small square.
    void drawPoint(const glm::vec2& position, float size, const glm::vec4& color);

    bool active() const { return batch_ != nullptr; }

private:
    Texture white_;
    SpriteBatch* batch_ = nullptr;
    TextureRegion region_;
};

}  // namespace ply
