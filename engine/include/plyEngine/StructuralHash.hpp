#pragma once

#include <cstddef>
#include <cstdint>

namespace ply {

/// FNV-1a 64 over explicit little-endian bytes.
///
/// A structural hash (ADR 0014) is evidence that two runs produced the same
/// state, so it must not depend on the host: `std::hash` is stable within one
/// build only, and hashing a struct's raw bytes would fold padding and host
/// endianness into the value. Everything goes through this class as
/// little-endian integers of a declared width, in a declared order.
class StructuralHasher {
public:
    static constexpr std::uint64_t kOffsetBasis = 0xcbf29ce484222325ull;
    static constexpr std::uint64_t kPrime = 0x100000001b3ull;

    StructuralHasher() = default;

    void byte(std::uint8_t value);
    void u32(std::uint32_t value);
    void i32(std::int32_t value);
    void u64(std::uint64_t value);
    void bytes(const void* data, std::size_t size);

    /// Hash so far; does not disturb the running state.
    std::uint64_t value() const { return state_; }

    void reset() { state_ = kOffsetBasis; }

private:
    std::uint64_t state_ = kOffsetBasis;
};

/// One-shot FNV-1a 64 over a byte range. Same value as hashing the bytes
/// through a StructuralHasher.
std::uint64_t fnv1a64(const void* data, std::size_t size);

}  // namespace ply
