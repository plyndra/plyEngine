#pragma once

#include "plyEngine/Entity.hpp"
#include "plyEngine/World.hpp"

#include <glm/glm.hpp>
#include <vector>

namespace ply {

/// Local transform relative to the entity's parent (or the world if none).
struct Transform {
    glm::vec2 position{0.0f, 0.0f};
    float rotation = 0.0f;  // radians, counter-clockwise
    glm::vec2 scale{1.0f, 1.0f};
};

/// Cached world-space matrix, produced by updateTransforms().
struct GlobalTransform {
    glm::mat4 matrix{1.0f};
    bool dirty = true;
};

/// Parent link. An entity with Parent is positioned relative to it.
struct Parent {
    Entity value{};
};

/// Children list, maintained alongside Parent.
struct Children {
    std::vector<Entity> value;
};

/// Local matrix for a Transform.
glm::mat4 localMatrix(const Transform& t);

/// Links child under parent. No-op if either is dead or if it would create a
/// cycle. Marks the affected subtree dirty.
void setParent(World& world, Entity child, Entity parent);

/// Unlinks child from its parent, keeping its local transform.
void clearParent(World& world, Entity child);

/// Marks an entity's world transform, and its whole subtree, as needing
/// recomputation. Call this after mutating a Transform obtained via get().
void markTransformDirty(World& world, Entity entity);

/// Writes a Transform and marks the subtree dirty (preferred over get()).
void setTransform(World& world, Entity entity, const Transform& transform);

/// Recomputes GlobalTransform for dirty subtrees, parents before children.
/// Entities without a Parent are roots.
void updateTransforms(World& world);

}  // namespace ply
