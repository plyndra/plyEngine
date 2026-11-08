#pragma once

#include <cstddef>
#include <cstdint>
#include <new>
#include <utility>

namespace ply {

/// Stable per-type identifier used in archetype bitmasks.
using ComponentTypeId = uint32_t;

namespace detail {

/// Type-erased operations an archetype column needs to relocate and destroy
/// components. This lets components be any movable type (e.g. one holding a
/// std::vector), not just trivially copyable ones.
struct ComponentMeta {
    size_t size = 1;
    size_t align = 1;
    /// Move-constructs src into dst (which must hold no live object), then
    /// destroys src.
    void (*relocate)(void* dst, void* src) = nullptr;
    /// Destroys the object at obj.
    void (*destroy)(void* obj) = nullptr;
};

template <typename T>
ComponentMeta makeComponentMeta() {
    ComponentMeta meta;
    meta.size = sizeof(T);
    meta.align = alignof(T);
    meta.relocate = [](void* dst, void* src) {
        T* from = static_cast<T*>(src);
        new (dst) T(std::move(*from));
        from->~T();
    };
    meta.destroy = [](void* obj) { static_cast<T*>(obj)->~T(); };
    return meta;
}

/// Registers a component type on first use and returns its stable id.
ComponentTypeId registerComponent(const ComponentMeta& meta);

/// Metadata for an already-registered component type.
const ComponentMeta* componentMeta(ComponentTypeId id);

/// Number of component types registered so far (diagnostics/tests).
uint32_t registeredComponentCount();

}  // namespace detail

/// The stable id for component type T. Assigned on first use.
template <typename T>
ComponentTypeId componentTypeId() {
    static const ComponentTypeId id =
        detail::registerComponent(detail::makeComponentMeta<T>());
    return id;
}

}  // namespace ply
