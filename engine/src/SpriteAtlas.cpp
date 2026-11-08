#include "plyEngine/SpriteAtlas.hpp"

#include "plyEngine/Spritesheet.hpp"

namespace ply {

TextureRegion makeRegion(int texWidth, int texHeight, int x, int y, int w, int h) {
    TextureRegion r;
    r.u0 = static_cast<float>(x) / static_cast<float>(texWidth);
    r.v0 = static_cast<float>(y) / static_cast<float>(texHeight);
    r.u1 = static_cast<float>(x + w) / static_cast<float>(texWidth);
    r.v1 = static_cast<float>(y + h) / static_cast<float>(texHeight);
    r.width = w;
    r.height = h;
    return r;
}

SpriteAtlas::SpriteAtlas(int width, int height, const unsigned char* rgba,
                         TextureFilter filter)
    : texture_(std::make_shared<const Texture>(width, height, rgba, filter)) {}

void SpriteAtlas::addRegion(const std::string& name, int x, int y, int w, int h) {
    regions_[name] = makeRegion(texture_->width(), texture_->height(), x, y, w, h);
}

void SpriteAtlas::addRegions(const std::vector<NamedRect>& rects) {
    for (const NamedRect& rect : rects) {
        addRegion(rect.name, rect.x, rect.y, rect.w, rect.h);
    }
}

const TextureRegion& SpriteAtlas::region(const std::string& name) const {
    return regions_.at(name);
}

const Texture& SpriteAtlas::texture() const { return *texture_; }
int SpriteAtlas::width() const { return texture_->width(); }
int SpriteAtlas::height() const { return texture_->height(); }

}  // namespace ply
