#include "plyEngine/Transform.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <unordered_set>

namespace ply {

glm::mat4 localMatrix(const Transform& t) {
    glm::mat4 m = glm::translate(glm::mat4(1.0f),
                                 glm::vec3(t.position.x, t.position.y, 0.0f));
    m = glm::rotate(m, t.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
    m = glm::scale(m, glm::vec3(t.scale.x, t.scale.y, 1.0f));
    return m;
}

namespace {

void markSubtreeDirty(World& world, Entity entity,
                      std::unordered_set<uint32_t>& visited) {
    if (!world.alive(entity) || !visited.insert(entity.index).second) {
        return;  // dead, or already visited (also guards against cycles)
    }
    if (GlobalTransform* gt = world.get<GlobalTransform>(entity)) {
        gt->dirty = true;
    }
    if (Children* kids = world.get<Children>(entity)) {
        for (Entity child : kids->value) {
            markSubtreeDirty(world, child, visited);
        }
    }
}

// True if `candidate` is `entity` or a descendant of it.
bool isInSubtree(World& world, Entity entity, Entity candidate) {
    if (entity == candidate) {
        return true;
    }
    Children* kids = world.get<Children>(entity);
    if (!kids) {
        return false;
    }
    for (Entity child : kids->value) {
        if (isInSubtree(world, child, candidate)) {
            return true;
        }
    }
    return false;
}

void detachFromParent(World& world, Entity child) {
    Parent* parent = world.get<Parent>(child);
    if (!parent) {
        return;
    }
    const Entity parentEntity = parent->value;
    if (Children* siblings = world.get<Children>(parentEntity)) {
        auto& list = siblings->value;
        list.erase(std::remove(list.begin(), list.end(), child), list.end());
        if (list.empty()) {
            world.remove<Children>(parentEntity);
        }
    }
    world.remove<Parent>(child);
}

}  // namespace

void markTransformDirty(World& world, Entity entity) {
    std::unordered_set<uint32_t> visited;
    markSubtreeDirty(world, entity, visited);
}

void setTransform(World& world, Entity entity, const Transform& transform) {
    if (Transform* current = world.get<Transform>(entity)) {
        *current = transform;
        markTransformDirty(world, entity);
    } else {
        world.add<Transform>(entity, transform);
        markTransformDirty(world, entity);
    }
}

void setParent(World& world, Entity child, Entity parent) {
    if (!world.alive(child) || !world.alive(parent) || child == parent) {
        return;
    }
    // Reject cycles: parent must not already be inside child's subtree.
    if (isInSubtree(world, child, parent)) {
        return;
    }

    detachFromParent(world, child);

    world.add<Parent>(child, Parent{parent});
    if (Children* kids = world.get<Children>(parent)) {
        kids->value.push_back(child);
    } else {
        world.add<Children>(parent, Children{{child}});
    }

    markTransformDirty(world, child);
}

void clearParent(World& world, Entity child) {
    if (!world.alive(child)) {
        return;
    }
    detachFromParent(world, child);
    markTransformDirty(world, child);
}

void updateTransforms(World& world) {
    // Walk from roots in topological order so a parent is always resolved
    // before its children. Dirty state inherits downward, so a dirty parent
    // forces its whole subtree to recompute.
    struct Frame {
        Entity entity;
        Entity parent;
        bool parentUpdated;
    };

    std::vector<Frame> stack;
    world.each<Transform>([&](Entity entity, Transform&) {
        if (!world.has<Parent>(entity)) {
            stack.push_back({entity, Entity{}, false});
        }
    });

    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();

        bool updated = false;
        if (GlobalTransform* gt = world.get<GlobalTransform>(frame.entity)) {
            if (gt->dirty || frame.parentUpdated) {
                const Transform* local = world.get<Transform>(frame.entity);
                if (local) {
                    glm::mat4 parentMatrix(1.0f);
                    if (const GlobalTransform* pg =
                            world.get<GlobalTransform>(frame.parent)) {
                        parentMatrix = pg->matrix;
                    }
                    gt->matrix = parentMatrix * localMatrix(*local);
                    gt->dirty = false;
                    updated = true;
                }
            }
        }

        if (Children* kids = world.get<Children>(frame.entity)) {
            for (Entity child : kids->value) {
                stack.push_back({child, frame.entity, updated});
            }
        }
    }
}

}  // namespace ply
