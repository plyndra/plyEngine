#pragma once

#include <cstdint>
#include <functional>

namespace ply {

/// A handle to an entity. The index addresses a slot; the generation makes
/// stale handles detectable, so a handle to a destroyed entity can never
/// silently refer to a recycled one.
struct Entity {
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;

    constexpr bool valid() const { return index != kInvalidIndex; }

    friend constexpr bool operator==(const Entity&, const Entity&) = default;

    /// Packed representation, for logging and hash maps.
    constexpr uint64_t bits() const {
        return (static_cast<uint64_t>(index) << 32) | generation;
    }
};

}  // namespace ply
