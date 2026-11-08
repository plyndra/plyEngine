#include "plyEngine/Window.hpp"

#include <glad/gl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";

const char* kFragmentShader = R"(#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, vTexCoord);
})";

GLuint compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader compile error: %s\n", log);
    }
    return shader;
}

GLuint linkProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "program link error: %s\n", log);
    }
    return prog;
}

}  // namespace

int main(int argc, char** argv) {
    int maxFrames = 0;  // 0 = run until the window closes
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);
        }
    }

    ply::WindowConfig cfg;
    cfg.title = "plyEngine - hello";
    cfg.width = 1280;
    cfg.height = 720;

    ply::Window window(cfg);
    if (!window.valid()) {
        std::fprintf(stderr, "failed to create window\n");
        return 1;
    }

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(ply::Window::glGetProcAddress))) {
        std::fprintf(stderr, "failed to load OpenGL via glad\n");
        return 1;
    }

    std::printf("OpenGL %s\n",
                reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    // Quad geometry: position + texcoord interleaved.
    const float vertices[] = {
        // x      y       u     v
        -0.5f, -0.5f,   0.0f, 0.0f,
         0.5f, -0.5f,   1.0f, 0.0f,
         0.5f,  0.5f,   1.0f, 1.0f,
        -0.5f,  0.5f,   0.0f, 1.0f,
    };
    const unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    GLuint vao = 0, vbo = 0, ebo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Checkerboard texture (procedural; no image loader needed until M4).
    const int texSize = 8;
    unsigned char pixels[texSize * texSize * 4];
    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            const bool white = ((x + y) % 2) == 0;
            const int i = (y * texSize + x) * 4;
            pixels[i + 0] = white ? 255 : 32;
            pixels[i + 1] = white ? 255 : 32;
            pixels[i + 2] = white ? 255 : 32;
            pixels[i + 3] = 255;
        }
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texSize, texSize, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLuint program = linkProgram(kVertexShader, kFragmentShader);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

    glClearColor(0.10f, 0.10f, 0.15f, 1.0f);

    int frames = 0;
    while (!window.shouldClose()) {
        window.pollEvents();
        glClear(GL_COLOR_BUFFER_BIT);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        window.swapBuffers();

        if (maxFrames > 0 && ++frames >= maxFrames) {
            break;
        }
    }

    std::printf("ply_hello rendered %d frames\n", frames);

    glDeleteTextures(1, &texture);
    glDeleteProgram(program);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    return 0;
}
