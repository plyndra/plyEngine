#include "plyEngine/Image.hpp"

#include <stb_image.h>

#include <cstdio>
#include <fstream>

namespace ply {

namespace {

std::vector<unsigned char> readFile(const std::string& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "could not open '" + path + "'";
        return {};
    }
    return std::vector<unsigned char>(std::istreambuf_iterator<char>(file),
                                      std::istreambuf_iterator<char>());
}

}  // namespace

Image Image::loadFromFile(const std::string& path) {
    Image image;
    std::string error;
    std::vector<unsigned char> bytes = readFile(path, error);
    if (bytes.empty()) {
        image.error_ = error.empty() ? "empty file: " + path : error;
        return image;
    }
    return loadFromMemory(bytes.data(), bytes.size());
}

Image Image::loadFromMemory(const void* data, size_t size) {
    Image image;
    int width = 0;
    int height = 0;
    int channels = 0;

    // Force RGBA so every texture has a predictable 4-byte layout.
    stbi_uc* decoded = stbi_load_from_memory(
        static_cast<const stbi_uc*>(data), static_cast<int>(size), &width, &height,
        &channels, 4);

    if (!decoded) {
        const char* why = stbi_failure_reason();
        image.error_ = why ? why : "unknown image decode error";
        return image;
    }

    image.width_ = width;
    image.height_ = height;
    image.pixels_.assign(decoded, decoded + static_cast<size_t>(width) * height * 4);
    stbi_image_free(decoded);
    return image;
}

}  // namespace ply
