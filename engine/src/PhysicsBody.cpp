#include "plyEngine/PhysicsBody.hpp"

#include "plyEngine/Transform.hpp"

#include <cmath>

namespace ply {

ColliderQuery tilemapQuery(const Tilemap& map) { return map.colliderQuery(); }

Aabb bodyAabb(const Transform* transform, const Body& body) {
    const glm::vec2 center = transform ? transform->position : glm::vec2{0.0f};
    return makeAabb(center, body.halfExtents * 2.0f);
}

void stepPhysics(World& world, const Tilemap& map, float dt,
                 const PhysicsSettings& settings) {
    if (dt <= 0.0f) {
        return;
    }

    const ColliderQuery query = map.colliderQuery();
    std::vector<std::pair<Entity, glm::vec2>> updates;

    world.each<Body, Transform>([&](Entity entity, Body& body, Transform& transform) {
        Velocity* velocity = world.get<Velocity>(entity);

        if (velocity) {
            // Integrate: semi-implicit Euler keeps this deterministic at a
            // fixed dt.
            velocity->value += settings.gravity * dt;
        }

        const glm::vec2 delta =
            velocity ? velocity->value * dt : glm::vec2{0.0f, 0.0f};

        const Aabb box = bodyAabb(&transform, body);
        const MoveResult moved = moveAndSlide(box, delta, query);

        transform.position = moved.center;

        body.hitX = moved.hitX;
        body.hitY = moved.hitY;
        body.grounded = false;

        if (velocity) {
            if (moved.hitX) {
                velocity->value.x = -velocity->value.x * settings.bounce;
                if (std::fabs(velocity->value.x) < settings.stopSpeed) {
                    velocity->value.x = 0.0f;
                }
            }
            if (moved.hitY) {
                // Downward motion stopped means we are standing on something.
                if (velocity->value.y < 0.0f) {
                    body.grounded = true;
                }
                velocity->value.y = -velocity->value.y * settings.bounce;
                if (std::fabs(velocity->value.y) < settings.stopSpeed) {
                    velocity->value.y = 0.0f;
                }
            }
        }

        // Defer transform writes so each() is not disturbed mid-iteration.
        updates.emplace_back(entity, transform.position);
    });

    for (const auto& [entity, position] : updates) {
        if (Transform* transform = world.get<Transform>(entity)) {
            transform->position = position;
            markTransformDirty(world, entity);
        }
    }
}

}  // namespace ply
