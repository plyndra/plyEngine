#pragma once

#include "plyEngine/SpriteAtlas.hpp"
#include "plyEngine/SpriteBatch.hpp"
#include "plyEngine/Texture.hpp"
#include "plyEngine/World.hpp"

#include <glm/glm.hpp>

#include <memory>

namespace ply {

/// A drawable quad on an entity. The texture is shared, so many sprites can
/// reference one atlas and batch into a single draw call.
struct Sprite {
    std::shared_ptr<const Texture> texture;
    TextureRegion region;
    glm::vec2 size{32.0f, 32.0f};
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

/// Draws every entity that has both GlobalTransform and Sprite.
/// Sprites are grouped by texture so each distinct texture costs one batch.
/// Call updateTransforms() first.
void drawSprites(World& world, SpriteBatch& batch, const glm::mat4& viewProj);

}  // namespace ply
