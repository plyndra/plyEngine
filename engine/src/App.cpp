#include "plyEngine/App.hpp"

#include <glad/gl.h>

#include <chrono>
#include <cstdio>
#include <memory>
#include <utility>

namespace ply {

struct App::Impl {
    Config config;
    std::unique_ptr<Window> window;
    Input input;
    OrthoCamera camera;
    World world;
    FixedTimestep timestep;

    StartFn startFn;
    UpdateFn updateFn;
    RenderFn renderFn;

    bool started = false;
    bool quit = false;
    bool glReady = false;
    const char* error = "";
    int frames = 0;
    int steps = 0;

    // viewport resize tracking (not part of the public API)
    int lastWidth = 0;
    int lastHeight = 0;

    explicit Impl(const Config& cfg)
        : config(cfg),
          timestep(cfg.fixedStep, cfg.maxSubsteps, cfg.maxFrameTime) {}
};

App::App() : App(Config{}) {}

App::App(Config config) : impl_(new Impl(config)) {
    impl_->window = std::make_unique<Window>(config.window);
    if (!impl_->window->valid()) {
        impl_->error = "window or GL context creation failed";
        return;
    }

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(&Window::glGetProcAddress))) {
        impl_->error = "failed to load OpenGL via glad";
        return;
    }

    impl_->glReady = true;
    impl_->lastWidth = impl_->window->width();
    impl_->lastHeight = impl_->window->height();
    impl_->camera.setViewport(impl_->lastWidth, impl_->lastHeight);
}

App::~App() { delete impl_; }

bool App::valid() const { return impl_ && impl_->glReady; }

const char* App::error() const { return impl_ ? impl_->error : "no impl"; }

void App::onStart(StartFn fn) { impl_->startFn = std::move(fn); }
void App::onUpdate(UpdateFn fn) { impl_->updateFn = std::move(fn); }
void App::onRender(RenderFn fn) { impl_->renderFn = std::move(fn); }

void App::quit() { impl_->quit = true; }

Window& App::window() { return *impl_->window; }
Input& App::input() { return impl_->input; }
OrthoCamera& App::camera() { return impl_->camera; }
World& App::world() { return impl_->world; }

int App::framesRendered() const { return impl_->frames; }
int App::stepsSimulated() const { return impl_->steps; }
const FixedTimestep& App::timestep() const { return impl_->timestep; }

namespace {
}  // namespace

// Keeps the camera viewport in sync with the window (handles resizes).
void App::syncViewport() {
    const int w = impl_->window->width();
    const int h = impl_->window->height();
    if (w != impl_->lastWidth || h != impl_->lastHeight) {
        impl_->lastWidth = w;
        impl_->lastHeight = h;
        impl_->camera.setViewport(w, h);
        glViewport(0, 0, w, h);
    }
}

int App::run() {
    if (!valid()) {
        std::fprintf(stderr, "plyEngine: App invalid: %s\n", error());
        return 1;
    }

    glViewport(0, 0, impl_->lastWidth, impl_->lastHeight);

    if (!impl_->started) {
        impl_->started = true;
        if (impl_->startFn) {
            impl_->startFn(*this);
        }
    }

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();

    while (!impl_->quit && !impl_->input.quitRequested()) {
        impl_->input.update();
        syncViewport();

        const auto now = Clock::now();
        double frameTime =
            std::chrono::duration<double>(now - previous).count();
        previous = now;

        const FixedTimestep::Advance advance = impl_->timestep.advance(frameTime);
        for (int i = 0; i < advance.steps; ++i) {
            if (impl_->updateFn) {
                impl_->updateFn(*this, impl_->timestep.step());
            }
            ++impl_->steps;
        }

        if (impl_->renderFn) {
            impl_->renderFn(*this, advance.alpha);
        }
        impl_->window->swapBuffers();
        ++impl_->frames;
    }
    return 0;
}

int App::runFrames(int frames, double frameDelta) {
    if (!valid()) {
        std::fprintf(stderr, "plyEngine: App invalid: %s\n", error());
        return 1;
    }

    glViewport(0, 0, impl_->lastWidth, impl_->lastHeight);

    if (!impl_->started) {
        impl_->started = true;
        if (impl_->startFn) {
            impl_->startFn(*this);
        }
    }

    for (int f = 0; f < frames; ++f) {
        impl_->input.update();
        syncViewport();

        const FixedTimestep::Advance advance = impl_->timestep.advance(frameDelta);
        for (int i = 0; i < advance.steps; ++i) {
            if (impl_->updateFn) {
                impl_->updateFn(*this, impl_->timestep.step());
            }
            ++impl_->steps;
        }

        if (impl_->renderFn) {
            impl_->renderFn(*this, advance.alpha);
        }
        impl_->window->swapBuffers();
        ++impl_->frames;
    }
    return 0;
}

}  // namespace ply
