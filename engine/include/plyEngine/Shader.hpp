#pragma once

#include <glm/glm.hpp>

namespace ply {

/// A compiled and linked GLSL program.
class Shader {
public:
    Shader(const char* vertexSrc, const char* fragmentSrc);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool valid() const;
    void use() const;
    unsigned int id() const;

    void setInt(const char* name, int value) const;
    void setMat4(const char* name, const glm::mat4& m) const;

private:
    unsigned int id_ = 0;
    bool valid_ = false;
};

}  // namespace ply
