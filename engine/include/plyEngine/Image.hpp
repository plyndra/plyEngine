#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ply {

/// A decoded image in RGBA8, row 0 = top of the image.
class Image {
public:
    Image() = default;

    /// Decodes a PNG (or other stb_image format) from disk.
    /// Returns an invalid Image on failure; see Image::error().
    static Image loadFromFile(const std::string& path);

    /// Decodes from an in-memory file blob (same formats).
    static Image loadFromMemory(const void* data, size_t size);

    bool valid() const { return !pixels_.empty(); }
    int width() const { return width_; }
    int height() const { return height_; }
    const unsigned char* pixels() const { return pixels_.data(); }
    const std::string& error() const { return error_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<unsigned char> pixels_;
    std::string error_;
};

}  // namespace ply
