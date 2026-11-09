#include "plyEngine/Scripting.hpp"

#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Transform.hpp"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace ply {

namespace {

constexpr const char* kEntityMeta = "ply.Entity";

/// Entities crossed into Lua as full userdata holding the packed handle, so a
/// stale handle stays distinguishable instead of being a bare integer index
/// that silently aliases a new entity.
struct LuaEntity {
    Entity entity;
};

/// What the `ply` API closures capture. Deliberately a separate, file-local
/// struct: the closures need the world plus the diagnostics counter, and
/// nothing about ScriptEngine's pimpl should be visible to them.
struct ScriptContext {
    World* world = nullptr;
    size_t* apiCalls = nullptr;
};

ScriptContext* implFromState(lua_State* L) {
    return static_cast<ScriptContext*>(lua_touserdata(L, lua_upvalueindex(1)));
}

LuaEntity* checkEntity(lua_State* L, int index) {
    auto* ref = static_cast<LuaEntity*>(luaL_checkudata(L, index, kEntityMeta));
    return ref;
}

int pushEntity(lua_State* L, Entity entity) {
    auto* ref = static_cast<LuaEntity*>(lua_newuserdatauv(L, sizeof(LuaEntity), 0));
    ref->entity = entity;
    luaL_setmetatable(L, kEntityMeta);
    return 1;
}

int entityEq(lua_State* L) {
    const auto* a = static_cast<LuaEntity*>(luaL_checkudata(L, 1, kEntityMeta));
    const auto* b = static_cast<LuaEntity*>(luaL_checkudata(L, 2, kEntityMeta));
    lua_pushboolean(L, a->entity.index == b->entity.index &&
                           a->entity.generation == b->entity.generation);
    return 1;
}

int entityToString(lua_State* L) {
    const auto* ref = static_cast<LuaEntity*>(luaL_checkudata(L, 1, kEntityMeta));
    lua_pushfstring(L, "Entity(%d:%u)", static_cast<int>(ref->entity.index),
                    static_cast<unsigned>(ref->entity.generation));
    return 1;
}

// --- ply API ---------------------------------------------------------------

int l_spawn(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);

    const lua_Number x = luaL_optnumber(L, 1, 0.0);
    const lua_Number y = luaL_optnumber(L, 2, 0.0);

    // A scripted entity starts as a gameplay object, not a bare id: Transform,
    // GlobalTransform and Velocity. Physics bodies and sprites are C++ side
    // (a script moves things; it does not decide what a thing looks like).
    World& world = *impl->world;
    const Entity entity = world.spawn();
    Transform transform;
    transform.position = glm::vec2{static_cast<float>(x), static_cast<float>(y)};
    world.add<Transform>(entity, transform);
    world.add<GlobalTransform>(entity, GlobalTransform{});
    world.add<Velocity>(entity, Velocity{});
    return pushEntity(L, entity);
}

int l_destroy(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    impl->world->destroy(ref->entity);
    return 0;
}

int l_alive(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    lua_pushboolean(L, impl->world->alive(ref->entity));
    return 1;
}

int l_entityCount(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    lua_pushinteger(L, static_cast<lua_Integer>(impl->world->entityCount()));
    return 1;
}

int l_getPosition(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    const Transform* transform = impl->world->get<Transform>(ref->entity);
    if (!transform) {
        return luaL_error(L, "entity has no Transform");
    }
    lua_pushnumber(L, static_cast<lua_Number>(transform->position.x));
    lua_pushnumber(L, static_cast<lua_Number>(transform->position.y));
    return 2;
}

int l_setPosition(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    const lua_Number x = luaL_checknumber(L, 2);
    const lua_Number y = luaL_checknumber(L, 3);
    Transform* transform = impl->world->get<Transform>(ref->entity);
    if (!transform) {
        return luaL_error(L, "entity has no Transform");
    }
    Transform updated = *transform;
    updated.position = glm::vec2{static_cast<float>(x), static_cast<float>(y)};
    // setTransform (not a raw write) so the hierarchy is marked dirty.
    setTransform(*impl->world, ref->entity, updated);
    return 0;
}

int l_getVelocity(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    const Velocity* velocity = impl->world->get<Velocity>(ref->entity);
    if (!velocity) {
        return luaL_error(L, "entity has no Velocity");
    }
    lua_pushnumber(L, static_cast<lua_Number>(velocity->value.x));
    lua_pushnumber(L, static_cast<lua_Number>(velocity->value.y));
    return 2;
}

int l_setVelocity(lua_State* L) {
    auto* impl = implFromState(L);
    ++(*impl->apiCalls);
    const LuaEntity* ref = checkEntity(L, 1);
    const lua_Number vx = luaL_checknumber(L, 2);
    const lua_Number vy = luaL_checknumber(L, 3);
    Velocity* velocity = impl->world->get<Velocity>(ref->entity);
    if (!velocity) {
        return luaL_error(L, "entity has no Velocity");
    }
    velocity->value = glm::vec2{static_cast<float>(vx), static_cast<float>(vy)};
    return 0;
}

