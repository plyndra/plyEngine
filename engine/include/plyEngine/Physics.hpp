#pragma once

#include <glm/glm.hpp>

#include <functional>
#include <vector>

namespace ply {

/// Axis-aligned bounding box in world units.
struct Aabb {
    glm::vec2 min{0.0f, 0.0f};
    glm::vec2 max{0.0f, 0.0f};

    glm::vec2 center() const { return (min + max) * 0.5f; }
    glm::vec2 size() const { return max - min; }
    glm::vec2 halfSize() const { return (max - min) * 0.5f; }
};

/// Box from a centre and a full size.
Aabb makeAabb(const glm::vec2& center, const glm::vec2& size);

/// Box from a bottom-left corner and a full size.
Aabb makeAabbFromBottomLeft(const glm::vec2& bottomLeft, const glm::vec2& size);

/// True when the boxes share any area (touching edges do not count).
bool overlaps(const Aabb& a, const Aabb& b);

bool contains(const Aabb& box, const glm::vec2& point);

/// Result of moving one box against a static box.
struct SweepHit {
    bool hit = false;
    /// Fraction of the requested delta travelled before contact, in [0, 1].
    float t = 0.0f;
    /// Unit separation direction pointing out of the obstacle (axis aligned).
    glm::vec2 normal{0.0f, 0.0f};
};

/// Sweeps `moving` by `delta` against a static `obstacle`.
///
/// If the boxes already overlap at t=0 the sweep reports no hit, so a body
/// that somehow starts inside geometry can still move out instead of sticking.
SweepHit sweepAabb(const Aabb& moving, const glm::vec2& delta, const Aabb& obstacle);

/// Fills `out` with every collider overlapping `area`.
using ColliderQuery = std::function<void(const Aabb& area, std::vector<Aabb>& out)>;

struct MoveResult {
    /// New box centre.
    glm::vec2 center{0.0f, 0.0f};
    /// Movement actually applied (may be shorter than the requested delta).
    glm::vec2 applied{0.0f, 0.0f};
    bool hitX = false;
    bool hitY = false;
};

/// Moves a body by `delta`, resolving X then Y separately so it slides along
/// surfaces and never tunnels through a wall at high speed.
///
/// `body` is the current box; `skin` backs the body off the surface slightly so
/// repeated sweeps do not immediately re-collide.
MoveResult moveAndSlide(const Aabb& body, const glm::vec2& delta,
                        const ColliderQuery& query, float skin = 0.001f);

}  // namespace ply
