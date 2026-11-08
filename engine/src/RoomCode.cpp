#include "plyEngine/RoomCode.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ply::companion {

namespace {
// No I, O, 0 or 1: they are misread when a code is spoken or typed.
constexpr const char kAlphabet[] = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ";
}  // namespace

const char* RoomCode::alphabet() { return kAlphabet; }

int RoomCode::alphabetSize() {
    return static_cast<int>(sizeof(kAlphabet) - 1);
}

RoomCode::RoomCode(std::string code) {
    code_ = normalizeRoomCode(code);
    valid_ = !code_.empty();
}

RoomCode RoomCode::generate() {
    std::random_device device;
    std::mt19937 rng(device());
    return generate(rng);
}

bool RoomCode::matches(const std::string& candidate) const {
    if (!valid_) {
        return false;
    }
    return code_ == normalizeRoomCode(candidate);
}

std::string normalizeRoomCode(const std::string& input) {
    std::string out;
    out.reserve(input.size());

    for (char c : input) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (std::isspace(uc) || c == '-' || c == '_') {
            continue;  // tolerate "AB CD", "AB-CD", "AB_CD"
        }
        const char upper = static_cast<char>(std::toupper(uc));
        if (std::strchr(kAlphabet, upper) == nullptr) {
            return {};  // character outside the room-code alphabet
        }
        out.push_back(upper);
    }

    if (static_cast<int>(out.size()) != RoomCode::kLength) {
        return {};
    }
    return out;
}

}  // namespace ply::companion
