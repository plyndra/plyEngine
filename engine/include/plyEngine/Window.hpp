#pragma once

namespace ply {

struct WindowConfig {
    const char* title = "plyEngine";
    int width = 1280;
    int height = 720;
    bool resizable = true;
};

/// A native window with an OpenGL 3.3 core context (SDL3-backed).
/// This is the start of the Foundation layer.
class Window {
public:
    explicit Window(const WindowConfig& cfg);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    /// True if the window and GL context were created successfully.
    bool valid() const;

    /// True once the user closes the window or presses Escape.
    bool shouldClose() const;

    /// Pump the platform event loop.
    void pollEvents();

    /// Swap the GL back buffer to the screen.
    void swapBuffers();

    int width() const;
    int height() const;

    /// Platform GL function lookup, for a GL loader such as glad.
    static void* glGetProcAddress(const char* name);

private:
    struct Impl;
    Impl* impl_;
};

}  // namespace ply
