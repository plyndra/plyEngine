#pragma once

namespace ply {

/// An RGBA8 2D texture uploaded to the GPU.
class Texture {
public:
    /// Uploads width*height RGBA pixels (row 0 = top of the image).
    Texture(int width, int height, const unsigned char* rgba);
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void bind(int unit = 0) const;

    int width() const;
    int height() const;
    unsigned int id() const;

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace ply