/// Replaces `print` so script output goes through the host's stdout with a
/// prefix, and cannot be used to reach a file.
int l_log(lua_State* L) {
    const int count = lua_gettop(L);
    std::string text = "[lua] ";
    for (int i = 1; i <= count; ++i) {
        if (i > 1) {
            text += "\t";
        }
        size_t length = 0;
        const char* value = luaL_tolstring(L, i, &length);
        text.append(value, length);
        lua_pop(L, 1);
    }
    std::printf("%s\n", text.c_str());
    return 0;
}

int l_panic(lua_State* L) {
    // Reaching the panic handler means the VM is unusable; report and abort
    // rather than continuing with a corrupt state machine.
    const char* message = lua_tostring(L, -1);
    std::fprintf(stderr, "plyEngine: fatal Lua error: %s\n",
                 message ? message : "(no message)");
    return 0;
}

}  // namespace

struct ScriptEngine::Impl {
    explicit Impl(World& w) : world(w) {}

    World& world;
    lua_State* L = nullptr;
    bool ready = false;
    bool hasUpdate = false;
    std::string error;
    size_t apiCalls = 0;
    ScriptContext context;
};

ScriptEngine::ScriptEngine(World& world) : impl_(std::make_unique<Impl>(world)) {}

ScriptEngine::~ScriptEngine() {
    if (impl_ && impl_->L) {
        lua_close(impl_->L);
    }
}

bool ScriptEngine::init() {
    if (!impl_) {
        return false;
    }
    if (impl_->L) {
        return true;
    }

    lua_State* L = luaL_newstate();
    if (!L) {
        impl_->error = "could not create a Lua state";
        return false;
    }
    impl_->L = L;
    lua_atpanic(L, l_panic);

    // Sandbox: base/string/table/math/coroutine/utf8 only. No io, no os, no
    // debug — a gameplay script must not reach the filesystem or the VM.
    luaL_requiref(L, "_G", luaopen_base, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 6);

    // Entity metatable.
    luaL_newmetatable(L, kEntityMeta);
    lua_pushcfunction(L, entityEq);
    lua_setfield(L, -2, "__eq");
    lua_pushcfunction(L, entityToString);
    lua_setfield(L, -2, "__tostring");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);

    // `ply` table. Each function is a closure over `World*`, so scripts never
    // hold a raw pointer they could forge.
    lua_newtable(L);

    struct Entry {
        const char* name;
        lua_CFunction fn;
    };
    const Entry entries[] = {
        {"spawn", l_spawn},
        {"destroy", l_destroy},
        {"alive", l_alive},
        {"entity_count", l_entityCount},
        {"get_position", l_getPosition},
        {"set_position", l_setPosition},
        {"get_velocity", l_getVelocity},
        {"set_velocity", l_setVelocity},
        {"log", l_log},
    };
    impl_->context.world = &impl_->world;
    impl_->context.apiCalls = &impl_->apiCalls;
    for (const Entry& entry : entries) {
        lua_pushlightuserdata(L, &impl_->context);
        lua_pushcclosure(L, entry.fn, 1);
        lua_setfield(L, -2, entry.name);
    }
    lua_setglobal(L, "ply");

    // print -> host stdout
    lua_pushcfunction(L, l_log);
    lua_setglobal(L, "print");

    impl_->error.clear();
    return true;
}

bool ScriptEngine::doString(const std::string& source, const std::string& chunkName) {
    if (!init()) {
        return false;
    }
    lua_State* L = impl_->L;

    if (luaL_loadbuffer(L, source.data(), source.size(), chunkName.c_str()) != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        impl_->error = message ? message : "Lua load error";
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        impl_->error = message ? message : "Lua runtime error";
        lua_pop(L, 1);
        return false;
    }

    impl_->error.clear();
    impl_->ready = true;

    // A script may define or replace update(); re-check each time it runs.
    lua_getglobal(L, "update");
    impl_->hasUpdate = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return true;
}

bool ScriptEngine::doFile(const std::string& path) {
    if (!init()) {
        return false;
    }
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        impl_->error = "cannot open script: " + path;
        return false;
    }
    std::string source;
    char buffer[4096];
    size_t read = 0;
    while ((read = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        source.append(buffer, read);
    }
    std::fclose(file);
    return doString(source, path);
}

bool ScriptEngine::setGlobalEntity(const std::string& name, Entity entity) {
    if (!init()) {
        return false;
    }
    lua_State* L = impl_->L;
    pushEntity(L, entity);
    lua_setglobal(L, name.c_str());
    return true;
}

bool ScriptEngine::hasUpdateCallback() const {
    return impl_ && impl_->hasUpdate;
}

bool ScriptEngine::callUpdate(float dt) {
    if (!impl_ || !impl_->L) {
        return true;
    }
    if (!impl_->hasUpdate) {
        return true;
    }
    lua_State* L = impl_->L;

    lua_getglobal(L, "update");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        impl_->hasUpdate = false;
        return true;
    }
    lua_pushnumber(L, static_cast<lua_Number>(dt));
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        const char* message = lua_tostring(L, -1);
        impl_->error = message ? message : "Lua update() error";
        lua_pop(L, 1);
        return false;
    }
    return true;
}

bool ScriptEngine::ready() const { return impl_ && impl_->ready; }

const std::string& ScriptEngine::error() const {
    static const std::string empty;
    return impl_ ? impl_->error : empty;
}

size_t ScriptEngine::apiCallCount() const { return impl_ ? impl_->apiCalls : 0; }

}  // namespace ply
