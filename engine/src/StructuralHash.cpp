#include "plyEngine/StructuralHash.hpp"

namespace ply {

void StructuralHasher::byte(std::uint8_t value) {
    state_ ^= value;
    state_ *= kPrime;
}

void StructuralHasher::u32(std::uint32_t value) {
    // Explicit little-endian: the hash must not depend on host endianness.
    byte(static_cast<std::uint8_t>(value & 0xffu));
    byte(static_cast<std::uint8_t>((value >> 8) & 0xffu));
    byte(static_cast<std::uint8_t>((value >> 16) & 0xffu));
    byte(static_cast<std::uint8_t>((value >> 24) & 0xffu));
}

void StructuralHasher::i32(std::int32_t value) {
    u32(static_cast<std::uint32_t>(value));  // two's complement, same bytes
}

void StructuralHasher::u64(std::uint64_t value) {
    u32(static_cast<std::uint32_t>(value & 0xffffffffu));
    u32(static_cast<std::uint32_t>((value >> 32) & 0xffffffffu));
}

void StructuralHasher::bytes(const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    for (std::size_t i = 0; i < size; ++i) byte(bytes[i]);
}

std::uint64_t fnv1a64(const void* data, std::size_t size) {
    StructuralHasher hasher;
    hasher.bytes(data, size);
    return hasher.value();
}

}  // namespace ply
