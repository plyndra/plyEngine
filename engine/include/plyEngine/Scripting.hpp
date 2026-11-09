#pragma once

#include "plyEngine/World.hpp"

#include <memory>
#include <string>

namespace ply {

/// Lua scripting for gameplay logic (M8).
///
/// Deliberately small and data-oriented: the script layer can create and
/// destroy entities and read/write the components that gameplay scripts
/// actually touch (Transform position, Velocity). Rendering, assets and the
/// fixed-step loop stay in C++ — a script moves things, it does not own the
/// frame.
///
/// The script API (global `ply` table):
///
///     e = ply.spawn(x, y)        -- Transform + GlobalTransform + Velocity
///     ply.destroy(e)
///     ply.alive(e)               -- false for stale handles (generation checked)
///     ply.entity_count()
///     x, y   = ply.get_position(e)     ply.set_position(e, x, y)
///     vx, vy = ply.get_velocity(e)     ply.set_velocity(e, vx, vy)
///     ply.log(...)               -- also `print`
///
/// Define a global `update(dt)` in the script and the host can call it once
/// per fixed step (callUpdate). Rendering, assets and physics integration stay
/// in C++: a script moves things, it does not own the frame.
///
/// Sandboxing: only `base`, `string`, `table`, `math`, `coroutine` and `utf8`
/// are opened. `io`, `os` and `debug` are absent, so a game script cannot
/// touch the filesystem, spawn processes or tamper with the VM. `print` is
/// redirected to `ply.log`, which reaches the host's stdout.
///
/// Thread affinity: a ScriptEngine and the World it borrows must be used and
/// destroyed on the same thread, inside the fixed step.
class ScriptEngine {
public:
    /// Borrows `world`; the world must outlive this object.
    explicit ScriptEngine(World& world);
    ~ScriptEngine();

    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;

    /// Creates the Lua state and installs the `ply` API. Returns false and
    /// sets `error()` if Lua could not be initialised.
    bool init();

    /// Loads and runs a file. Returns false and sets `error()` on a syntax or
    /// runtime error (the message includes the script line).
    bool doFile(const std::string& path);

    /// Runs a chunk of source. Same error contract as doFile().
    bool doString(const std::string& source, const std::string& chunkName = "chunk");

    /// Publishes an entity handle as a Lua global, so a script can register
    /// entities the C++ side created (`engine.setGlobalEntity("drone", e)`
    /// followed by a chunk that calls its own `add_drone(drone, phase)`).
    bool setGlobalEntity(const std::string& name, Entity entity);

    /// True when the script defined a global `update(dt)` function; callUpdate
    /// calls it once per fixed step when present.
    bool hasUpdateCallback() const;

    /// Calls the script's `update(dt)`. Returns false and sets `error()` if the
    /// callback raised. A script without `update` is a no-op that returns true.
    bool callUpdate(float dt);

    /// True once any script has run successfully.
    bool ready() const;

    /// Last error message (empty when the last operation succeeded).
    const std::string& error() const;

    /// Number of times `ply` API calls have been made (diagnostics/tests).
    size_t apiCallCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace ply
