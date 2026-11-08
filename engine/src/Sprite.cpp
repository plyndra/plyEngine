#include "plyEngine/Sprite.hpp"

#include "plyEngine/Transform.hpp"

#include <algorithm>
#include <vector>

namespace ply {

void drawSprites(World& world, SpriteBatch& batch, const glm::mat4& viewProj) {
    // Gather first: begin()/end() must wrap one texture at a time, and we must
    // not mutate the world while iterating it.
    struct Instance {
        const Texture* texture;
        glm::mat4 transform;
        TextureRegion region;
        glm::vec2 size;
        glm::vec4 color;
    };

    std::vector<Instance> instances;
    world.each<GlobalTransform, Sprite>([&](Entity, GlobalTransform& gt, Sprite& sprite) {
        if (!sprite.texture) {
            return;
        }
        instances.push_back({sprite.texture.get(), gt.matrix, sprite.region,
                             sprite.size, sprite.color});
    });

    if (instances.empty()) {
        return;
    }

    // Group by texture so each texture is one begin/end + one draw call.
    std::stable_sort(instances.begin(), instances.end(),
                     [](const Instance& a, const Instance& b) {
                         return a.texture < b.texture;
                     });

    const Texture* current = nullptr;
    for (const Instance& instance : instances) {
        if (instance.texture != current) {
            if (current != nullptr) {
                batch.end();
            }
            current = instance.texture;
            batch.begin(*current, viewProj);
        }
        batch.draw(instance.transform, instance.size, instance.region, instance.color);
    }
    batch.end();
}

}  // namespace ply
