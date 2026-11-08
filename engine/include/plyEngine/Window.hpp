#pragma once

namespace ply {

struct WindowConfig {
    const char* title = "plyEngine";
    int width = 1280;
    int height = 720;
    bool resizable = true;
};

/// A native window with an OpenGL 3.3 core context (SDL3-backed).
class Window {
public:
    explicit Window(const WindowConfig& cfg);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool valid() const;
    void swapBuffers();

    int width() const;   // live query
    int height() const;  // live query

    /// Platform GL function lookup, for a GL loader such as glad.
    static void* glGetProcAddress(const char* name);

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace ply
