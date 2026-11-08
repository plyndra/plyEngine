#include "plyEngine/CompanionProtocol.hpp"

#include <algorithm>
#include <cmath>

namespace ply::companion {

namespace {

float clampAxis(double value) {
    if (value < -1.0) {
        return -1.0f;
    }
    if (value > 1.0) {
        return 1.0f;
    }
    return static_cast<float>(value);
}

}  // namespace

std::string encodeMessage(const std::string& type, uint32_t seq,
                          const nlohmann::json& payload) {
    nlohmann::json frame = nlohmann::json::object();
    frame["v"] = kProtocolVersion;
    frame["type"] = type;
    frame["seq"] = seq;
    frame["payload"] = payload;
    return frame.dump();
}

bool decodeMessage(const std::string& text, Message& out, std::string& error) {
    const nlohmann::json frame = nlohmann::json::parse(text, nullptr, false);
    if (frame.is_discarded()) {
        error = "malformed JSON";
        return false;
    }
    if (!frame.is_object()) {
        error = "frame is not a JSON object";
        return false;
    }
    if (!frame.contains("v") || !frame["v"].is_number_integer()) {
        error = "missing numeric 'v'";
        return false;
    }
    const int version = frame["v"].get<int>();
    if (version != kProtocolVersion) {
        error = "protocol version mismatch: client " + std::to_string(version) +
                ", server " + std::to_string(kProtocolVersion);
        return false;
    }
    if (!frame.contains("type") || !frame["type"].is_string()) {
        error = "missing string 'type'";
        return false;
    }

    out.version = version;
    out.type = frame["type"].get<std::string>();
    out.seq = 0;
    if (frame.contains("seq") && frame["seq"].is_number_unsigned()) {
        out.seq = frame["seq"].get<uint32_t>();
    } else if (frame.contains("seq") && frame["seq"].is_number_integer()) {
        const int64_t raw = frame["seq"].get<int64_t>();
        out.seq = raw < 0 ? 0u : static_cast<uint32_t>(raw);
    }
    out.payload = frame.contains("payload") && frame["payload"].is_object()
                      ? frame["payload"]
                      : nlohmann::json::object();
    return true;
}

nlohmann::json encodeController(const ControllerState& state) {
    nlohmann::json payload = nlohmann::json::object();
    payload["x"] = clampAxis(state.x);
    payload["y"] = clampAxis(state.y);
    payload["buttons"] = state.buttons;
    return payload;
}

bool decodeController(const nlohmann::json& payload, ControllerState& out,
                      std::string& error) {
    if (!payload.is_object()) {
        error = "input payload is not an object";
        return false;
    }
    ControllerState state;
    if (payload.contains("x") && payload["x"].is_number()) {
        state.x = clampAxis(payload["x"].get<double>());
    }
    if (payload.contains("y") && payload["y"].is_number()) {
        state.y = clampAxis(payload["y"].get<double>());
    }
    if (payload.contains("buttons")) {
        if (!payload["buttons"].is_number_unsigned() &&
            !payload["buttons"].is_number_integer()) {
            error = "'buttons' is not an integer";
            return false;
        }
        const int64_t raw = payload["buttons"].get<int64_t>();
        state.buttons = raw < 0 ? 0u : static_cast<uint32_t>(raw);
    }
    out = state;
    return true;
}

}  // namespace ply::companion
