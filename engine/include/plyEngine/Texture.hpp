#pragma once

namespace ply {

/// Sampling mode for a texture.
enum class TextureFilter {
    Nearest,  ///< Crisp pixels (pixel art).
    Linear,   ///< Smooth sampling (pre-scaled art, UI).
};

/// An RGBA8 2D texture uploaded to the GPU.
class Texture {
public:
    /// Uploads width*height RGBA pixels (row 0 = top of the image).
    ///
    /// NOTE ON COLOR SPACE: pixels are uploaded as sRGB-encoded RGBA8 and
    /// sampled without linearization (GL_FRAMEBUFFER_SRGB is off), so texture
    /// colors reach the framebuffer byte-for-byte. This is the deliberate M2
    /// choice for 2D pixel art; switching to a linear pipeline (sRGB internal
    /// formats + GL_FRAMEBUFFER_SRGB) is an M4 decision, see docs/DESIGN.md.
    Texture(int width, int height, const unsigned char* rgba,
            TextureFilter filter = TextureFilter::Nearest);
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
