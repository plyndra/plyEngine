// Headless tests for the Lua scripting layer (no GL, no window).
//
// Asserts the contract scripts depend on: entities created from Lua really
// exist in the World, positions/velocities round-trip, the update hook runs
// with the step size, errors are reported instead of crashing, and the sandbox
// has no io/os/debug access.
//
// Contracts that live inside Lua are checked with `assert(...)` in the script:
// a failed assertion makes doString() return false, so the same chunk proves
// both sides.

#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Scripting.hpp"
#include "plyEngine/Transform.hpp"
#include "plyEngine/World.hpp"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

/// Runs a chunk that should succeed, reporting the engine's error if it does not.
void expectOk(ply::ScriptEngine& script, const char* source, const char* what) {
    if (!script.doString(source, what)) {
        std::fprintf(stderr, "  FAIL: %s -> %s\n", what, script.error().c_str());
        ++failures;
    }
}

}  // namespace

int main() {
    // --- init and trivial chunks ------------------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);
        check(script.init(), "init() succeeds");
        check(!script.ready(), "not ready before any script runs");
        check(script.doString("local x = 1", "trivial"), "trivial chunk runs");
        check(script.ready(), "ready() after a successful chunk");
        check(script.error().empty(), "no error after success");
        check(!script.hasUpdateCallback(), "no update() defined yet");
        check(script.callUpdate(0.016f), "callUpdate without a hook is a no-op");
    }

    // --- entities created from Lua are real -------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);

        expectOk(script, R"(
            e = ply.spawn()
            ply.set_position(e, 120, 240)
            ply.set_velocity(e, -30, 5)
            assert(ply.alive(e), "spawned entity must be alive")
            assert(ply.entity_count() == 1, "one entity expected, got " .. ply.entity_count())

            local x, y = ply.get_position(e)
            assert(x == 120 and y == 240, "position must round-trip, got " .. x .. "," .. y)
            local vx, vy = ply.get_velocity(e)
            assert(vx == -30 and vy == 5, "velocity must round-trip, got " .. vx .. "," .. vy)
        )", "spawn+set");

        check(world.entityCount() == 1, "Lua spawn created exactly one entity");

        // The World must hold what Lua wrote: iterate the real components.
        bool found = false;
        world.each<ply::Transform>([&](ply::Entity, ply::Transform& transform) {
            if (transform.position.x == 120.0f && transform.position.y == 240.0f) {
                found = true;
            }
        });
        check(found, "Lua-written position is visible in the World");

        bool velocityOk = false;
        world.each<ply::Velocity>([&](ply::Entity, ply::Velocity& velocity) {
            if (velocity.value.x == -30.0f && velocity.value.y == 5.0f) {
                velocityOk = true;
            }
        });
        check(velocityOk, "Lua-written velocity is visible in the World");
        check(script.apiCallCount() >= 5, "API calls were counted");
    }

    // --- destroy from Lua removes the entity -------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);
        expectOk(script, R"(
            e = ply.spawn()
            ply.destroy(e)
            assert(ply.alive(e) == false, "destroyed entity must not be alive")
            assert(ply.entity_count() == 0, "world must be empty")
        )", "destroy");
        check(world.entityCount() == 0, "destroy from Lua emptied the World");
    }

    // --- update(dt) hook ---------------------------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);
        expectOk(script, R"(
            e = ply.spawn()
            ply.set_position(e, 0, 0)
            ticks = 0
            last_dt = 0
            function update(dt)
                ticks = ticks + 1
                last_dt = dt
                local x, y = ply.get_position(e)
                ply.set_position(e, x + dt * 100, y)
            end
        )", "update hook");

        check(script.hasUpdateCallback(), "update() is detected");

        const float step = 0.05f;
        for (int i = 0; i < 10; ++i) {
            check(script.callUpdate(step), "callUpdate succeeds");
        }

        float x = -1.0f;
        world.each<ply::Transform>([&](ply::Entity, ply::Transform& transform) {
            x = transform.position.x;
        });
        // 10 steps of dt*100 at 0.05 -> 50 units, within float tolerance.
        check(std::fabs(x - 50.0f) < 0.01f, "update() moved the entity by dt");

        expectOk(script, "assert(ticks == 10, 'ticks=' .. ticks)\n"
                         "assert(math.abs(last_dt - 0.05) < 1e-6, 'dt not forwarded')",
                 "hook bookkeeping");
    }

    // --- errors are reported, not fatal ------------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);

        check(!script.doString("this is not lua", "syntax"), "syntax error is rejected");
        check(!script.error().empty(), "syntax error sets a message");

        check(!script.doString("error('boom')", "runtime"), "runtime error is rejected");
        check(script.error().find("boom") != std::string::npos,
              "runtime error message is propagated");

        const std::string missing = "/nonexistent/ply_script.lua";
        check(!script.doFile(missing), "missing script file is rejected");
        check(script.error().find("cannot open") != std::string::npos,
              "missing file reports why");

        // A type error from the API surfaces as a Lua error, not a crash.
        check(!script.doString("ply.set_position(42, 1, 2)", "bad argument"),
              "wrong argument type is rejected");

        // The engine keeps working after errors.
        check(script.doString("ok = true", "after errors"), "recovers after errors");
    }

    // --- sandbox -----------------------------------------------------------
    {
        ply::World world;
        ply::ScriptEngine script(world);
        expectOk(script, R"(
            assert(io == nil, "io must not be exposed")
            assert(os == nil, "os must not be exposed")
            assert(debug == nil, "debug must not be exposed")
            assert(load == nil or type(load) == "function", "sandbox sanity")
            assert(type(ply.spawn) == "function", "ply API is present")
        )", "sandbox");
    }

    // --- entities carry generation, so stale handles are detectable --------
    {
        ply::World world;
        ply::ScriptEngine script(world);
        expectOk(script, R"(
            first = ply.spawn()
            ply.destroy(first)
            second = ply.spawn()          -- may reuse the index
            assert(first ~= second, "a recycled index must not equal the old handle")
            assert(ply.alive(first) == false, "stale handle must not be alive")
            assert(ply.alive(second), "new handle must be alive")
        )", "generations");
    }

    if (failures == 0) {
        std::printf("scripting test: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "scripting test: %d failure(s)\n", failures);
    return 1;
}
