#pragma once

#include <cstdint>
#include <random>
#include <string>

namespace ply::companion {

/// Human-friendly room codes.
///
/// The alphabet omits characters that are easy to confuse when read off a
/// screen and typed on a phone: I, O, 0 and 1. Codes are compared
/// case-insensitively.
class RoomCode {
public:
    static constexpr int kLength = 4;

    /// Alphabet actually used (30 characters: 2-9 and most letters).
    static const char* alphabet();
    static int alphabetSize();

    RoomCode() = default;
    explicit RoomCode(std::string code);

    /// Generates a random code using the given engine.
    template <typename Rng>
    static RoomCode generate(Rng& rng) {
        std::uniform_int_distribution<int> dist(0, alphabetSize() - 1);
        std::string code;
        code.reserve(kLength);
        for (int i = 0; i < kLength; ++i) {
            code.push_back(alphabet()[dist(rng)]);
        }
        return RoomCode(std::move(code));
    }

    static RoomCode generate();

    const std::string& value() const { return code_; }
    bool valid() const { return valid_; }

    /// Case-insensitive comparison.
    bool matches(const std::string& candidate) const;

private:
    std::string code_;
    bool valid_ = false;
};

/// Normalizes user input: trims whitespace, upper-cases, drops separators.
/// Returns an empty string when the result is not a valid code.
std::string normalizeRoomCode(const std::string& input);

}  // namespace ply::companion
