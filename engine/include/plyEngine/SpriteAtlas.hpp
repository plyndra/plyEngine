#pragma once

#include "plyEngine/Texture.hpp"

#include <string>
#include <unordered_map>

namespace ply {

/// A sub-rectangle of a texture, in normalized UV coordinates.
/// (u0, v0) is the top-left corner of the region in image space;
/// (u1, v1) is the bottom-right corner.
struct TextureRegion {
    float u0 = 0.0f, v0 = 0.0f;
    float u1 = 1.0f, v1 = 1.0f;
    int width = 0, height = 0;
};

/// A single texture plus named sub-regions. All sprites drawn from one atlas
/// can share a single draw call (see SpriteBatch).
class SpriteAtlas {
public:
    SpriteAtlas(int width, int height, const unsigned char* rgba);

    /// Add a named region in pixel coordinates, (x, y) = top-left origin.
    void addRegion(const std::string& name, int x, int y, int w, int h);

    const TextureRegion& region(const std::string& name) const;
    const Texture& texture() const;

    int width() const;
    int height() const;

private:
    Texture texture_;
    std::unordered_map<std::string, TextureRegion> regions_;
};

}  // namespace ply
