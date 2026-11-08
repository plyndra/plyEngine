#pragma once

#include "plyEngine/Physics.hpp"
#include "plyEngine/Tilemap.hpp"
#include "plyEngine/Transform.hpp"
#include "plyEngine/World.hpp"

#include <glm/glm.hpp>

namespace ply {

/// Linear velocity in world units per second.
struct Velocity {
    glm::vec2 value{0.0f, 0.0f};
};

/// A dynamic AABB body, centred on its Transform position.
struct Body {
    glm::vec2 halfExtents{16.0f, 16.0f};
    /// Set by stepPhysics: a body resting on solid ground.
    bool grounded = false;
    /// Contact flags from the most recent step (useful for gameplay logic).
    bool hitX = false;
    bool hitY = false;
};

/// Per-world simulation settings.
struct PhysicsSettings {
    glm::vec2 gravity{0.0f, -1600.0f};
    /// Fraction of velocity retained when a body hits a wall/floor.
    float bounce = 0.0f;
    /// Velocity below this magnitude on contact is zeroed (stops jitter).
    float stopSpeed = 4.0f;
};

/// Collider query over a tilemap, the common static world for M5.
ColliderQuery tilemapQuery(const Tilemap& map);

/// Advances every Body by dt using per-axis swept resolution against `map`.
///
/// Ordering: integrate velocity, move X, move Y. `grounded` is set when the
/// downward motion was stopped by a solid cell.
void stepPhysics(World& world, const Tilemap& map, float dt,
                 const PhysicsSettings& settings = {});

/// Box for a body at its current Transform.
Aabb bodyAabb(const Transform* transform, const Body& body);

}  // namespace ply
