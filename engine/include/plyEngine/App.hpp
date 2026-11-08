#pragma once

#include "plyEngine/FixedTimestep.hpp"
#include "plyEngine/Input.hpp"
#include "plyEngine/OrthoCamera.hpp"
#include "plyEngine/Window.hpp"
#include "plyEngine/World.hpp"

#include <functional>

namespace ply {

/// Owns the window, GL context, input, camera, world and the fixed-timestep
/// game loop. Games fill in three callbacks and call run().
///
/// The raw pieces stay public via accessors, so a game that wants to drive its
/// own loop is not locked out.
class App {
public:
    struct Config {
        WindowConfig window;
        double fixedStep = 1.0 / 60.0;
        int maxSubsteps = 5;
        double maxFrameTime = 0.25;
    };

    using StartFn = std::function<void(App&)>;
    /// Called once per fixed simulation step, with the step size in seconds.
    using UpdateFn = std::function<void(App&, double)>;
    /// Called once per rendered frame, with the interpolation alpha in [0,1).
    using RenderFn = std::function<void(App&, double)>;

    App();
    explicit App(Config config);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// True if the window, GL context and GL loader all initialized.
    bool valid() const;
    const char* error() const;

    void onStart(StartFn fn);
    void onUpdate(UpdateFn fn);
    void onRender(RenderFn fn);

    /// Runs until the window closes. Returns the process exit code.
    int run();

    /// Runs exactly `frames` frames with a fixed simulated delta, without
    /// waiting on vsync. Used for deterministic headless/CI runs.
    int runFrames(int frames, double frameDelta = 1.0 / 60.0);

    void quit();

    Window& window();
    Input& input();
    OrthoCamera& camera();
    World& world();

    int framesRendered() const;
    int stepsSimulated() const;
    const FixedTimestep& timestep() const;

private:
    void syncViewport();

    struct Impl;
    Impl* impl_;
};

}  // namespace ply
