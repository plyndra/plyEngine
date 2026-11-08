#include "plyEngine/SpriteAtlas.hpp"

namespace ply {

SpriteAtlas::SpriteAtlas(int width, int height, const unsigned char* rgba)
    : texture_(width, height, rgba) {}

void SpriteAtlas::addRegion(const std::string& name, int x, int y, int w, int h) {
    TextureRegion r;
    r.u0 = static_cast<float>(x) / static_cast<float>(texture_.width());
    r.v0 = static_cast<float>(y) / static_cast<float>(texture_.height());
    r.u1 = static_cast<float>(x + w) / static_cast<float>(texture_.width());
    r.v1 = static_cast<float>(y + h) / static_cast<float>(texture_.height());
    r.width = w;
    r.height = h;
    regions_[name] = r;
}

const TextureRegion& SpriteAtlas::region(const std::string& name) const {
    return regions_.at(name);
}

const Texture& SpriteAtlas::texture() const { return texture_; }
int SpriteAtlas::width() const { return texture_.width(); }
int SpriteAtlas::height() const { return texture_.height(); }

}  // namespace ply
