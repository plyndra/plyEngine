#include "plyEngine/Window.hpp"

#include <SDL3/SDL.h>

#include <cstdio>

namespace ply {

struct Window::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    bool closeRequested = false;
    int width = 0;
    int height = 0;
};

Window::Window(const WindowConfig& cfg) : impl_(new Impl) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "plyEngine: SDL_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_WindowFlags flags = SDL_WINDOW_OPENGL;
    if (cfg.resizable) {
        flags = static_cast<SDL_WindowFlags>(flags | SDL_WINDOW_RESIZABLE);
    }

    impl_->window = SDL_CreateWindow(cfg.title, cfg.width, cfg.height, flags);
    if (!impl_->window) {
        std::fprintf(stderr, "plyEngine: SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }

    impl_->context = SDL_GL_CreateContext(impl_->window);
    if (!impl_->context) {
        std::fprintf(stderr, "plyEngine: SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return;
    }

    SDL_GL_MakeCurrent(impl_->window, impl_->context);
    SDL_GL_SetSwapInterval(1);  // vsync

    impl_->width = cfg.width;
    impl_->height = cfg.height;
}

Window::~Window() {
    if (impl_) {
        if (impl_->context) {
            SDL_GL_DestroyContext(impl_->context);
        }
        if (impl_->window) {
            SDL_DestroyWindow(impl_->window);
        }
        SDL_Quit();
        delete impl_;
    }
}

bool Window::valid() const {
    return impl_ && impl_->window != nullptr && impl_->context != nullptr;
}

bool Window::shouldClose() const {
    return impl_ && impl_->closeRequested;
}

void Window::pollEvents() {
    if (!impl_) {
        return;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            impl_->closeRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            impl_->closeRequested = true;
        }
    }
}

void Window::swapBuffers() {
    if (impl_ && impl_->window) {
        SDL_GL_SwapWindow(impl_->window);
    }
}

int Window::width() const { return impl_ ? impl_->width : 0; }
int Window::height() const { return impl_ ? impl_->height : 0; }

void* Window::glGetProcAddress(const char* name) {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

}  // namespace ply
