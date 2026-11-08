#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace ply::companion {

/// Wire protocol version. Bumped whenever the message shape changes; the
/// server rejects clients that disagree instead of misinterpreting them.
inline constexpr int kProtocolVersion = 1;

/// Canonical per-player controller state. Phones, keyboard and gamepads all
/// reduce to this, so a game never parses protocol details.
struct ControllerState {
    float x = 0.0f;  ///< -1..1 (dpad left/right)
    float y = 0.0f;  ///< -1..1 (dpad down/up)
    uint32_t buttons = 0;

    friend bool operator==(const ControllerState&, const ControllerState&) = default;
};

/// Button bits carried in ControllerState::buttons.
enum Button : uint32_t {
    kButtonA = 1u << 0,
    kButtonB = 1u << 1,
    kButtonStart = 1u << 2,
};

/// A decoded protocol frame.
struct Message {
    int version = kProtocolVersion;
    std::string type;
    uint32_t seq = 0;
    nlohmann::json payload = nlohmann::json::object();
};

// --- Message type names ------------------------------------------------------
// server -> client
inline constexpr const char* kTypeHello = "hello";      // version, room, slots
inline constexpr const char* kTypeJoined = "joined";    // slot, colour, labels
inline constexpr const char* kTypeRejected = "rejected";  // reason
inline constexpr const char* kTypeRoster = "roster";    // slots[]
inline constexpr const char* kTypePong = "pong";
// client -> server
inline constexpr const char* kTypeJoin = "join";  // room, name
inline constexpr const char* kTypeInput = "input";  // x, y, buttons
inline constexpr const char* kTypePing = "ping";

/// Serializes a frame to JSON text.
std::string encodeMessage(const std::string& type, uint32_t seq,
                          const nlohmann::json& payload);

/// Parses a frame. Rejects malformed JSON, a non-object frame, a missing or
/// non-numeric version, a missing type, and a protocol version mismatch.
bool decodeMessage(const std::string& text, Message& out, std::string& error);

/// Controller state -> JSON payload / back. Out-of-range axes are clamped.
nlohmann::json encodeController(const ControllerState& state);
bool decodeController(const nlohmann::json& payload, ControllerState& out,
                      std::string& error);

}  // namespace ply::companion
