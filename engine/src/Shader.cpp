#include "plyEngine/Shader.hpp"

#include <glad/gl.h>

#include <cstdio>

namespace ply {

namespace {

unsigned int compile(GLenum type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "plyEngine: shader compile error:\n%s\n", log);
    }
    return shader;
}

}  // namespace

Shader::Shader(const char* vertexSrc, const char* fragmentSrc) {
    unsigned int vs = compile(GL_VERTEX_SHADER, vertexSrc);
    unsigned int fs = compile(GL_FRAGMENT_SHADER, fragmentSrc);

    id_ = glCreateProgram();
    glAttachShader(id_, vs);
    glAttachShader(id_, fs);
    glLinkProgram(id_);
    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok = 0;
    glGetProgramiv(id_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(id_, sizeof(log), nullptr, log);
        std::fprintf(stderr, "plyEngine: program link error:\n%s\n", log);
        valid_ = false;
    } else {
        valid_ = true;
    }
}

Shader::~Shader() {
    if (id_) {
        glDeleteProgram(id_);
    }
}

bool Shader::valid() const { return valid_; }

void Shader::use() const { glUseProgram(id_); }

unsigned int Shader::id() const { return id_; }

void Shader::setInt(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(id_, name), value);
}

void Shader::setMat4(const char* name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(id_, name), 1, GL_FALSE, &m[0][0]);
}

}  // namespace ply
