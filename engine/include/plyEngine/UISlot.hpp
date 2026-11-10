#pragma once

#include "plyEngine/UIAtlas.hpp"

#include <string>
#include <string_view>

namespace ply {

/// A Slot's visual state (UI_FRAMEWORK.md §5): which atlas region draws the
/// slot's nine-slice frame. The icon and stack count are slot *data*, not
/// state; only the frame region follows the state.
enum class UISlotState { Normal, Selected, Disabled };

/// Stable token for the state, e.g. "Selected". Never renamed — tests and the
/// art lint match the token, not prose (same contract as the atlas error
/// codes).
const char* toString(UISlotState state);

/// The atlas region name that draws a slot's frame in `state`:
/// "slot.normal", "slot.selected", "slot.disabled". The "slot." prefix is a
/// naming convention shared with the fixture atlas; a state whose region is
/// missing from a given atlas is an art-lint failure, found by findRegion
/// returning nullptr there.
std::string_view slotFrameRegionName(UISlotState state);

/// Stack-count label per the slot formatting rules (UI_FRAMEWORK.md §5,
/// "count formatting"):
///   - count <= 1: empty — a lone item (or an empty slot) shows no number;
///   - 2..999: decimal digits;
///   - count >= 1000: "999+" — the largest label the pixel font bakes for,
///     so a huge stack cannot outgrow the slot.
/// Independent of state: a selected or disabled slot formats the same way.
/// Locale-independent by construction (plain ASCII digits).
std::string formatStackCount(int count);

}  // namespace ply
