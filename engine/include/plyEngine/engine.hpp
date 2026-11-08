#pragma once

#include <string_view>

namespace ply {

/// Engine version, bumped at each milestone (M0).
inline constexpr std::string_view kEngineVersion = "0.1.0";

/// Initializes engine subsystems. Returns 0 on success.
int init();

/// Shuts the engine down cleanly.
void shutdown();

}  // namespace ply
